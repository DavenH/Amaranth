# Cycle V2 Viewport Pan Performance

## Status

Complete (2026-09-10).

## Problem

Panning a complex graph is visibly laggy even though viewport event handling is
negligible. The existing node and cable sprite caches key their pixels on
absolute screen placement, so every viewport translation rerasterizes all
visible graph content.

## Authoritative Boundaries

- `NodeCanvasViewport` remains the owner of world-to-screen translation and
  zoom.
- `NodeCanvasPresentation` remains the paint and composition owner.
- `NodeCanvasNodeLayerCache` and `NodeCanvasCableLayerCache` remain narrow
  presentation caches around the existing node and cable renderers.
- `NodeCanvasPresentation::paintNode` and `NodeCableRenderer::paint` remain the
  only pixel producers. The cache must not reproduce their rendering logic.
- `CanvasPerformanceMetrics` and the agent session transport remain the
  before/after measurement boundary.

## Baseline

The Debug standalone opened `downfall.cyclegraph` (22 nodes, 23 edges) and ran
eight individually settled wheel-pan events. Viewport handlers averaged
0.008 ms, while JUCE paints averaged 79.95 ms and reached 97.81 ms.

- Nodes averaged 31.02 ms with 176 misses and zero hits.
- Cable bodies averaged 23.84 ms with 152 misses and zero hits.
- Canvas utilities averaged 20.26 ms; the always-visible palette accounted for
  16.98 ms.
- OpenGL renders averaged 0.29 ms and are not the bottleneck.

Artifact: `/private/tmp/cycle-v2-pan-baseline.json`.

## Design

Cache node and cable sprites in translation-independent local coordinates.
Panning changes only each sprite's destination rectangle; node content, cable
shape, zoom, physical scale, selection, previews, and all existing semantic
keys remain authoritative invalidators.

For cables, compare source, destination, and path after translating them into
their sprite bounds. A changed relative shape still misses. The composed cable
image may rebuild when placement changes; it must draw reused authoritative
sprites rather than rerasterizing cable paths.

Keep visibility eviction unchanged. A node or edge leaving the visible frame
is released, and content re-entering after eviction may miss normally.

## Completion Criteria

- Focused cache tests prove that pure translation hits and draws at the new
  position while size, zoom, scale, content, style, and relative geometry still
  invalidate.
- The complex-panning fixture records cache hits and materially lower node,
  cable, and total paint durations.
- A panned canvas screenshot preserves node, cable, and inline-control
  alignment.
- Focused tests, the standalone build, style checks, and `git diff --check`
  pass.

## Stable End State

The cache translation is a presentation-only optimization with no graph,
interaction, DSP, or serialization behavior. No compatibility layer or
deletion target is introduced.

## Implementation And Measurement Result

Node sprites now rasterize against exact floating local bounds and reuse their
pixels across viewport translation. The old viewport-revision and absolute
screen-rectangle keys were removed; zoom still changes sprite size and misses.
The focused sequence also proves that a translated hit draws at its new screen
position.

Cable sprites now compare a translation-independent, subpixel-quantized shape
fingerprint. Their second-level composite no longer rebuilds continuously: a
changed layout draws the authoritative sprites directly, and a composite is
rebuilt only after the same layout is observed again. Image revisions prevent
a stale composite from surviving style or geometry changes.

The palette was the largest screen-space utility cost and does not depend on
the viewport. Its authoritative painter now feeds a presentation-owned image
keyed by active section, hovered entry, bounds, and physical scale.

The identical eight-pan Downfall fixture improved as follows:

- JUCE paint mean: 79.95 ms to 44.82 ms (44% reduction).
- JUCE paint maximum: 97.81 ms to 53.66 ms (45% reduction).
- Node stage: 31.02 ms to 14.85 ms, with 176 hits and zero misses after the
  warm-up frame.
- Palette stage: 16.98 ms to 0.63 ms.
- Cable-body stage: 23.84 ms to 20.54 ms in the full distribution. Isolated
  per-pan samples measured 13.46–14.10 ms; each retained 16–17 sprite hits and
  rerasterized only two or three newly visible or changed edges.
- OpenGL remained below 0.4 ms average and was not optimized.

The broader mixed canvas fixture improved from a 42.23 ms to 32.84 ms paint
mean (22%) while preserving its graph and parameter-edit work.

Artifacts:

- `/private/tmp/cycle-v2-pan-baseline.json`
- `/private/tmp/cycle-v2-pan-confirm.json`
- `/private/tmp/cycle-v2-general-confirm.json`
- `/private/tmp/cycle-v2-pan-final.png`

The final screenshot preserves node, cable, port, inline Pan, palette, dock,
and utility alignment after the round-trip pan. The next material candidates
are a translation-aware whole-node composite and a wider retained cable
working set; together the remaining node/cable sprite composition accounts for
roughly 35 ms of the Debug frame. Those require their own bounded cache design
rather than broader invalidation suppression.

Verification completed with 105 focused cache assertions, 1,238 assertions in
95 canvas cases, the standalone Debug build, and the runtime fixture/capture.
The full suite passed 661 of 665 cases. Its failures are outside this change:
the already-recorded spectral amplitude thresholds and African Horn canonical
JSON check, plus the independently reproduced Envelope rail-spacing mismatch
recorded in `ui-bugs.md` during this audit.
