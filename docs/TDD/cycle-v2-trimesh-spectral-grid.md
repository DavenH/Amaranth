# Cycle V2 Trimesh spectral grid and surface resolution

Status: implemented (2026-09-18).

## Contract and sources

Cycle 1 `Spectrum2D::drawBackground` uses the selected MIDI key's
`LogRegions` ramp for harmonic ticks. `Spectrum3D::updateBackground` uses
the same ramp for horizontal harmonic guides, while `Panel3D` adjusts each
spectral column to its MIDI key. Cycle V2 already uses `LogRegionMapping`
for spectral column length and `TrimeshGridwiseDsp` for mature mesh
rasterization. These are the authoritative implementations; the panel layer
only translates the resulting harmonic positions to background geometry.

The selected note comes from preview state when no key-scale axis is mapped.
For a mapped axis, the node's current morph parameter supplies the panel note.
Keyboard preview changes publish that morph parameter; direct morph edits must
retain the edited value and move the panel's harmonic grid with it. The
key-scale axis comes from the compiled modulation mapping and controls whether
pitch spans the 3D x axis.
Authored presets can omit explicit Voice Context signal edges; the preview
pitch resolver uses the sole Voice Context in that case and declines to guess
when multiple contexts exist.
The 2D background must show harmonics up to the range limit for that note.
The 3D background must show each harmonic at its pitch-dependent display
height, ending its trace when that harmonic leaves the range. Expanded 3D
surfaces should sample at least one column per logical pixel of panel width;
compact previews retain their smaller fixed budget.

The visual geometry is message/GL-thread work. No graph commands or durable
resources are involved. The surface uses the existing grid DSP and a direct
output buffer, avoiding a separate payload allocation for every column.
For a key-scale primary axis, the surface column's morph and MIDI key must
both follow its x coordinate. The slice note may change with the mapped morph
parameter, but it must not set the spectral sampling ramp for every surface
column. Each column samples the mature LogRegions ramp for its own key, and
Panel3D places those samples using that same ramp.
Cycle 1's rendering cost scaled with panel width. Cycle V2's former 96-column
expanded view was cheaper but visibly under-sampled; the target restores the
same width scaling and keeps single-node invalidation.

## Completion

- [x] 2D backgrounds consume the full cached LogRegions ramp for the preview
      key. Existing bridge tests cover the differing region sizes at MIDI 48
      and 72; the guide renderer uses the same ramp as Cycle 1.
- [x] 3D harmonic traces follow each column's key when the mapped axis spans
      x. A fixed key produces horizontal traces. The native Organ 4 capture
      shows the non-linear pitch traces.
- [x] Expanded surface columns track panel width. The native fixture reports
      586 columns for a 586-pixel 3D panel, with 1699 spectral rows and MIDI
      keys 20–127 across the columns.
- [x] The organ fixture completed its expanded redraw and OpenGL diagnostics
      without a failure. The direct-buffer renderer reuses the mature grid
      sampler and avoids a payload allocation for each column.
- [x] Focused Trimesh and pitch-resolver tests, native UI capture, and style
      review passed.
- [x] A mapped morph rail keeps its edited position through repeated movement,
      updates fixed-pitch spectral columns and harmonic backgrounds, and returns
      to its original position and pitch on undo.
- [x] Changing a key-scale primary-axis morph leaves the 3D column samples and
      harmonic guides unchanged; each column uses the same note for sampling,
      row count, and display placement.
- [x] Expanded local morph movement keeps the expanded pixel-width column
      count instead of restoring the 96-column compact preview.
- [x] Compact pitch-spanning previews resample each column's valid harmonic
      rows into the full display height; the expanded panel retains its
      per-column harmonic samples and placement.

Evidence: `scripts/fixtures/cycle-v2-agent-organ-harmonic-grid.json`,
`/private/tmp/cycle-v2-organ-harmonic-grid-report.json`, and
`/private/tmp/cycle-v2-organ-harmonic-grid.png`.
The red-axis follow-up is covered by the direct per-column harmonic parity and
surface-invariance assertions in `TestTrimeshNodeDsp.cpp`, plus
`scripts/fixtures/cycle-v2-agent-red-axis-pitch-columns.json`; its native
report and capture are `/private/tmp/cycle-v2-red-axis-pitch-report.json` and
`/private/tmp/cycle-v2-red-axis-pitch.png`. The Organ preset baseline capture
at `/private/tmp/cycle-v2-red-axis-before.png` shows the same 3D surface and
harmonic guides before the red slice moves; the 2D slice changes as expected.
The compact pitch preview has a focused high-note row assertion in
`TestTrimeshNodeDsp.cpp` and a native Organ screenshot at
`/private/tmp/cycle-v2-compact-pitch.png` from
`scripts/fixtures/cycle-v2-agent-trimesh-compact-pitch-preview.json`.
