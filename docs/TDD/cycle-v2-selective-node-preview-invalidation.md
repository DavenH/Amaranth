# Cycle V2 Selective Node Preview Invalidation

## Status

Implemented.

## Problem

The node sprite cache keys every entry on the global presentation revision.
A single Trimesh preview publication therefore invalidates all 15 visible node
sprites, producing an 86 ms node-layer frame even though only the affected
runtime preview changed.

## Technical Design

`NodeCanvasPresentation::paintNode` remains the authoritative node painter and
`NodePreviewRenderer` remains the authoritative preview painter. The existing
node cache already compares immutable node state, viewport, resource revision,
selection, bounds, and physical scale. The global revision currently acts as a
coarse surrogate for three additional inputs that must be made explicit before
it can be removed:

- the exact `NodePreviewResult` for that node;
- graph-resolved render semantics and Trimesh Guide configuration;
- the effective Unison/Voice preview context consumed by that node.

Each produced preview now has an immutable content revision. Incremental
rendering preserves that revision when a dirty rerender produces identical
visible content, so the UI hit path compares revisions in constant time. The
cache retains the exact runtime result as a fallback for manually constructed
results without a revision. A narrow render-context fingerprint covers
semantic enum values, the mature
`TrimeshGuidePreparation::configurationKey`, and the effective Unison context.
Runtime sprite signatures also include secondary values, which are visibly
consumed by output-meter previews.

No graph, rendering, Guide, Unison, or preview behavior is reimplemented. The
change only translates authoritative visible inputs into a complete cache key.

## Verification

- Cache tests prove that one node's runtime publication misses only that node,
  and cover runtime presence/content, context, resources, viewport, node state,
  selection, scale, drawing, and eviction.
- Renderer tests protect secondary runtime values in preview signatures.
- The Trimesh/Spy fixture must replace the observed 15-miss publication frame
  with selective misses and materially reduce the 86 ms node-layer maximum.
- A canvas capture verifies node previews, domains, Guide-aware Trimesh pixels,
  ports, and selection composition.
- Focused and complete Cycle V2 tests, standalone Debug, style, hot-loop, and
  diff checks complete before commit.

The Trimesh/Spy fixture measured the following change over ten frames:

- node-cache misses fell from 15 to 2;
- node painting mean/max fell from 10.53/86.03 ms to 2.92/14.15 ms;
- complete JUCE painting mean/max fell from 22.78/105.86 ms to 13.38/49.29 ms;
- cache-hit node frames remained inexpensive at a 1.69 ms mean, compared with
  1.48 ms before this slice;
- preview-worker time remained effectively unchanged at 18.91 ms, compared
  with 19.04 ms before this slice.

The focused Trimesh performance fixture passed. The final visual capture at
`/private/tmp/cycle-v2-node-selective-visual.png` preserves node previews,
semantic colours, ports, curves, Guides, Spy previews, and cables.

The standalone Debug application and tests build successfully. All focused
cache, preview-content, and incremental-publication tests pass. The complete
Cycle V2 suite passes 515 of 516 cases; the sole failure is the pre-existing,
documented hover-help assertion at `TestNodeCanvasHitRouter.cpp:66`. Diff and
hot-loop checks pass. `clang-tidy` is not installed in the local environment.

## Stable End State

Node sprites invalidate from their complete visible inputs. Global UI event or
publication revisions no longer discard unrelated node pixels.
