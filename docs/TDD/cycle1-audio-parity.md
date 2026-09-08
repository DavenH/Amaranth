# Cycle 1 Audio Pipeline Parity

Status: implemented

## Objective

Restore Cycle 1 voice lifecycle, spectral enablement, scratch/time evolution,
and filtered-spectrum behavior to the audible contracts of the legacy project.
The work is driven by representative bundled presets rather than synthetic
approximations.

## Authority And Boundaries

- `/Users/daven/repos/amaranth-legacy/Audio/Voices` is authoritative for voice
  allocation, note lifecycle, cycle composition, envelope advancement, and
  spectral phase/magnitude interaction.
- The current shared rasterization and FFT implementations remain authoritative
  below their adapter boundaries. Cycle 1 may translate lifecycle, mesh,
  property, and buffer types but must not duplicate their domain algorithms.
- MIDI allocation belongs to `Synthesizer`/JUCE, per-note rendering belongs to
  `SynthesizerVoice` and `CycleBasedVoice`, and rasterizer sampling remains in
  the shared rasterization layer.
- Realtime preparation must remain allocation-free during rendering. Any new
  capacity must be installed through the existing preparation lifecycle.

## Reported Contracts

1. A new note can begin while an earlier note is completing its release.
2. BrightLead3 changes audibly when its scratch envelope is enabled or bypassed.
3. Disabling a valid phase layer does not disable or silence an independently
   enabled magnitude layer.
4. Calming contains only its intended low harmonic content across A1, G1, F1,
   and E1; filtered notes do not acquire unrelated high-frequency buzzing.
5. Dunk2 evolves away from its short initial time cubes as voice time advances.
6. PWM audibly changes pulse width over the note instead of rendering one
   stationary cycle.
7. Hard-panned phase layers produce distinct left and right signals; Acidic's
   two phase layers must not collapse to dual mono.
8. With its waveshaper bypassed, Acidic's authored declick setting brings the
   signal continuously to silence at note-off.
9. Ping produces audible output with its authored unison configuration, not
   only when unison is bypassed.
10. A volume envelope with no authored release remains at its held value while
    the optional declick ramp owns voice retirement; MIDI block position must
    not shorten that ramp.

## Test Strategy

- Schedule overlapping release and attack events in one offline render, and
  assert that both MIDI pitches are present during the overlap.
- Render enabled/bypassed scratch and phase variants in one process and compare
  time-local RMS contours and harmonic power.
- Measure early-versus-late cycle/spectral differences for Dunk2 and PWM.
- Measure high-band to harmonic-band power for the four Calming notes at 44.1
  and 48 kHz, preserving artifacts for cyclogram/spectrogram inspection.
- Measure Acidic's steady-state side-to-mid energy and its worst note-off
  discontinuity with waveshaping bypassed.
- Render Ping both with authored effects and with unison bypassed. The authored
  render must remain audible, while the bypass render localizes any failure to
  unison voice composition rather than its time mesh or volume envelope.
- Keep tests at the audio output boundary. Add focused unit coverage only for a
  shared DSP invariant needed to localize a failure.

## Progress

- Spectral magnitude/phase validity is independent of the visible editor
  domain, with a magnitude-only output regression.
- Ordinary and filtered time-domain layers now receive the sampled scratch or
  linear voice time as the rasterizer's current morph position.
- The current note period now determines its interpolation cadence, and the
  time-evolution integration compares first and second PWM notes in one
  process.
- Scratch-enabled/bypassed BrightLead3 and PWM, plus early/late Dunk2 and PWM,
  have output-level regressions.
- Realtime and visual inverse FFTs now share the legacy harmonic-tail clearing
  rule. Calming's A1/G1/F1/E1 matrix guards the resulting high-band limit.
- A live-device pointer regression holds C3 into release and starts A4 from the
  same on-screen keyboard. It verifies that the second pitch dominates while
  the first note still has a residual tail.
- Follow-up reproduction shows Acidic is exactly dual mono despite phase pans
  of 1.0 and 0.0. Ping is silent with its authored unison enabled and audible
  with only unison bypassed; delay enablement does not affect the failure.
- Phase accumulation now targets the active note-sized phase views, preserving
  distinct hard-panned offsets on Accelerate instead of rejecting a mismatched
  full-allocation add.
- Unison count changes now resize cycle storage and prepare both oscillator
  rasterizers under the audio lock. Ping's authored seven-voice configuration
  therefore reaches already-prepared retained cycle states at note start.
- Acidic's authored declick path is guarded at 50, 150, 400, and 800 ms note
  lengths. Its existing note-off and terminal fades pass without additional
  DSP logic.
- Anasound's no-release volume envelope is guarded at the same note lengths.
  Its output must remain present through the middle of the 10 ms declick tail
  even when note-off lands near the end of a processing block.

## Completion Criteria

- Every reproduced defect has failing-before/passing-after evidence. Reported
  paths that are already continuous retain representative regression coverage.
- Current behavior matches the corresponding legacy control flow without a
  copied DSP or rasterization implementation.
- Modified realtime paths allocate no memory, full tests pass, and each
  coherent fix is documented and committed before the next slice.

## Completion Evidence

- All nine reported contracts have focused audio-output or live-keyboard
  regressions.
- Acidic's side/mid RMS ratio is `2.45` at 44.1 kHz and `2.40` at 48 kHz.
  Ping's authored render has `0.0689` steady RMS at both rates. The Acidic
  declick matrix keeps note-off second differences below `0.000107` and
  terminal steps below `0.000016`.
- Anasound's no-release declick remains audible for `10.10` to `10.13` ms at
  48 kHz across 50, 150, 400, and 800 ms notes. Terminal steps are below
  `1e-8`; the same matrix passes at 44.1 kHz.
- The parity changes reuse `MorphPosition::withTime`, `SpectralLayerCore`, the
  existing rasterizer preparation lifecycle, and the legacy layer-selection
  rule; no preset-specific production path was added.
- `ctest --test-dir build/standalone-debug --output-on-failure` passes all 852
  discovered tests. The standalone Cycle build and each focused integration
  script pass at their documented sample rates.

## Deletion Targets

- Remove temporary diagnostic fixtures and logging after each failure is
  localized.
- Do not retain preset-specific production branches or test-only DSP paths.
