# Cycle V1/V2 Differential Audio Parity

## Status

In progress. Both applications render an equivalent scheduled offline MIDI
contract, including Cycle 1's nonstandard MIDI reference translation. A
2026-09-08 audit found that the paired runner recorded that translation but did
not apply it, invalidating the low correlations reported earlier that day. The
harness now captures the unquantized channel-major float output, checks repeat
determinism, and reports exact sample equality separately from diagnostic
gain/latency fitting. Cycle 1 also has an end-to-end UI-keyboard-to-device
fixture that requires callback progress plus finite nonzero output.
Prepared Cycle V2 spectral frames now consume live controls and rerasterize at
synthesis-cycle frontiers. Raw, allocation-free capture now observes equivalent
mature spectral-frame boundaries in both engines, and the paired runner reports
the first unequal stage without using preview products. Filter Saw localizes
the first material evolving mismatch to magnitude-layer processing.

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

The requested note is expressed in Cycle V2's standard MIDI reference. The
runner subtracts the manifest's `legacyMidiReferenceOffset` when constructing
the Cycle 1 event and records both scheduled notes in the result. This boundary
translation is separate from a preset's authored octave control.

Cycle V2 renders the request through a prepared `RealtimeGraphRenderer` in
successive blocks. The graph is prepared once, events are queued once with
sample-derived timestamps, and the last partial block is copied without
changing the requested duration. The result is stereo WAV data plus metrics and
the render parameters used.

### Output-rate parity boundary

Cycle 1's `SynthAudioSource::processBlock()` is authoritative for compatibility
renders above 44.1 kHz: it maps each device block and its MIDI offsets onto a
44.1 kHz synthesis clock, renders the existing voice/effect pipeline there,
then applies the existing stateful `HermiteState` converter per channel. The
block clock is now a shared `CycleDsp::InternalRateBlockAdapter`; Cycle 1 reuses
its MIDI facade unchanged, while Cycle V2's offline adapter translates its
timestamped event type at that boundary. The Hermite DSP remains the shared
library implementation.

This compatibility policy is explicit and limited to differential offline
captures. Cycle V2's production renderer continues to synthesize natively at
the device rate. The adapter may translate block sizes, event offsets, buffer
ownership, and converter lifecycle; it must not contain oscillator, envelope,
effect, or graph behavior. Its stable end state is a shared block clock and
resampler with the two renderers retaining only their event-type and ownership
translation.

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
| saw | One static time mesh; no envelopes, effects, unison, or guide noise | Regenerated exactly. At MIDI 36–72 it reaches `0.98850–0.99844` correlation after the MIDI reference fix. It exposes remaining gain, onset, resampling, and Cycle 1 startup-repeatability gaps. |
| filter-saw | One time layer and one subtractive magnitude layer; no phase, effects, unison, or guide noise | Regenerated exactly and byte-repeatable in both engines. Correcting the harmonic-region note contract improves correlation to `0.95298–0.89154` at MIDI 36–72. The remaining time-varying mismatch exposes the prepared spectral runtime's static-frame limitation. |
| power | Time layer plus volume envelope | Regenerated exactly but rejected as an audio oracle: Cycle 1 renders silence because the active time layer has no authored waveform geometry. |
| Subbass | Time, magnitude, phase, volume/scratch envelopes | Port manifest was strict, but current notes 48–72 fail its old output thresholds; diagnostic only. |
| guitar-3-g | Time + spectral, phase pan, volume/scratch, 2x oversampling, waveshaper, IR, EQ, delay | Regenerated exactly from a live canonical export while retaining node presentation. The corrected MIDI 48 comparison reaches only `0.19678` correlation, and Cycle 1's effect-bearing render is not byte-repeatable. |
| japan-drum | Two time layers, two magnitude layers, phase, volume envelope, five guide assignments | Regenerated exactly; all four guides have zero noise/offset/phase. One corrected render repeated exactly, but a later run did not repeat in Cycle 1. Its large evolving mismatch remains diagnostic until that intermittent startup state is isolated. |
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

`CycleDsp::SpectralStageCaptureRecorder` is the shared observational boundary.
It preallocates storage before rendering, captures one explicitly selected
synthesis frame, and writes raw float payloads plus SHA-256 metadata only after
the render completes. Both engines feed the same recorder at the time frame,
forward FFT, post-layer spectrum, and reconstructed fixed-frame boundaries.
The recorder does not transform, normalize, resample, or otherwise participate
in synthesis.

### Magnitude sampling boundary

Cycle 1's authoritative path samples spectral meshes with
`LogRegions::getRegion(noteState.lastNoteNumber)` and clears magnitude and phase
bins above that note-dependent region before inverse FFT. Both operations reuse
the mature shared `LogRegionMapping` and `SpectralLayerCore` implementations.

Cycle V2 now performs only the boundary translation required by its standard
oscillator note: it adds `LogRegionMapping::legacyMidiNoteBias` before spectral
mesh sampling, limits sampling to the resulting harmonic count, and clears a
scratch copy of the IFFT inputs above that count. It does not mutate shared
graph slots or duplicate mapping/filter algorithms. This adapter remains stable
until MIDI-note domains become explicit types; at that point the integer bias
and this documentation should be replaced by the typed boundary.

This correction improves Filter Saw at every tested note but does not complete
parity. Prepared Cycle V2 frames now consume live modulation and rerasterize at
the shared synthesis-cycle cadence. Raw stage capture shows its time frame and
forward FFT remain closely matched while the magnitude-layer output separates
as the scratch envelope evolves.

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
   Reopened: correcting the legacy MIDI reference substantially aligns the
   minimal Saw fixture, but deterministic Japan Drum still diverges. Stage
   capture is now required at the multi-layer/spectral boundaries.
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
    In progress: the runner's omitted legacy MIDI event translation invalidated
    the 2026-09-08 pre-fix comparisons. Saw and Japan Drum have been rerun on
    the corrected boundary; Subbass also remains input-invalid because its
    omitted morph state is inherited from the startup document.
12. Admit deterministic whole-graph ports incrementally. In progress:
    `saw`, `filter-saw`, `guitar-3-g`, and `japan-drum` now match fresh canonical
    conversion exactly and preserve their prior presentation. Saw substantially
    matches after reference translation. Filter Saw isolates the first large
    discrepancy to magnitude-layer frequency sampling/compositing, before
    phase, multi-layer operations, or effects. Guitar 3 G additionally exposes
    nondeterministic Cycle 1 effect-tail state.
13. Add deterministic seed injection/persistence at the shared render contract
    for Guide noise, Unison jitter, and reverb, then admit one fixture for each.
    In progress: Cycle 1 voice/rasterizer seed injection is complete and proves
    repeatability for the first two fixtures; cross-engine seed mapping and
    Cycle 1 reverb remain open.
14. Remove each gain, latency, scheduling, and sample-rate policy discrepancy
    from the comparison boundary until admitted fixtures require raw exact
    sample equality.
15. Restore Cycle 1's note-dependent spectral sampling boundary in the Cycle V2
    prepared frame. Complete: the standard oscillator note is translated once
    for `LogRegionMapping`, sampling is bounded by its harmonic region, and IFFT
    scratch spectra are cleared with shared `SpectralLayerCore` behavior.
16. Make prepared spectral frames consume live voice-time/key/velocity
    modulation and rerasterize at the Cycle 1 cycle cadence. Complete; Filter
    Saw, PWM, Dunk 2, and Japan Drum evolve deterministically and are invariant
    to 64, 127, 256, and 512-sample host partitions.
17. Capture one selected spectral synthesis frame at equivalent mature Cycle 1
    and Cycle V2 boundaries, write raw payloads with hashes after rendering,
    and report the first unequal stage in the paired runner. Complete. The
    shared recorder captures semantic stereo frames and the note-active,
    non-DC harmonic region without allocating on the realtime path. The runner
    validates payload hashes and reports exact mismatches, correlation, raw
    residual, and gain-matched residual for each stage.
18. Localize the Filter Saw post-layer mismatch below the aggregate magnitude
    operation. Complete. The shared recorder now captures the raw magnitude
    operand and effective morph. Cycle V2 preserves Cycle 1's nonwrapping
    spectral margin, compiles oscillator-owned default morph inputs, snaps
    note-start depth controls, derives Voice Time from the absolute sample
    frontier, and advances scratch envelopes with the authored normalized voice
    duration. Filter Saw MIDI 48 now reaches `0.99937` output correlation, and
    its frame-32 raw magnitude operand reaches `0.99901` correlation.
19. Separate the remaining raw magnitude-raster difference from magnitude
    shaping, then capture the pitch-clocked cyclic reconstruction boundary.
    Complete. The shaped-operand capture proves the transfer function agrees,
    and the pitch-cycle capture locates the full-cycle offset in Cycle V2's use
    of the post-advance interpolation position. Using the authoritative Cycle 1
    cycle-start position produces effectively perfect same-rate output
    correlation without weakening host-block partition invariance.
20. Localize the `+18.66 dB` post-oscillator gain difference, then reconcile the
    44.1-to-48 kHz output-rate policy at an explicit conversion boundary.
    Complete: both automation renderers now accept the same explicit output
    gain, leaving their production defaults unchanged. A unity-gain Filter Saw
    render uses the extracted Cycle 1 block clock and shared Hermite converter.
    Removing Cycle V2's non-authoritative extra oscillator FIFO pad aligns both
    44.1 and 48 kHz at zero lag, effectively `1.00000` correlation, and
    `0.00049` residual. Neither discrepancy is normalized in the analyzer.
21. Localize the remaining same-clock numerical residual, beginning with the
    already-observed raw time-frame difference. Preserve zero-lag unity-gain
    comparison and do not replace the mature mesh rasterizer with a test
    approximation. Complete: shortest-round-trip mesh serialization and a
    regenerated Filter Saw graph remove port precision loss without expanding
    the JSON structure. The time-raster capture proves the remaining frame-32
    difference is the scratch clock, not rasterization: Cycle 1 uses
    `0.7148094`, while Cycle V2's blockwise signal supplies `0.7242211`.
22. Introduce a shared cycle-clocked envelope playback boundary for prepared
    oscillator regions. Complete: the authoritative implementation is
    `CycleBasedVoice::updateEnvelopes()` using the shared
    `EnvelopePlaybackEngine` in one-sample-per-cycle mode. Reuse its sampling,
    advancement, loop/release, and guide-seed behavior unchanged. The boundary
    must translate a compiled envelope attachment plus voice lifecycle and
    elapsed cycle samples into one scalar shared by every consuming mesh
    operation. Once present, prepared envelope attachments must stop deriving
    scratch time from a blockwise `SignalPayload`; arbitrary non-envelope
    scratch signals may retain that graph-level path. Do not add per-operation
    history or a delayed-buffer approximation. `PreparedCycleEnvelopeBank`
    owns one cursor per compiled envelope attachment, shares it across every
    consuming mesh operation, and follows live prepared-envelope adoption. Both
    chained lanes and shared spectral frames advance the mature engine before
    rasterization. Spectral frames also restore Cycle 1's high-quality
    `round(16 / period)` stride. Filter Saw frame 32 now has byte-identical
    time-raster samples and morph coordinates in both engines.
23. Localize the newly exposed one-cycle output scheduling offset. Complete:
    once scratch is correctly aligned, Filter Saw's captured synthesis stages
    agree through the time frame and differ first by five magnitude bins at
    `8.8e-8` normalized residual, but the analyzed Cycle V2 output is delayed by
    one 337-sample internal cycle (367 samples at 48 kHz). The prior early
    scratch signal accidentally masked this delay. Cycle V2 now prepares the
    next shared spectral frame before emitting cycles from the preceding control
    interval, matching Cycle 1's current/future frame ownership. At MIDI 48 the
    output now aligns at zero lag with `0.9999999984` correlation and a `5.6e-5`
    normalized residual. The captured pitch-clocked cycle also has the same
    10,450-sample frontier and 337-sample length in both engines. MIDI 48, 60,
    and 72 all align at zero lag with correlations of at least `0.9999999919`;
    their normalized residuals range from `5.6e-5` to `1.27e-4`.
24. Localize the remaining deterministic numeric differences. Complete: Filter
    Saw first differs in five magnitude-raster bins (`8.8e-8` normalized
    residual), which expands through reconstruction and Hermite resampling to a
    `5.6e-5` output residual. The spectral waveform coordinates and slopes are
    byte-identical; the first mismatch is five one-ULP differences in the
    logarithmic harmonic positions. Cycle 1 precomputes every MIDI region into
    one contiguous bank, while Cycle V2 regenerated one independently aligned
    vector. Accelerate's vector logarithm produced slightly different results
    at those buffer offsets. `LogRegions` now exposes its default precomputed
    bank to Cycle V2, while Cycle V2 delegates interpolation to the same bulk
    `sampleAtIntervals` path. The spectral raster, shaped operand, and
    post-layer spectrum are byte-identical at Filter Saw MIDI 48/frame 32.
    Artifact:
    `/tmp/cycle-filter-saw-shared-log-regions/comparison.json`.
25. Localize the reconstructed-frame residual. Complete: Cycle 1's spectral
    arrays omit DC, so an active count of 169 retains harmonics 1 through 169.
    Cycle V2 uses a full-polar array whose index zero is DC, but passed that same
    count directly to the shared tail-clear operation and therefore erased
    harmonic 169. The renderer now translates the legacy harmonic count to the
    full-polar bin count at that boundary. A focused Filter Saw reconstruction
    test guards both the retained final harmonic and the cleared following bin.
    On the current merged preset, MIDI 48/frame 32 reconstructs with a
    `2.6e-7` normalized residual; its earlier forward-FFT and post-layer
    differences are also floating-point-scale (`8.8e-8` and `1.1e-7`). The
    four-note MIDI 36–72 output matrix remains zero-lag and deterministic with
    correlations of at least `0.9999999919` and normalized residuals from
    `3.6e-5` to `1.27e-4`. The remaining amplification occurs in pitch-clocked
    Hermite cycle resampling, not spectral reconstruction. Artifacts:
    `/tmp/cycle-filter-saw-final-harmonic/comparison.json` and
    `/tmp/cycle-filter-saw-final-harmonic-notes/comparison.json`.

Future work: replace the inherited quality-selected control interval with an explicit
control-rate contract that may request sub-cycle synthesis updates. That is a
quality/architecture change, not part of Cycle 1 parity, and must retain the
cycle-clocked envelope boundary rather than returning to blockwise sampling.

Separate output-control gap: Cycle V2 currently applies fixed `0.125` headroom
after voice summation, and its Output node has meters but no authored master-gain
parameter. This cannot affect oscillator-stage parity and is not the source of
the magnitude-raster difference. Adding a Cycle 1-mapped vertical master fader
belongs in an Output-node control slice, with the fixed safety headroom kept as
a distinct implementation concern.

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
