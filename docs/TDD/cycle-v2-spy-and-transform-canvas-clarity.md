# Cycle V2 Spy and transform canvas clarity

## Status

Implemented.

## Design

The graph semantic resolver owns domain-to-render-style defaults. A Spy showing
the FFT of a time signal must render with the spectrum preview's domain,
scale, and spectral colour material, while an untoggled Spy continues to use
its source output's authored semantics. `SignalProbeCanvas` translates only
this display-domain difference; `NodePreviewRenderer` and
`TrimeshRenderProfile` remain the raster and palette authorities.

`SignalProbeCanvas` owns cable markers and tethers. Numbered markers are a
relic of the deleted Spy rail: use small colour-coded connection dots instead.
The cable path remains the anchor. At its attachment point, the tether's first
tangent is perpendicular to the cable tangent and chosen to point toward the
Spy card. The card-side tangent can retain the current soft approach. This is
constant work per Spy and does not alter the graph or tap DSP.

`NodeDefinition` owns FFT/IFFT natural size. Reduce both from 180 × 178 to
144 × 142 world units (about 20%), preserving enough height for their two-port
side. `NodePreviewRenderer` owns the inner glyph bounds; centre the Fourier
symbols at roughly the Add/Multiply symbol size. Existing preset positions
remain unchanged while sizes follow their node definitions on load.

The changed policies have one owner each. No canvas code duplicates FFT,
spectral normalization, graph editing, or signal processing. Delete the old
number glyph and fixed upward tether control point from the Spy paint path.

## Verification

- `cycle-v2-agent-spy-transform-visuals.json` opens Baroque Flute, asserts
  both transform dimensions and the Spy card, and captures the canvas.
  A local copy with `frequencyView=true` confirms the Spy uses the warm
  spectral palette. The preview statistics report the source domain (`Time`),
  so they do not assert the displayed frequency view.
- The tether geometry test covers horizontal cables with Spies on both sides
  and vertical cables with Spies on both sides. Focused Spy marker, transform
  size, and serializer tests pass (36 assertions).
- `SignalProbeCanvas` owns cable attachment geometry, markers, and card paint;
  it relies on `NodeCanvasScene` for cable paths and `GraphPresentationFacts`
  for preview data. `GraphRenderSemanticResolver` alone maps a display domain
  to a default render semantic. `NodeDefinition` owns natural node size, while
  `NodePreviewRenderer` places the transform icon using `NodeIconRenderer`.
  The latter two files exceed the architecture review threshold but this slice
  adds no new policy branch or responsibility to either. The old numbered
  marker and fixed upward departure are deleted.
- The broad `[probe]` test filter still has three known failures in runtime
  preview and preset expectations, already recorded in `audio-bugs.md` and
  `ui-bugs.md`. `honerism-3.cyclegraph` currently contains `probe2`, whereas
  its test expects `probe`. These are outside this canvas presentation slice.
- Debug and Release `CycleV2` builds pass. The focused CTest selection passes
  all four cases, and the eight-command canvas fixture passes. The architecture
  audit, diff whitespace check, and hot-loop math scan pass. Clang-tidy reports
  only existing warnings in the touched files.
