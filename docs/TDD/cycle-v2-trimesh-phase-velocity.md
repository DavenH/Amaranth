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
