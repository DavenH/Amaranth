# Cycle V2 Presentation Cache Lifecycle

Status: implemented on 2026-08-31

## Problem

Presentation caches can outlive the graph content that produced them. This is
visible as stale Spy tiles after loading another preset, invalid first-frame
Envelope/Waveshaper/IR snapshots, and Guide preview renders flashing at the
canvas origin. Guide edits in `Live` mode also synchronously recompile the
graph, preventing repeated latest-state feedback.

## Authoritative Implementations

- `GraphPresentationModel` owns compiled plans, incremental preview execution,
  latest-generation publication, and the durable presentation snapshot.
- `GraphCommandDispatcher` owns transient Guide publications and their
  consolidated `GraphChangeSet`.
- `CurvePanelHost` owns OpenGL curve rendering and framebuffer snapshot
  publication. Its snapshot is valid only after the widget has synchronized
  the current graph model and completed that render.
- `NodeCanvasPresentation`, `NodeCanvasNodeLayerCache`, and
  `SignalProbePreviewTileCache` own raster presentation caches only. They must
  not decide graph or preview semantics.

## Design

1. A full document replacement explicitly resets presentation-only caches.
   Cache entries may reuse node/probe ids and numeric revisions within a new
   document, so those values are not a document identity.
2. Curve widgets synchronize the current graph model at the existing OpenGL
   preview-render boundary, after context-dependent resources exist and before
   snapshot capture. A default-model snapshot must not become the first
   authoritative preview for a loaded node.
3. Guide preview readback retains the panel renderer's proven capture bounds,
   then immediately restores the OpenGL canvas underlay before publication.
   The temporary render therefore cannot leak at the canvas origin while the
   JUCE layer catches up.
4. Guide content changes preserve the compiled topology. They refresh the
   affected Trimesh configurations and preview products. `Live` movement uses
   the existing coalesced asynchronous latest-generation scheduler; commit
   persists once and supersedes stale movement work.

## Boundaries

- No cache duplicates model, graph traversal, rasterizer, or DSP behavior.
- No UI code mutates the graph outside `GraphCommandDispatcher`.
- Cache reset is a lifecycle operation, not a substitute for comparing
  ordinary same-document content dependencies.
- Guide add/remove/assignment remains expressed by the existing semantic
  change set; the compiled node/edge topology is unchanged.

## Verification

- Load African Horn and then Baroque Flute in one process; the second snapshot
  exposes only Baroque Flute probe values and freshly painted tiles.
- A loaded curve node is synchronized before its first cached OpenGL preview.
- The OpenGL underlay is restored in the same frame after Guide readback.
- Two Guide updates in one transient gesture change the Spy payload twice,
  compile zero additional times, commit once, and undo once.
- The focused Live-spy automation performs two updates and observes distinct
  probe metrics after each publication.

## Completion Criteria

- All four reported regressions have focused coverage.
- Presentation cache reset is wired to graph-file and snapshot replacement.
- Guide movement no longer recompiles topology or blocks the message thread on
  synchronous preview work.
- Relevant tests, focused automation, screenshot capture, style checks, and a
  coherent commit are complete.

## Verification Evidence

- `[cycle-v2][runtime][guides]`: 26 assertions in 2 cases passed.
- `[cycle-v2][canvas][performance][cache]`: 92 assertions in 5 cases passed.
- `[cycle-v2][canvas][presentation]`: 76 assertions in 8 cases passed.
- `cycle-v2-agent-guide-editor-live-spy.json` passed.
- `cycle-v2-agent-preset-cache-reset.json` passed.
- OS captures:
  `/private/tmp/cycle-v2-preset-switch-after2.png`,
  `/private/tmp/cycle-v2-alto-curve-preview.png`, and
  `/private/tmp/cycle-v2-stengah-curve-preview.png`.
- `git diff --check` passed. `clang-tidy` was unavailable in the environment.
