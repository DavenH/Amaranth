# Cycle V2 Spy detail time-span parity

## Status

Implemented.

## Design

`GraphPreviewExecutor::periodRowsForMidiNote` is the authoritative pitch-period
calculation. The compact preview renderer captures at least 512 source frames
and then reduces the rendered grid to at most 512 rows. The expanded capture
currently requests only the pitch period; for high notes that is 128 or 256
frames, so opening a Spy changes the visible time span even though the note and
signal address are unchanged.

Put the shared source-frame policy next to the period calculation, and use it
for both compact and expanded captures. Keep the existing expanded 512-column
grid and 512-row display cap. `NodeCanvas` only requests the shared frame
count; it does not choose a DSP window. Preserve the direct capture API, which
accepts an explicit frame count, for callers that intentionally inspect one
period. Align expanded display-domain semantics with the compact Spy so a
frequency toggle also keeps the same spectral scale and material.

The source-frame calculation is constant time. The expanded render retains its
existing 512-row cap and per-probe traversal; compact per-gesture work and
unrelated graph traversal do not change. Delete the separate compact-only
source-frame choice and the detail view's implicit one-period choice.

`NodeCanvas.cpp` is already above the architecture size trigger. This slice
changes only its existing detail-open orchestration. The ongoing extraction
plan is `cycle-v2-node-canvas-orchestrator.md`; no capture or render policy is
added to that file.

## Verification

- At MIDI 72, a semantic test compares compact and expanded time-domain Spy
  values after selecting the same 512-frame source span. The expanded grid has
  512 columns and 512 rows; its mean difference from the compact grid is below
  the existing 0.02 tolerance. Explicit one-period capture remains available
  through the direct API.
- `cycle-v2-agent-spy-detail-time-span.json` passes all 16 commands: high-note
  regular Spy detail has 512 × 512 samples, and the output Spy spectrum has
  257 frequency rows from the same 512 source frames. A local time-view output
  Spy fixture also passes with 512 detail rows. The updated legacy Stengah Spy
  detail fixture passes all 13 commands.
- The old `SignalProbeDetailView::resolutionForMidiNote` shortcut is removed;
  pitch-period tests now address `GraphPreviewExecutor` directly. Source-frame
  eligibility formerly lived separately in compact preview rendering and
  detail opening; it now has one decision site in `GraphPreviewExecutor`.
  Display-domain style selection for card/detail formerly had two decision
  sites; it now has one in `GraphRenderSemanticResolver`.
- `NodeCanvas.cpp` is 3114 → 3113 lines and remains orchestration around the
  existing capture API. `PresentationPreviewRenderer.cpp` is 237 → 236;
  `GraphPreviewExecutor.cpp` is 639 → 643; `SignalProbeDetailView.cpp` is
  101 → 97. No graph copy, serialization, resource preparation, or new
  unrelated-node lookup occurs on detail opening. The touched files keep their
  prior responsibilities and dependency direction.
- Four focused CTest cases pass, as do the two Spy reference cases. Debug and
  Release app builds, architecture audit, diff/style checks, and the focused
  canvas fixtures pass. The phase-velocity agent fixture could not run because
  its source `.cyclegraph` is absent; the issue is recorded in `ui-bugs.md`.
