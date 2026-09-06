# Cycle V1/V2 Differential Audio Parity

## Status

Implemented. Both applications render the same scheduled offline MIDI contract,
the checked-in Subbass fixture has a strict source/graph equivalence manifest,
and the four-note differential render passes its waveform, spectrum, and
cyclogram thresholds. Cycle 1 also has an end-to-end UI-keyboard-to-device
fixture that requires callback progress and finite nonzero output.

## Goal

Render one authored preset through Cycle 1 and its explicitly validated Cycle
V2 port, then report the first meaningful audio discrepancy without confusing
preset conversion, MIDI scheduling, gain, latency, or analysis artifacts with
a DSP mismatch.

## Authoritative Implementations

- `cycle/src/App/CycleAutomation.cpp::captureAudio()` owns the established
  automation command shape: sample rate, block size, channels, duration, and
  scheduled MIDI events.
- `cycle-v2/src/Runtime/RealtimeGraphRenderer.*` owns Cycle V2 MIDI lifecycle,
  per-voice execution, stereo folding, output headroom, and block processing.
- `scripts/port_cycle_v1_preset.py` owns the supported representation
  translation from Cycle 1 canonical JSON into a Cycle V2 graph.
- `AudioHub` owns Cycle 1's live device, keyboard-state merge, selected source,
  and callback lifecycle. Cycle V2's equivalent owner is
  `StandaloneAudioEngine`.
- Cycle 1's `SynthFilterVoice` and the shared `CycleDsp` oscillator primitives
  remain authoritative for spectral-frame reconstruction semantics.

The new offline renderer is a narrow lifecycle adapter around
`RealtimeGraphRenderer`. It translates sample-offset events into its timestamped
queue and copies block output into an owned capture. It does not duplicate
voice allocation, scheduling, oscillator, spectral, effect, or mixing logic.

Live validation uses one shared callback-capture primitive below both device
owners. Each owner retains device setup, MIDI routing, and rendering; the
shared primitive only preallocates capture storage, copies callback output, and
reports callback bounds and signal metrics. It is not an alternate renderer.

## Initial Supported Preset Contract

The first parity fixture is intentionally narrow:

- one active time layer, magnitude layer, and phase layer, with empty spectral
  layers treated as bypass rather than zero-valued operands;
- Unison disabled, leaving one deterministic oscillator lane;
- fixed morph controls;
- deterministic or disabled Guide noise;
- at most one deterministic volume and scratch envelope;
- no pitch-envelope motion;
- post-oscillator effects disabled;
- no external audio resources.

The converter emits a machine-readable equivalence manifest containing every
field used by this subset. Unsupported active layers, modulation, effects,
resources, or nondeterministic state are errors, not ignored differences. The
manifest records source paths, destination node/parameter paths, and normalized
values so a paired render cannot begin until the translation validates.

## Shared Offline Render Contract

Both applications consume the same declarative request:

- sample rate and block size;
- channel count and duration;
- ordered note-on, note-off, controller, channel-pressure, and all-notes-off
  events expressed in samples or milliseconds;
- MIDI channel, note, velocity/value, and deterministic ordering.

Cycle V2 renders the request through a prepared `RealtimeGraphRenderer` in
successive blocks. The graph is prepared once, events are queued once with
sample-derived timestamps, and the last partial block is copied without
changing the requested duration. The result is stereo WAV data plus metrics and
the render parameters used.

## Differential Analysis

The external comparison aligns only integer latency. For periodic ambiguity it
chooses the smallest absolute lag within 0.0001 correlation of the maximum. It
reports that alignment and the constant gain fit separately but does not
time-warp, filter, normalize each cycle, or alter either render.

For the stereo mixdown it reports:

- peak, RMS, mean/DC, and finite-sample status;
- fundamental frequency, magnitude, and phase at the requested MIDI note;
- harmonic magnitude and phase through the available Nyquist range;
- interharmonic/noise energy;
- normalized residual, spectrum difference, and mean-cycle difference after
  the single reported latency and gain fit;
- a phase-folded cyclogram consistency summary.

Log-spectrum comparison uses an -80 dB floor relative to the reference peak so
PCM quantization noise and numerical near-silence cannot dominate the score.

The report keeps raw WAV paths, preset-equivalence manifest, render request,
and analysis parameters together. A failed equivalence check prevents an audio
parity verdict.

## Diagnostic Stage Ladder

Final-output mismatch is followed down these existing product boundaries:

1. time-mesh cycle;
2. FFT magnitude and phase;
3. post-layer magnitude and phase;
4. reconstructed fixed IFFT frame;
5. pitch-clocked cyclic reconstruction;
6. post-effect output.

Cycle V2 graph probes expose authored graph boundaries. Cycle 1 needs a narrow
diagnostic export at equivalent mature boundaries before a stage can claim
sample parity. Preview products are not substitutes for audio products.

## Negative Boundaries

- Do not compare Cycle 1's scheduled stereo render with Cycle V2's current
  mono diagnostic block and call the result DSP parity.
- Do not treat a successfully parsed port as proof of semantic equivalence.
- Do not ignore active unsupported layers or effects during conversion.
- Do not compensate fractional delay, pitch drift, phase drift, or spectral
  shaping in the analyzer.
- Do not copy Cycle 1 oscillator or effect algorithms into the harness.
- Do not bless manually adjusted Cycle V2 presets as exact ports without a
  regenerated equivalence manifest.
- Do not treat offline `processBlock()` output as proof that the standalone
  audio device is open or that UI keyboard events reach its callback.

## Implementation Slices

1. Add and test the Cycle V2 scheduled offline renderer around
   `RealtimeGraphRenderer`.
2. Upgrade Cycle V2 `captureAudio` to the Cycle 1 command shape and stereo WAV
   output.
3. Add strict converter validation and equivalence-manifest generation for the
   supported subset. Complete.
4. Add the paired app runner and deterministic WAV analyzer. Complete.
5. Author/export the minimal Cycle 1 spectral fixture, port it through the
   strict converter, and check both artifacts in. Complete using the existing
   `subbass.cyc` factory source and a generated Cycle V2 graph.
6. Add stage capture only where the first final-output discrepancy requires it.
   No stage capture was needed: correcting the legacy MIDI reference produced
   final-output parity and disproved the apparent reconstruction fault.
7. Exercise Cycle 1's actual UI-keyboard-to-device path with callback capture.
   Share the capture mechanics with Cycle V2, and require callback progress plus
   finite nonzero stereo output from a held keyboard note. This exposed and
   corrected `AudioSourceProcessor` constructing a zero-channel realtime view
   whenever JUCE supplied the usual zero `startSample`. Cycle 1 now requests no
   unnecessary input channels and the Subbass live fixture passes through the
   real device callback.

Each slice receives focused semantic tests, a refactor/style pass, and a
coherent commit before the next slice.

## Completion Criteria

- One command renders the validated preset pair through both standalone apps.
- Both renders use identical sample rate, block partition, channels, duration,
  MIDI schedule, and deterministic source state.
- Unsupported preset content blocks comparison with a focused diagnostic.
- The report distinguishes preset-equivalence, render, and audio-analysis
  failures.
- The initial spectral fixture has checked-in source, port, manifest, and
  expected comparison thresholds.
- A mismatch can be localized to the earliest available stage without using a
  preview approximation.
- Cycle 1's UI keyboard produces finite, non-clipping audio through its real
  standalone device callback without requiring an input device.

## First Differential Result

The command below renders MIDI notes 36, 48, 60, and 72 at 48 kHz through both
standalone products and writes a self-contained report directory:

```sh
python3 scripts/compare_cycle_audio.py \
    scripts/fixtures/cycle-subbass-audio-equivalence.json
```

The first run exposed three harness-invalidating defects before comparison:

- Cycle 1 did not initialize its existing Hermite sample-rate converter when
  automation selected 48 kHz and crashed in `CircleBuffer::write()`.
- Cycle V2 compiled the Voice Context octave but did not pass it to prepared
  oscillator regions. Subbass therefore rendered one octave too high.
- Cycle 1's legacy pitch helper defines A440 as MIDI 81, while Cycle V2 uses
  standard MIDI 69. Preserving only the preset's displayed octave therefore
  left Cycle V2 one octave above the legacy audio. The converter now records
  and applies this reference-note translation explicitly.

The earlier apparent half-frequency component was this reference-note mismatch,
not evidence of a carry defect. After correcting it, the 2026-09-06 report at
`/private/tmp/cycle-subbass-parity-corrected/comparison.json` passes all four
notes. Correlation is 0.99990 or better, gain-matched normalized residual is
0.0143 or better, log-spectrum RMSE is 0.05 dB or better, and mean cyclogram
difference is 0.0115 or better. The fit also consistently reports Cycle V2's
intentional fixed output headroom as approximately 18.06 dB below Cycle 1.
