# Cycle V2 Canvas Performance Instrumentation

## Status

Complete.

## Problem

The node canvas has become perceptibly laggy, but current diagnostics only count
render-invalidation coalescing. They do not show which ordinary editing triggers
dominate a session, how expensive their message-thread handling is, how long a
repaint waits after each trigger, or how much time JUCE and OpenGL rendering
consume. Optimizations therefore lack a repeatable before/after baseline.

## Authoritative Boundaries

- `NodeCanvas` remains the JUCE event, paint, timer, and OpenGL lifecycle owner.
- `RenderInvalidationAccumulator` remains the authoritative repaint-coalescing
  boundary.
- `NodeCanvasPresentation`, `NodeCanvasInteraction`, `NodeCanvasAuthoring`, and
  `NodeCanvasEditorCoordinator` retain all rendering, interaction, graph-edit,
  and editor behavior unchanged.
- Cycle V2 automation remains the external inspection boundary used by focused
  runtime fixtures.

The performance collector observes these boundaries only. It does not reproduce
rendering or editing behavior, inspect node kinds, or influence scheduling.

## Measurement Contract

A resettable observation window reports:

- elapsed wall time;
- invocation count and handler-duration distribution for typical trigger
  families: hover, pointer gesture, viewport, graph edit, parameter edit,
  preview/runtime publication, timer, layout/lifecycle, and other;
- repaint-request count by trigger family, including the session distribution;
- repaint dispatch count, coalescing ratio, and request-to-JUCE-paint latency by
  trigger family;
- JUCE paint and OpenGL render duration distributions;
- invalidation requests, scheduled/completed flushes, and category dispatches.

Durations use fixed histogram buckets and aggregate count/sum/maximum values.
Recording must not allocate, log, repaint, or change application behavior.
Metrics are descriptive rather than pass/fail timing assertions because host,
debug/release mode, window size, graph content, and GPU materially affect them.

The Trimesh/Spy extension additionally reports:

- native generic parameter (including Delay beats) and Trimesh vertex update
  and gesture-commit duration distributions at the
  command-service boundary (automation wrappers are not an adequate proxy for
  child-component pointer events);
- hover resolution duration plus changed/unchanged presentation-state and
  expanded-editor-occlusion counts;
- asynchronous preview queue delay, worker duration, audio/grid traversal,
  preview extraction, publication delay, and request-to-publication latency;
- published, superseded-before-start, stale/cancelled, and no-work request
  counts so “fast” timings cannot hide discarded work.

`GraphPresentationModel` is the authoritative async lifecycle owner and owns
its collector. `GraphAudioExecutor` and `GraphPreviewExecutor` remain the
authoritative algorithms; timers surround their existing calls without
reimplementing either path. `NodeEditorCommandService` reports completed
Trimesh operations through a narrow observer and retains all edit behavior.

## Automation

Add commands to reset and inspect the canvas performance window. Inspection
returns structured JSON suitable for saving with a fixture report. A focused
fixture should reset metrics, exercise representative hover, drag, viewport,
graph-edit, and parameter-edit operations, allow pending paints to settle, and
capture the window.

## Completion Criteria

- Unit tests prove histogram aggregation, percentile reporting, trigger
  distribution, repaint attribution, coalescing, and reset behavior.
- The canvas records trigger handling, repaint requests, JUCE paints, and OpenGL
  renders without moving domain behavior into the collector.
- Automation can reset and inspect a measurement window.
- A representative runtime fixture produces a baseline report, and the audit
  documents the dominant trigger and frame-cost findings from that run.
- Cycle V2 tests and standalone build pass; style and diff checks are clean.

## Stable End State

The collector remains a small UI-infrastructure component. Optimization work
uses the same automation fixture and JSON schema for before/after comparison.
No deletion target is introduced; the existing invalidation diagnostics remain
owned by their accumulator and are composed into the exported snapshot.

## Baseline Audit

Captured on 2026-08-26 on macOS/Apple Silicon with the `standalone-debug`
build and `scripts/fixtures/cycle-v2-agent-canvas-performance.json` through the
session transport. The window lasted 1.46 seconds and included hover, pointer
drag, viewport, graph-edit, parameter-edit, timer, repaint, JUCE paint, and
OpenGL activity.

- Timer callbacks were 67.7% of recorded trigger invocations, but averaged only
  0.33 ms. Hover averaged 0.10 ms and viewport handling 0.002 ms. These are not
  the first-order lag source in this sample.
- Pointer-gesture handling averaged 5.55 ms and reached 15.55 ms. The expensive
  drag event is close to a 60 Hz frame budget before repaint work begins.
- Graph-edit handling averaged 24.28 ms and reached 32.88 ms. The parameter
  edit took 19.43 ms. Both synchronously exceed a 60 Hz frame budget.
- JUCE canvas paints averaged 58.30 ms and reached 81.66 ms. OpenGL renders
  averaged 12.78 ms and reached 39.62 ms. Full-canvas JUCE presentation is the
  dominant measured cost.
- Hover, pointer, and viewport repaint latency was ordinarily about 10 ms, with
  one hover reaching 40.20 ms. Graph and parameter edit repaint latency reached
  251.10 ms and 242.96 ms respectively, showing that edit/publication work and
  queued presentation work materially delay visible feedback.
- Sixteen repaint requests became eleven invalidation flushes and nine JUCE
  paints. Coalescing is active (1.45 requests per invalidation flush), but it
  does not compensate for the measured paint cost.

These absolute timings are Debug-build and host dependent. Use the same build,
window size, preset, session transport, and fixture when comparing changes.
The optimization order suggested by this audit is: reduce/restrict JUCE canvas
painting, separate synchronous graph/parameter publication work from immediate
feedback, then profile the expensive pointer-drag path. Timer, hover, and
viewport micro-optimization should follow only if a broader representative
distribution changes their contribution.

Source inspection supports the measured ordering:

- each JUCE repaint traverses grid, edges, annotations, nodes, Guide/Spy dock,
  minimap, legend, palette, and status presentation through
  `NodeCanvasPresentation::paint`; repaint invalidation currently has only a
  full-canvas category, although `NodeCanvasScene` itself correctly caches its
  geometry by graph/document/viewport/presentation revisions;
- every OpenGL canvas render visits every visible curve-model node, and
  `CurvePanelHost::renderPreview` performs panel resize, curve preparation,
  panel rendering, framebuffer image capture, and snapshot publication. Debug
  logs show Waveshaper and Impulse Response geometry/waveform preparation
  repeating throughout an otherwise idle measurement window;
- the instrumentation intentionally leaves these authoritative paths unchanged.
  The next optimization slice should add revision/size invalidation around
  curve preview snapshot rendering and introduce narrower dirty-region or
  layer caching below `NodeCanvasPresentation`, with this fixture guarding the
  end-to-end effect.

## Trimesh, Spy, and Delay Baseline

Captured on 2026-08-27 with the same Debug build through the session transport,
using a 120 ms inter-command settling interval. These figures are distributions
over small focused gestures, not release-mode benchmarks.

- Three Trimesh point updates averaged 1.62 ms; gesture commit took 15.81 ms.
  The causal preview worker took 17.84 ms: 4.96 ms in incremental graph/audio
  traversal and 7.97 ms in preview/Spy extraction. The completed worker then
  waited 162.32 ms for message-thread publication, making request-to-publication
  latency 182.55 ms. The low Spy resolution is therefore not the dominant
  computation in this sample.
- The Trimesh gesture requested nine editor-attributed canvas repaints plus one
  publication repaint. JUCE canvas paints averaged 71.35 ms and reached
  150.96 ms; edit/publication repaint latency reached roughly 1.08 seconds.
  Apparent Spy delay is primarily message-thread and presentation pressure.
- Five Delay beats updates averaged 0.034 ms; no preview audio or extraction
  ran. Commit took 14.85 ms, the worker took 0.041 ms, and message-thread
  publication still waited 108.51 ms. JUCE canvas paints averaged 48.76 ms and
  reached 95.11 ms. Delay DSP is not the source of slider lag.

The first guarded optimization keeps hosted-editor invalidation local and
suppresses canvas hover work/repaint when the expanded editor occludes the
pointer or hover presentation state is unchanged. Delay now repaints its local
preview explicitly. Under the same fixtures, edit-driven canvas repaint
requests fell from 10 to 1 for Trimesh and from 5 to 1 for Delay. The remaining
publication delay and expensive JUCE parent paints justify a subsequent
message-thread scheduling and clipped/layer-caching optimization slice; they
are now directly measurable in schema v2.

The next clipped-presentation slice separates ordinary console hover from
relationship hover. Node, port, edge, and action hover now request only the
console rectangle and enter the existing status renderer directly, skipping
node, cable, preview, Guide/Spy, utility, and OpenGL-preview presentation.
Guide tile, Spy, and palette hover continue to request the full canvas. Target
node hover does not infer a Guide relationship; it remains an ordinary console
hover. Repaint-scope metrics report two status requests and zero full-canvas
requests for two ordinary semantic target transitions, while same-target
movement remains at zero and the focused Guide hover reports one full-canvas
request.
