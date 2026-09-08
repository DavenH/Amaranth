# Cycle V1/V2 Differential Audio Parity

## Status

In progress. Both applications render the same scheduled offline MIDI contract,
but the exact-parity expansion on 2026-09-08 invalidated the checked-in Subbass
fixture's old “verified” status: its current four-note render no longer passes
the recorded similarity thresholds at three notes. The harness now captures
the unquantized channel-major float output, checks repeat determinism, and
reports exact sample equality separately from diagnostic gain/latency fitting.
Cycle 1 also has an end-to-end UI-keyboard-to-device fixture that requires
callback progress and finite nonzero output.

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

### Exact sample contract

WAV bytes are not the exact DSP boundary: JUCE's 24-bit conversion can differ
by one quantization step even when two Cycle 1 float renders have normalized
residual below `5.3e-7`. Each app therefore writes an optional channel-major
little-endian float sidecar directly from the offline capture buffer. Exact
parity means identical metadata and identical float payload bytes at this
boundary, before the analyzer performs alignment, gain fitting, mixdown, or
spectral transforms.

The runner can request two to five fresh application renders. Repeatability is
a prerequisite to cross-engine interpretation. Cycle V2 graph opening also
waits for background graph/DSP preparation before capture; without that wait,
effect-heavy graphs could race the first prepared-state publication and appear
nondeterministic.

Cycle 1's mature voices normally seed their per-note rasterizer RNGs from wall
clock time. The offline command now accepts an automation-only `randomSeed` and
applies it to every preallocated voice and both voice implementations before
the MIDI schedule begins. Normal realtime behavior is unchanged. With seed
`1129927500`, both Subbass and `guitar-3-g` produce byte-identical float payloads
across fresh processes in each engine.

## Representative preset ladder

Current Cycle 1 documents are exported through the running product before a
port is admitted. The converter remains the authoritative representation
translation. The first broad candidates are:

| Preset | Deterministic coverage | Current admission result |
| --- | --- | --- |
| Subbass | Time, magnitude, phase, volume/scratch envelopes | Port manifest was strict, but current notes 48–72 fail its old output thresholds; diagnostic only. |
| guitar-3-g | Time + spectral, phase pan, volume/scratch, 2x oversampling, waveshaper, IR, EQ, delay | Regenerated exactly from a live canonical export while retaining node presentation. Both engines are byte-repeatable, but MIDI 48 correlation is only `0.09498`; target the oscillator/effect stage ladder. |
| japan-drum | Two time layers, two magnitude layers, phase, volume envelope, five guide assignments | Regenerated exactly; all four guides have zero noise/offset/phase and both engines are byte-repeatable. Correlation is `0.09425`, `0.01743`, `0.03134`, and `0.01921` at MIDI 36, 48, 60, and 72. |
| Icycle | Broad synthesis/effects plus six-voice Unison | Guide noise is disabled. Current graph differs from fresh conversion in reverb size; Unison repeatability still needs an admitted pair. |
| accoustic | Broad graph including reverb | Current graph differs in morph/link state, envelope state, reverb size, and IR high-pass; do not use for DSP attribution yet. |
| organ-2 | Spectral layers, envelopes, Unison, IR, delay, reverb | Current graph differs from fresh conversion in reverb size; reverb seed parity is unresolved. |

Noise-bearing presets are deferred until both engines expose and honor the
same persisted or injected seed. “Noise level zero” alone does not exempt a
graph from the repeat-render gate.

Fresh conversion is itself not sufficient for older documents that omit
session-owned controls. Subbass, for example, does not persist its morph panel
state; Cycle 1 inherits the startup preset's morph position while the converter
must synthesize a default. The exact manifest must record and apply that state
explicitly before hashes can establish equivalent inputs.

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
8. Restore Cycle 1's sample-rate volume-envelope boundary and guard OohAah note
   transitions. Volume playback is now advanced only by `SynthesizerVoice`,
   multiplied from the existing `EnvRasterizer` playback output, and timed from
   the voice's actual sample rate. The focused fixture requires a low-energy
   first 50 ms and rejects large adjacent-sample jumps through release.
9. Audit Cycle 1's generalized voice pipeline against the legacy implementation.
   Restore inactive-volume bypass, pitch-envelope initialization, and any other
   confirmed lifecycle or DSP contracts with focused observable regressions.
   Complete: the port once stopped voices without an active volume envelope,
   left the pitch-envelope availability flag unset, used a local vector index
   as a durable layer index, and stopped held notes when edits switched between
   time-only and spectral voices. The restored code reuses the existing
   rasterizers and `stealNoteFrom()` state transfer rather than introducing a
   second playback path.
10. Replace quantized-WAV equality with a raw float capture sidecar, add exact
    payload/first-mismatch reporting, and require fresh repeat renders before
    attribution. Complete.
11. Revalidate all checked-in equivalence manifests on the current branch.
    In progress: Subbass was demoted to diagnostic after its current output
    contradicted the recorded result.
12. Admit deterministic whole-graph ports incrementally. In progress:
    `guitar-3-g` and `japan-drum` now match fresh canonical conversion exactly,
    preserve their prior presentation, and repeat byte-for-byte in both apps.
    Their severe output mismatches are now valid DSP discrepancies and make the
    oscillator/spectral stage ladder the next required slice.
13. Add deterministic seed injection/persistence at the shared render contract
    for Guide noise, Unison jitter, and reverb, then admit one fixture for each.
    In progress: Cycle 1 voice/rasterizer seed injection is complete and proves
    repeatability for the first two fixtures; cross-engine seed mapping and
    Cycle 1 reverb remain open.
14. Remove each gain, latency, scheduling, and sample-rate policy discrepancy
    from the comparison boundary until admitted fixtures require raw exact
    sample equality.

Each slice receives focused semantic tests, a refactor/style pass, and a
coherent commit before the next slice.

## Legacy Audio-Pipeline Audit

The generalized Cycle 1 pipeline was compared with
`amaranth-legacy/Audio/SynthAudioSource.cpp` and the legacy voice hierarchy.
This is a source-contract audit around the mature renderer, not a claim that
every inherited legacy behavior is correct.

| Boundary | Audit result |
| --- | --- |
| MIDI note lifecycle and sustain | Preserved; note start, note-off, release, sustain, and hard-stop ownership remain in `SynthesizerVoice`. |
| Envelope ownership | Corrected; volume advances once at sample rate, while pitch and scratch remain on the internal cycle timeline. An inactive volume layer now bypasses gain instead of terminating the note. |
| Pitch envelope | Corrected; initial availability, dynamic sampleability, and source layer identity now reach the existing pitch rasterizer. |
| Voice implementation changes | Corrected; a held note transfers its existing oscillator state between unison and spectral voices, except during release. |
| Oscillator and spectral reconstruction | Preserved through the existing voice hierarchy and shared `CycleDsp` frame primitives; the strict Subbass differential render remains the observable guard. |
| Internal/output sample-rate boundary | Preserved; synthesis and effects run at 44.1 kHz, then the existing Hermite stage converts to the device rate. Volume is intentionally evaluated at the prepared output rate. |
| Effects and master gain | Preserved; waveshaper, tube, EQ, delay, reverb, then master gain. |
| Cycle cache | Intentionally inert in both trees because the legacy implementation returns before cache generation. |

Two issues are demonstrably inherited by both trees and are therefore tracked
as legacy defects rather than port-parity regressions: carried MIDI can replay
after a zero-internal-sample block, and global scratch renders an output-block
sample count with a 44.1 kHz delta at non-44.1 kHz device rates. Neither is
changed without a representative timing fixture.

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
- Every admitted preset is byte-repeatable within both engines in fresh app
  processes.
- Exact fixtures compare the raw float payload with no alignment, gain fit,
  normalization, or quantization.
- The representative ladder covers all supported synthesis layers, envelope
  purposes, Unison, waveshaper, IR, EQ, delay, reverb, and deterministic noise.
- Every unsupported or semantically incomplete Cycle V2 feature is linked to a
  concrete preset/render in `audio-bugs.md`.

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
not evidence of a carry defect. The historical 2026-09-06 report passed, but it
is not reproducible on the 2026-09-08 merged branch. At 48 kHz, current
correlation falls from `0.98620` at MIDI 36 to `0.95695` at MIDI 72, with
gain-matched residual rising from `0.1656` to `0.2902`. The same note-dependent
trend occurs at 44.1 kHz, so output-rate conversion is not the primary cause.
The fixture remains useful for localization but no longer carries a verified
parity verdict.
