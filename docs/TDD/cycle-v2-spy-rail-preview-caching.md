# Cycle V2 Spy Rail Preview Caching

## Status

Implemented.

## Problem

After node and cable sprite caching, Spy rail painting remains the largest
repeatable uncached presentation cost in the Trimesh editing fixture. Each
paint rebuilds a compact `NodePreviewResult`, copies its complete value vector,
and asks the authoritative preview renderer to rasterize every visible tile,
even when only hover or canvas interaction state changed.

## Technical Design

`GraphPreviewResult::SignalProbePreview` is the authoritative published Spy
preview. `NodePreviewRenderer` remains the sole authoritative pixel producer.
`SignalProbeRail` owns presentation ordering and will own a narrow per-preview
sprite cache; it must not reproduce heatmap, waveform, spectrum, or Trimesh
rendering.

The cache covers only the preview rectangle. Tile chrome, selection and hover
borders, keyboard focus, ordinals, overflow feedback, and disconnected state
remain live composition. A cache entry is reusable only when the probe ID,
complete preview values and metadata, resolved render semantic, logical bounds,
and physical display scale match exactly. Entries absent from a visible rail
frame are evicted.

The existing aggregate `spyRail` presentation stage remains comparable with
prior captures. A nested `spyRailPreviews` stage attributes the connected
preview portion, and `presentationCache.spyPreviewTiles` exports hit/miss
counts plus all-hit and miss-containing frame durations. Parent and child stage
durations are not additive.

## Ownership and Lifecycle

- Runtime preview workers publish immutable value snapshots; no worker or DSP
  behavior changes.
- `SignalProbeRail` and its cache live and paint on the message thread.
- `NodePreviewRenderer` remains the sole preview rasterizer.
- `CanvasPerformanceMetrics` records bounded counters and distributions.
- Graph mutation, undo, preview scheduling, and realtime refresh policy remain
  unchanged.

## Verification Plan

- Focused cache tests cover exact reuse, every semantic preview-key category,
  bounds, physical scale, drawing, and eviction.
- Metrics tests cover the new substage and exported/reset cache data.
- The existing Trimesh/Spy performance fixture provides comparable before and
  after captures and confirms all-hit behavior during unchanged repaints.
- A focused UI capture checks preview pixels, tile chrome, ordinals, and probe
  annotations after integration.
- Standalone Debug and the applicable Cycle V2 tests pass; style, hot-loop, and
  diff checks are clean.

## Measurement Evidence

The prior Trimesh/Spy capture measured the uncached Spy rail at approximately
3.45 ms per paint. The identical point-edit fixture after this change recorded
81 cache hits and 9 misses across ten frames: the one published preview change
correctly rerasterized all nine tiles, while every unchanged frame reused all
nine.

All-hit preview composition averaged 1.00 ms. The genuine nine-tile miss cost
17.01 ms. The full Spy rail averaged 1.88 ms on the nine reuse frames (derived
by excluding the single 17.28 ms miss frame), a 46% reduction from the prior
3.45 ms repeat cost. The ten-frame aggregate remained 3.42 ms because it
intentionally includes the one required cold rerasterization.

The export now distinguishes `presentationStages.spyRailPreviews` and
`presentationCache.spyPreviewTiles`, making future hit-path and invalidation
regressions visible independently of rail chrome.

## Verification Evidence

The standalone Debug app and Cycle V2 test target build successfully. Focused
cache and metrics tests pass with 68 assertions. The complete Cycle V2 suite
runs 514 cases with 513 passing; the sole failure remains the pre-existing
edge-hover help assertion at `TestNodeCanvasHitRouter.cpp:66`, documented in
`docs/TDD/ui-bugs.md` and outside the Spy presentation path.

The Trimesh/Spy automation fixture passes. A 1728x962 canvas capture confirms
that cached time, magnitude, and phase heatmaps retain their distinct pixels,
domain-coloured tile borders, ordinals, and dock composition.

## Stable End State

The cache is a replaceable presentation optimization around immutable runtime
results. It owns no graph, interaction, preview-generation, or rendering-domain
logic and can be deleted without changing observable semantics.
