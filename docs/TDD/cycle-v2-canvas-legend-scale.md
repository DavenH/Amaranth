# Cycle V2 Canvas Legend Scale

Status: Implemented (2026-09-05)

## Objective

Increase the canvas utility legend's text and domain samples by 30 percent so
they remain legible at production size, without disturbing the minimap,
performance keyboard, or compact-window containment.

## Authoritative Implementation

- `NodeCanvasPresentation::paintLegend` owns the four semantic domain entries
  and their rendering.
- `CanvasUtilityDock::layout` owns the minimap, legend, keyboard, and status
  geometry and its compact collision policy.
- `CanvasChromeMetrics` owns shared canvas typography and geometry values.

This slice changes only legend metrics and its preferred surface height. It
does not alter port-domain colours, entries, utility-dock order, keyboard
minimums, or minimap minimums.

## Test-First Contract

1. Legend font size, line length, stroke, insets, row stride, and text bounds
   are exactly `1.3` times their current production values.
2. The preferred legend surface height grows by the same factor and remains
   aligned to the minimap's right edge.
3. Compact layout continues to clip the legend surface before the keyboard;
   neither the keyboard nor minimap shrinks to accommodate it.
4. Painting is clipped to the returned legend bounds so responsive height
   reduction cannot spill entries into the keyboard.

## Negative Boundaries

- Do not enlarge only the font or only the colour samples.
- Do not change domain colours, labels, or entry order.
- Do not reduce the performance keyboard or minimap.
- Do not introduce a second responsive dock implementation.

## Completion Criteria

- Focused metric, layout, and legend semantic tests pass.
- A production screenshot confirms the enlarged legend remains aligned and
  contained.
- Standalone Debug builds; the Cycle V2 suite, `git diff --check`, style
  review, and production-diff review pass before commit.

## Implementation Evidence

- `CanvasChromeMetrics` now centralizes the legend's font, line, stroke,
  insets, text box, and row stride at exactly `1.3` times their former values.
- `CanvasUtilityDock` grows the preferred legend surface from `98` to `127.4`
  pixels and raises its compact minimum from `30` to `39` pixels. Its existing
  collision policy continues to preserve the minimap and keyboard dimensions.
- Legend painting is clipped to the responsive surface bounds, preventing
  entries from spilling into the keyboard when the dock shortens the legend.
- Focused legend and utility-dock tests pass 34 assertions. Production evidence
  at `/private/tmp/cycle-v2-legend.png` confirms the enlarged samples and text
  remain aligned and contained at the standard window size.
- Standalone Debug builds. The full Cycle V2 suite completed with 11,085 of
  11,086 assertions passing; the sole failure is the pre-existing edge-help
  assertion at `TestNodeCanvasHitRouter.cpp:66`. `git diff --check`, style
  review, and production-diff review passed; `clang-tidy` is unavailable in
  the local environment.
