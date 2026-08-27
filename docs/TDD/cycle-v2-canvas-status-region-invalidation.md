# Cycle V2 Canvas Status Region Invalidation

## Status

Complete.

## Problem

Ordinary node, port, edge, and action hover changes only the canvas console
text. The current hover optimization suppresses unchanged semantic hover, but
every changed hover still requests a full `NodeCanvas` repaint. Baseline Debug
measurements put those parent paints at roughly 50-70 ms, so moving between
ordinary targets still pays for every node preview, cable, Guide/Spy panel, and
canvas utility.

Guide, Spy, and palette hover are different: they alter relationships,
highlights, tethers, tiles, or pullouts outside the console and must retain
full-canvas invalidation.

## Authoritative Implementations

- `NodeCanvas::updateHoverAt` remains the owner of resolved hover presentation
  state and decides whether a semantic hover transition occurred.
- `NodeCanvasPresentation` remains the only renderer of the canvas console and
  full canvas. The narrow paint entry point reuses its existing `paintStatus`
  implementation; it does not reproduce console layout or styling.
- `CanvasUtilityDock::layout` remains the authority for the console rectangle.
- `RenderInvalidationAccumulator` remains the coalescing boundary. It carries
  independent full-canvas and status-region categories but no UI policy.
- `CanvasPerformanceMetrics` remains the measurement boundary and records full
  versus status-region repaint request counts without changing scheduling.

No adapter or copied rendering behavior is introduced. The translation at the
boundary is only from semantic hover effects to a repaint scope.

## Design

`NodeCanvas` classifies each hover update as one of:

- none, when all semantic presentation state is unchanged;
- status, when only resolved console hover text changed;
- canvas, when palette, Guide, or Spy hover state changed.

Canvas invalidation dominates status invalidation when both are pending. A
status-only dispatch calls JUCE's rectangle repaint with the console bounds.
During that clipped paint, `NodeCanvas` invokes the existing presentation
status renderer and skips node, cable, preview, dock, and OpenGL-preview work.
All other paint clips retain the existing full presentation path.

Mouse exit and expanded-editor occlusion use the same classification so stale
console text is cleared with a status repaint while relationship presentation
still receives a full repaint when necessary.

## Measurement Contract

The canvas performance snapshot adds full and status-region repaint request
counts. Existing trigger attribution, repaint latency, paint duration, and
coalescing metrics remain unchanged. The focused hover fixture asserts that:

1. entering an ordinary node requests one status repaint;
2. movement within the same semantic node requests no repaint;
3. entering a different ordinary node requests one more status repaint;
4. no full repaint is attributed to those transitions.

## Completion Criteria

- Ordinary semantic hover changes repaint only the console region.
- Palette, Guide, and Spy hover changes still repaint the complete canvas.
- Canvas exit and editor occlusion clear console hover without repainting the
  full canvas when no relationship state changed.
- Repaint-scope metrics reset and export deterministically.
- Focused unit tests and Cycle V2 automation cover the classification and
  measured repaint scopes.
- Cycle V2 tests and standalone build pass; style and diff checks are clean.

## Stable End State

This slice establishes the first narrow canvas layer: the console. Further
static-content caching can add presentation-owned layers behind the same
invalidation boundary. The status category is permanent infrastructure; there
is no deletion target in this slice.

## Implementation Evidence

Implemented on 2026-08-27. The focused ordinary-hover fixture records two
semantic hover repaint requests as two status-region requests and zero
full-canvas requests. Its same-target move leaves both counts unchanged. The
Guide tether fixture records its hover transition as one full-canvas request
and zero status requests, preserving relationship presentation.

An OS-level canvas capture after the queued clipped repaint shows the new
console text cleanly over the OpenGL grid without a synchronous full-component
snapshot. The full Cycle V2 test target and standalone target build, the
repaint-scope metric tests pass, and both focused automation fixtures pass.
