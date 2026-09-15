# Cycle V2 Spy Oscillator Traversal Parity

## Status

Implemented 2026-09-15.

## Problem

A Spy downstream of a compiled oscillator region observes the flat diagnostic
processor chain rather than the signal semantics of the prepared oscillator.
Consequently a looping scratch Envelope is sampled as one linear display pass,
and configured Unison lanes are absent from time-domain Spy grids. The
`pwm-lead` preset exposes both failures: its scratch-driven surface becomes
constant partway across the Spy and its eight voices collapse to one.

## Authoritative Implementations

- Loop, sustain, release, and per-voice Envelope advancement:
  `Rasterization::EnvelopePlaybackEngine`.
- Cycle 1 visual pitch and Unison column composition:
  `Cycle::Rasterization::UnisonPhaseColumnRenderer`.
- Cycle 2 audio lane configuration and phase trajectory:
  `CycleDsp::UnisonCore` and compiled `VoiceContext` state.
- Probe transport and rendering remain unchanged in
  `GraphPreviewExecutor` and `SignalProbeRail`.

The implementation must extract the reusable column-mixing primitive from the
Cycle 1 renderer. Cycle 2 may translate a captured column-major traversal grid
and compiled Voice Context values into that primitive, but it must not copy the
phase rotation, interpolation, or lane accumulation algorithm.

## Design

Envelope diagnostic traversal starts a private graphic playback cursor and
advances it through the shared playback engine for the normalized preview
duration. This cursor is separate from realtime voice state and honors the
authored loop exactly.

The oscillator-region diagnostic boundary keeps the existing flat processors
for their authoritative mesh, FFT, IFFT, and envelope grid transformations.
At the region materialization step, a domain-owned traversal renderer applies
compiled pitch and Unison phase to the time grid with the extracted shared
column mixer. The materialized audio block continues to come from the prepared
oscillator region; downstream diagnostic processors therefore receive both the
real audio block and the corresponding two-dimensional traversal observation.

Only the materialization output receives lane composition. Raw time,
magnitude, and phase probes inside a spectral recipe continue to describe
their exact pre-materialization graph values.

## Boundaries And Complexity

- No graph search, serialization, or durable mutation occurs while rendering.
- Realtime execution is unchanged; traversal composition runs only when
  diagnostics are captured.
- Trimesh/FFT/IFFT traversal rasterization runs once per column, independent of
  Unison order. The subsequent lane-composition pass is
  `O(columns * rows * lanes)` and performs only phase rotation, interpolation,
  gain, and accumulation of the already-computed waveform, matching Cycle 1.
- Probe extraction and paint remain passive copies/presentation of the source
  observation.
- The adapter stores no Envelope, Trimesh, FFT, or Unison domain state machine.

## Verification

- A looping scratch Envelope traversal repeats through the shared playback
  policy instead of becoming constant after the sustain boundary.
- A direct Trimesh oscillator Spy changes when multi-voice Unison is enabled,
  includes phase drift across columns, and returns to the single-lane grid when
  Unison is bypassed.
- A spectral oscillator Spy after IFFT/volume receives the same composed grid,
  while probes on raw spectral branches remain unchanged.
- `pwm-lead` retains changing scratch traversal through the full Spy width and
  exposes its eight-voice composition.
- Focused Catch2 tests, `git diff --check`, hot-loop review, and a production
  Cycle V2 capture pass.

## Completion Criteria

- Spy grids downstream of an oscillator materializer include the compiled
  pitch and Unison lane behavior with Cycle 1 visual parity.
- Looping scratch traversal is produced only by the shared Envelope playback
  engine.
- Diagnostic audio uses the prepared oscillator result even when internal flat
  processors remain necessary for traversal products.
- No duplicate phase-mixing or Envelope loop implementation remains.

## Implementation Evidence

- `EnvelopeSignalProcessor` now advances a private
  `EnvelopePlaybackEngine` cursor for diagnostic columns.
- `UnisonColumnMixer` owns the phase rotation, interpolation, gain, and
  accumulation used by both the Cycle 1 visual renderer and Cycle V2's
  oscillator-materialization traversal renderer.
- The PWM Lead regression verifies the looping scratch Envelope and its
  downstream Trimesh through the second half of the traversal, then compares
  the eight-voice Spy against an actual Unison bypass while holding the raw
  magnitude grid constant.
- Focused Envelope, PWM Lead, and shared Unison tests pass. Cycle 1 tests and
  the Cycle V2 standalone target compile. The PWM Lead automation fixture
  passes and captures both compact and expanded Spy views at
  `/private/tmp/cycle-v2-pwm-lead-spy.png` and
  `/private/tmp/cycle-v2-pwm-lead-spy-detail.png`.
