# Cycle V2 Trimesh Phase Velocity

Status: Complete

## Contract

The Trimesh `signalType=spectralPhase` retains its phase output domain. Its
`phaseMode` parameter is `absolute` by default or `velocity`. Absolute keeps
the mature per-frame phase offset. Velocity interprets the signed raster as
cycles per second at Width 1x, applies existing gain, Width, and harmonic
scaling, and integrates each bin over elapsed voice time. The first frame of a
note contributes zero. A note-on or processor reset clears the integrator.
The authored Trimesh editor surface shows velocity; downstream live capture
shows the accumulated phase.

## Spy traversal contract

A Spy attached directly to a Velocity Trimesh shows the integrated phase over
the traversal window, starting at zero in its first column. A constant
positive or negative authored rate therefore produces a phase ramp with the
corresponding direction, modulo one turn. Each following column integrates
its sampled velocity for the same voice-time step used by the oscillator
traversal, `voiceDurationSeconds / (columns - 1)` when inside a voice. The
diagnostic frame count controls vertical resolution, not traversal duration.
The diagnostic traversal uses a separate accumulator; it must not advance the
realtime voice state or replace the Trimesh editor's authored-rate surface.

## Audio prediction

The first prepared spectral frame after note-on has zero added phase. Later
frames rotate each affected harmonic by its accumulated phase, changing the
rendered waveform while the Trimesh phase layer leaves spectral magnitudes
alone. The complete onset audio block may already differ from a zero-phase
render because the oscillator renders subsequent frames within that block.
The phase source resets on note-on, so the rotation starts again from zero.

## Ownership and complexity

The existing Trimesh rasterizer owns mesh sampling and guide behavior. A small
Trimesh phase accumulator owns only per-bin running phase and bounded wrapping.
The prepared spectral source and general processor adapt their timing to it.
State is allocated at preparation and updates cost O(active bins), with no
allocation on the audio path. Existing Phase remains the compatibility default.

## Completion

- [x] Parameter, editor toggle, persistence, undo, and legacy default.
- [x] Both Trimesh audio paths integrate and reset with voice lifecycle.
- [x] Static editor presents authored rate; live downstream phase reflects state.
- [x] Focused semantic and interaction tests, UI capture, architecture and style review.
- [x] Spy traversal integrates rate across columns and matches its sampled operands.
- [x] Compact and detailed downstream time-domain Spies retain phase drift
      when their row counts differ.

## Verification and architecture review

The prepared source test covers elapsed-time accumulation, reset, JSON round
trip, and audible downstream difference from Absolute; the editor test covers
toggle publication and undo. `ctest` passed four focused cases. The 12-command
agent fixture passed, and `/private/tmp/cycle-v2-trimesh-phase-velocity-os.png`
shows the Velocity toggle and authored phase surface without clipping.

The phase accumulator owns integration and bounded wrapping, while both audio
renderers own only timing adaptation. `NodeDefinition.cpp` remains a cohesive
definition registry despite its 819 lines; this change adds one parameter
declaration there. The broad Trimesh and spectral filters include unrelated
open failures recorded in `ui-bugs.md` and `audio-bugs.md`.

The Spy regression compares every phase bin and column with the Absolute
mode's sampled rate integrated over the diagnostic window. It failed with a
flat zero grid before the fix and passes after it. The traversal integration
uses a separate prepared accumulator in `TrimeshNodeAudioProcessor.cpp`; it
does not alter the voice accumulator. Five focused tests pass, including the
prepared source, note cadence, toggle, and Spy contracts. The runtime file is
515 lines, below the architecture size trigger; it still owns only Trimesh
audio rendering, traversal rendering, and their timing translation. The
Trimesh rasterizers remain the sole mesh sampling owners, with no copied
policy or deletion target. The architecture audit and diff check pass.

The audio regression renders the preset in Velocity, Absolute, and zero-phase
control modes. Velocity has nonzero output and differs from both controls; its
first 256-sample block and later blocks differ from the zero-phase control.
The separate prepared-source assertions verify zero phase on the first frame,
elapsed-time accumulation, and reset. An initial prediction that the *whole*
first audio block would equal zero phase failed; frame rendering within that
block explains the difference. All five focused phase tests pass.

`content/presets/phase-velocity-test.cyclegraph` is the manual listening and
Spy fixture. It routes a stronger Velocity Phase Trimesh into the established
bright lead graph, with Phase Velocity and Audio Output probes. The focused
agent fixture opens and compiles it, verifies the selected mode and live Spy,
and captures a C4 note with peak 0.183 and RMS 0.037 at 48 kHz.

The traversal-time regression initially failed: the first Spy step contained
0.0003 radians where voice-time integration predicted 0.094 radians. The
oscillator traversal renderer already uses the compiled voice duration, so
`GraphAudioExecutor` now passes that duration into the prepared Trimesh
diagnostic context. This is one preparation-time lookup; the realtime block
path still performs no graph search. The Trimesh processor integrates its
sampled rate across that duration with its separate preview accumulator.
For nodes outside a voice region, diagnostic traversal retains block duration
as a fallback. There is no parallel lifecycle or mesh-sampling implementation.

The phase-grid regression, downstream inverse-FFT test at 512-row compact and
128-row detail resolutions, and all five focused phase tests pass. The
15-command agent fixture opens and closes the detail view at 512 columns and
128 rows. An OS screenshot at
`/private/tmp/cycle-v2-phase-velocity-after-screen.png` shows distinct
time-domain drift in the expanded view at a wide Width setting. The production
diff changes 1 context field, 10 lines of preparation-time translation, and
the Trimesh diagnostic time step. `GraphAudioExecutor.cpp` has 1,168 lines;
its existing responsibilities are processor preparation, buffer binding, and
graph execution. The new lookup is confined to its preparation boundary,
reusing the compiled context and leaving traversal rendering in its existing
owners.
