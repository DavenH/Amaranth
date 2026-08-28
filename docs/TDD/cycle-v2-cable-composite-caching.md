# Cycle V2 Cable Composite Caching

## Status

Implemented.

## Problem

Cable sprites now hit their per-edge cache during ordinary preview publication,
but every full canvas frame still composites all 16 visible sprites separately.
The Trimesh/Spy workload measures cable bodies at a 2.18 ms mean and 5.70 ms
maximum even with 160 sprite hits and no misses.

## Technical Design

`NodeCableRenderer` remains the sole cable pixel producer. The existing
`NodeCanvasCableLayerCache` remains authoritative for complete per-edge
geometry, style, zoom, and physical-scale comparison. A second cache level
assembles those authoritative sprites into one transparent image.

A composite hits only when every visible per-edge sprite hits and the ordered
visible edge membership, clipped union bounds, and physical scale match the
previous frame. Any sprite miss, addition, removal, reorder, canvas clipping
change, or scale change rebuilds the composite from the existing sprites. The
composite does not inspect graph or cable domain state and does not reproduce
rendering behavior.

The cache reports frame-level composite hits, misses, and their duration in
addition to the existing per-edge sprite statistics. This keeps the previous
metric comparable while making the new optimization directly observable.

## Measurement Result

The composite and control builds were measured in separate external app
sessions with the same 16-cable graph, ten parameter updates, and 120 ms
between commands. Unchanged per-edge composition averaged 6.67 ms across 11
all-hit frames. The composite recorded ten hits averaging 3.34 ms, a 50%
reduction. Including its one 15.01 ms cold rebuild, cable-body mean still fell
34%, from 6.67 to 4.40 ms.

A shorter four-frame sample was initially ambiguous because the cold rebuild
dominated its three hits. Extending the distribution established the intended
steady realtime behavior and retained the cold cost in the miss metric rather
than hiding it. Full-frame totals were not used to claim the effect because
unrelated node and Spy cold misses differed between sessions.

The OS capture at `/private/tmp/cycle-v2-cable-composite-visual.png` preserves
ordinary cables, modulation bundles, probe ordinals, node composition, and
Spy previews.

Standalone Debug and test targets build successfully. Focused cache and metrics
tests pass with 91 assertions. The complete Cycle V2 suite passes 516 of 517
cases; the sole failure is the pre-existing, documented hover-help assertion
at `TestNodeCanvasHitRouter.cpp:66`. Diff and hot-loop checks pass. No new node
kind branches or domain rendering logic were introduced. `clang-tidy` is not
installed in the local environment.

## Verification

- A cache sequence test covers initial composition, unchanged reuse, style
  invalidation, ordered membership removal, scale, bounds, and output pixels.
- Canvas metrics export and reset frame-level composite outcomes.
- The paired external sessions demonstrate reduced all-hit and aggregate
  cable-body duration.
- A focused capture verifies ordinary and modulation-bundle cables, Spy probe
  ordinals, and surrounding composition; the cache sequence covers selected
  style invalidation.
- `cycle-v2-agent-cable-composite-performance.json` remains as a stable
  parameter-publication workload because the older point-drag fixture can land
  on a selection-only gesture when its mesh geometry changes.

## Stable End State

Unchanged cable bodies require one cached image composition per full frame.
Individual cable changes continue to repaint through the mature renderer and
reassemble the composite without stale pixels.
