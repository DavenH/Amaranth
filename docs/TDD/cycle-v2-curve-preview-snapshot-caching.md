# Cycle V2 Curve Preview Snapshot Caching

## Status

Complete.

## Problem

Every canvas OpenGL frame asks every visible curve-model node to resize,
prepare, render, read back, and publish a compact preview snapshot. Most frames
do not change the curve content or its rendered size, so this repeats the most
expensive part of the curve-panel path while the cached image is already valid.

## Authoritative Implementation

- `CurvePanelHost::renderPreview` remains the sole owner of panel preparation,
  OpenGL rendering, framebuffer capture, and snapshot publication.
- `CurveEditorWidget` and `CurvePanelController` continue to translate the node
  editor lifecycle into that host; they do not reproduce rendering behavior.
- `NodeCanvasPresentation` continues to decide which curve nodes are visible.

The cache guard belongs in `CurvePanelHost`, immediately outside the existing
authoritative render sequence. A hit reuses the last published snapshot; a miss
executes that sequence unchanged.

## Cache Contract

A compact preview snapshot is reusable only while all of these remain equal:

- logical preview width and height;
- OpenGL scale factor;
- curve model revision;
- transient curve-content revision;
- preview-presentation revision for non-model view changes;
- the host's panel invalidation generation.

Screen position is intentionally excluded. The snapshot is painted into the
current node bounds and its pixels depend on size and scale, not canvas
translation. Empty bounds do not establish a valid cache entry. Releasing the
shared OpenGL resources invalidates the entry.

Panel callbacks increment the host invalidation generation. The generation is
captured only after a successful render, so invalidations produced during
preparation are represented by the newly published snapshot rather than
forcing an unconditional second render.

## Complexity and Lifecycle

An unchanged visible curve node performs `O(1)` cache-key comparison per
OpenGL frame. Rendering and framebuffer readback occur only on a cache miss.
The cache stores metadata only; the existing `CurvePanelSnapshotCache` remains
the image owner. The OpenGL render thread owns cache decisions, while the panel
invalidation generation is atomic because callbacks originate on the JUCE
message thread.

## Completion Criteria

- Repeated requests with the same content, size, scale, and invalidation
  generation execute one authoritative render.
- Content, presentation, size, scale, panel invalidation, and OpenGL resource
  release each force the next render.
- Cache hit/miss diagnostics are observable without affecting scheduling.
- A focused runtime fixture shows repeated idle frames reusing snapshots while
  curve edits still visibly refresh them.
- Focused semantic tests, the Cycle V2 test suite, and the standalone build pass.

## Stable End State

The optimization is a narrow guard around `CurvePanelHost::renderPreview`.
There is no compatibility adapter or duplicated renderer, and no deletion
target is introduced.

## Implementation

- `CurvePanelPreviewRenderCache` compares the explicit render dependencies and
  records hit/miss diagnostics without owning image data.
- `CurvePanelHost::renderPreview` returns immediately on a hit. A miss executes
  its existing panel preparation, render, framebuffer capture, and snapshot
  publication unchanged, then records the invalidation generation observed at
  the end of that render.
- Curve model and transient-content revisions come from the existing
  controller lifecycle. `CurveEditorWidget` supplies a separate presentation
  revision for view-only changes such as Envelope polarity and vertical-range
  adjustment.
- Panel invalidation callbacks advance an atomic generation, and releasing the
  shared OpenGL resources invalidates the cache entry.
- The automation-only `requestCanvasOpenGLFrame` command requests a real canvas
  OpenGL frame so cache fixtures do not depend on incidental repaint timing.

## Runtime Proof

The focused `cycle-v2-agent-curve-preview-cache.json` fixture ran through the
Cycle V2 session transport with 120 ms between commands. It created a visible
Envelope node, requested stable OpenGL frames, changed Red morph from 0.5 to
0.65, and captured the resulting canvas.

- The edited Envelope reported four cache hits and two misses across six
  observed requests, avoiding two thirds of its otherwise-authoritative compact
  panel renders. The first stable snapshot served two consecutive requested
  frames; the Red edit forced the second miss.
- The session delivered 30 OpenGL canvas frames. Curve rasterizer preparation
  no longer repeated once per frame; it occurred only for cache misses and
  explicit panel invalidations.
- All fixture assertions passed, including nonzero reuse, required misses, and
  the observable Red morph update to 0.65. The final canvas capture retained
  the Envelope, Waveshaper, Impulse Response, and guide preview imagery.

Artifacts:

- `/private/tmp/cycle-v2-curve-cache-final-report-2.json`
- `/private/tmp/cycle-v2-curve-cache-final-2.log`
- `/private/tmp/cycle-v2-curve-preview-cache.png`

## Verification

- Cache dependency test: 12 assertions passed.
- Node editor host tests: 112 assertions passed across 6 test cases.
- Focused session fixture: all commands and assertions passed.
- `standalone-debug` Cycle V2 build passed with `--parallel 10`.
- Full suite: 745 of 746 tests passed. The sole failure is the pre-existing,
  documented `Node canvas hit routing preserves action edge and palette
  placement semantics` regression.
- `git diff --check` and the visualization hot-loop scalar-math check passed;
  `clang-tidy` is unavailable in the current environment.
