# Cycle V2 Canvas Cable and Utility Optimization

## Status

Implemented.

## Problem

Per-node sprites removed much of the repeat node-paint cost, leaving cable
painting at roughly 4-5 ms per full frame and intermittent 30-60 ms utility
outliers in the Delay and Trimesh editing fixtures. The existing aggregate
`cables` and `utilities` metrics do not distinguish stable cable raster work
from probe annotations, or identify which utility produces the outliers.

## Technical Design

`NodeCanvasScene` is the authoritative cable-geometry owner and already caches
the visible and hit-test paths for a graph, document, viewport, and
presentation snapshot. `NodeCableRenderer` remains the sole authoritative
cable painter. `SignalProbeRail` remains the cable-annotation painter, and
`NodeCanvasPresentation` remains the composition owner for minimap, legend,
palette, and status painting.

The next instrumentation slice adds nested, fixed-storage duration
distributions for cable bodies, cable annotations, minimap, legend, palette,
and status. The existing aggregate stages remain available for comparison;
their children are attribution metrics and must not be summed with the parent.
No renderer or invalidation behavior changes in this slice.

Representative external-session Delay and Trimesh fixtures select the next
optimization. Stable cable pixels may be cached only through the existing
`NodeCableRenderer`, with a key covering the authoritative scene edge,
rendering style, zoom, physical scale, and sprite bounds. Probe annotations
remain independent because hover, selection, and workspace state change their
output. If a utility is dominant instead, optimize that utility at its
existing presentation boundary with equivalent complete invalidation.

## Ownership and Lifecycle

- `NodeCanvasScene` owns cable path construction and its scene-snapshot cache.
- `NodeCanvasPresentation` owns paint order and cache lifetime on the message
  thread.
- `CanvasPerformanceMetrics` records bounded duration distributions without
  allocating in the paint path.
- Semantic graph mutation and realtime preview publication are unchanged.

## Measurement Evidence

The external session transport ran the same Delay-slider and Trimesh/Spy
fixtures as the first node-cache slice, allowing a message-loop interval
between every gesture command. Cable bodies accounted for 3.74 of the 3.77 ms
mean cable stage in Delay and 3.39 of 4.29 ms in Trimesh. Cable annotations
averaged 0.03 and 0.90 ms respectively.

The apparent utility outliers were localized to palette painting: the Delay
palette had one 26.28 ms maximum and the Trimesh palette one 19.38 ms maximum,
while their repeat frames were below 0.5 ms. Minimap, legend, and status were
not repeatable bottlenecks. The broader canvas-mutation fixture confirmed
cable bodies as stable work at 9.87 ms mean and palette warm-up as intermittent
work at 16.46 ms mean.

## Implemented Cache

`NodeCanvasCableLayerCache` owns one transparent HiDPI sprite per visible scene
edge. `NodeCableRenderer::paint` remains the only pixel producer. A sprite is
reused only when its exact path, endpoints, endpoint presentation flags, cable
style, logical bounds, zoom, and physical scale match. Entries are addressed
by the scene edge index, compared by visible content, and evicted after a frame
in which they are absent. Pending connections and probe annotations remain
uncached and independently composited.

Metrics export `presentationCache.cableLayer` hit/miss counts and all-hit or
miss-containing frame distributions. Focused tests cover visible geometry,
style, zoom, scale, reuse, drawing, and eviction.

## Optimization Evidence

With identical external-session fixtures, the Delay cable-body mean fell from
3.74 to 1.79 ms (52%), Trimesh from 3.39 to 1.89 ms (44%), and the broader
canvas workload from 9.87 to 4.97 ms (50%). Delay recorded 90 hits and no
misses; Trimesh recorded 128 hits and no misses. The topology/viewport workload
recorded 96 hits and 16 misses, demonstrating selective invalidation rather
than stale whole-layer reuse.

The Delay full-paint mean fell from 13.78 to 10.60 ms. Trimesh and the broad
canvas fixture remained dominated by preview-node misses, Spy painting, and
palette warm-up, so their total means are noisy even though cable-body cost
fell. An OS-level canvas capture of the 16-edge, nine-probe graph retained
cable routing, domain colours, endpoint markers, probe badges, and layer order.

The standalone Debug target builds successfully. Focused presentation metrics
and cache tests pass. The complete Cycle V2 executable ran 513 test cases with
512 passing; its sole failure is the pre-existing edge-hover help-text
regression in `TestNodeCanvasHitRouter.cpp`, already recorded in
`docs/TDD/ui-bugs.md` and outside the paint/cache path.

## Completion Criteria

- Focused tests cover substage aggregation, export names, and reset.
- Delay and Trimesh editing captures identify the repeatable dominant child
  stage and the source of utility outliers.
- The selected optimization reuses the authoritative painter and has focused
  invalidation and eviction tests.
- Before/after captures show reduced frame cost without stale cable, probe,
  palette, minimap, legend, or status pixels.
- The focused UI fixture, standalone build, and applicable Cycle V2 tests pass;
  style and diff checks are clean.

## Stable End State

Substage metrics remain diagnostic infrastructure. Any cache is a narrow
presentation implementation detail and does not duplicate geometry,
rasterization, interaction, or graph behavior.
