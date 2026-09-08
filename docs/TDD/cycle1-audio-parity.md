# Cycle 1 Audio Pipeline Parity

Status: in progress

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

## Test Strategy

- Schedule overlapping release and attack events in one offline render, and
  assert that both MIDI pitches are present during the overlap.
- Render enabled/bypassed scratch and phase variants in one process and compare
  time-local RMS contours and harmonic power.
- Measure early-versus-late cycle/spectral differences for Dunk2 and PWM.
- Measure high-band to harmonic-band power for the four Calming notes at 44.1
  and 48 kHz, preserving artifacts for cyclogram/spectrogram inspection.
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

## Completion Criteria

- Every reported contract has a failing-before/passing-after regression.
- Current behavior matches the corresponding legacy control flow without a
  copied DSP or rasterization implementation.
- Modified realtime paths allocate no memory, full tests pass, and each
  coherent fix is documented and committed before the next slice.

## Deletion Targets

- Remove temporary diagnostic fixtures and logging after each failure is
  localized.
- Do not retain preset-specific production branches or test-only DSP paths.
