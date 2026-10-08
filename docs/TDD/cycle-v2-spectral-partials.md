# Cycle V2 spectral partials

Status: Implemented

## Design

Cycle 1's `Spectrum2D::drawPartials` is the visual reference: it reads the
current 3D grid column, uses that column's MIDI note to obtain the harmonic
log ramp, and switches from bars to a filled curve as horizontal spacing
shrinks. Cycle V2's authoritative grid is `TrimeshPanelDataSource::panelColumns`.
Each column owns the same displayed height values supplied to the 3D panel.
`LogRegions::getDefaultRegion` owns the harmonic positions for both panels.

The data source copies only the selected column under its grid lock. The 2D
panel receives the primary axis morph position from `TrimeshPanelBridge`, then
draws the partials in its existing `preDraw` pass beneath the editable curve.
The panel does not render DSP, copy the grid, or change model state. Morph
updates select a different column without rebuilding the surface. The nearest
column matches Cycle 1's selection and keeps the overlay exactly on sampled
3D grid values.

The new drawing uses separated bars with rounded value tips and square ends at
the baseline, slim bars as spacing narrows, and one filled contour at subpixel
spacing.
This replaces Cycle 1's individual cap and side stroke styling while keeping
its data and frequency placement. No compatibility adapter or duplicate DSP
path is introduced.

## Completion criteria

- Magnitude and phase spectral 2D panels draw partial heights from the 3D
  grid column at the primary axis morph position.
- Horizontal placement follows the selected column's log harmonic ramp,
  including key scale columns with different MIDI notes.
- Wide partials occupy about 60% of each harmonic slot, have rounded value
  tips and square ends at the baseline; dense partials resolve to one filled
  contour.
- Time 2D panels remain unchanged.
- A focused semantic test proves selected column values and note, while a
  focused UI fixture and screenshot prove the overlay appears in the editor.

## Verification

- `CycleV2_tests`: selected columns match the 3D data for magnitude and phase;
  sampling at different morph positions changes the selected MIDI note without
  a surface rebuild.
- `scripts/fixtures/cycle-v2-agent-spectral-partials.json`: the bipolar
  magnitude editor remains responsive through a morph gesture. The resulting
  screenshot shows separated low partials and a merged high-frequency contour.
- Existing surface and domain mapping tests pass. The touched C++ files remain
  below the repository's architecture size review thresholds.
