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
