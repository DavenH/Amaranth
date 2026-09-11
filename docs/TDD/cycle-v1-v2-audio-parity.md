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
| filter-saw | One time layer and one subtractive magnitude layer; no phase, effects, unison, or guide noise | Regenerated exactly and byte-repeatable in both engines. After restoring cycle-clocked scratch, frame ownership, shared log regions, and the final active harmonic, MIDI 36–72 is zero-lag with correlation of at least `0.9999999919`. |
| fallout | One time layer, one subtractive magnitude layer, one additive phase layer, and output gain; no envelopes, effects, unison, or guide noise | Regenerated exactly from a live canonical export. Its captured time, magnitude, and phase raster/operand boundaries are byte-identical at MIDI 48/frame 32. MIDI 36–72 remains zero-lag with `0.99974–1.00000` correlation after restoring the mature phase harmonic ramp and phase-only curve interpolation. |
| shiny | One time layer, two multiplicative magnitude layers, one additive phase layer, and output gain; no envelopes, effects, unison, or guide noise | Regenerated exactly from a direct canonical export. At MIDI 48/frame 32 it is byte-identical from the time frame through reconstructed spectral output. MIDI 36–72 is deterministic, zero-lag, and reaches `0.999999776–0.999999876` correlation. |
| simple-bass | Time layer, one multiplicative magnitude layer, and a volume envelope; no active phase, scratch, effects, unison, or guide noise | Verified. Shared document declick and the legacy split-rate volume-envelope clock are restored. Complete 75/200/400 ms notes at 48 kHz have zero lag and at least `0.99999999991` correlation. |
| power | Time layer plus volume envelope | Regenerated exactly but rejected as an audio oracle: Cycle 1 renders silence because the active time layer has no authored waveform geometry. |
| Subbass | Time, magnitude, phase, volume/scratch envelopes | Port manifest was strict, but current notes 48–72 fail its old output thresholds; diagnostic only. |
| guitar-3-g | Empty time bypass + spectral, phase pan, volume/scratch, 2x oversampling, waveshaper, IR, EQ, delay | Regenerated exactly from a direct canonical export while retaining node presentation. Per-channel waveshaper and IR state now match Cycle 1 ownership. MIDI 36–72 meets the diagnostic audio thresholds; EQ and delay add no material gap. MIDI 36 still fails Cycle 1's raw repeat gate, so the fixture is not admitted. |
| japan-drum | Two time layers, two magnitude layers, phase, volume envelope, five guide assignments | Regenerated exactly; all four guides have zero noise/offset/phase. One corrected render repeated exactly, but a later run did not repeat in Cycle 1. Its large evolving mismatch remains diagnostic until that intermittent startup state is isolated. |
| Icycle | Broad synthesis/effects plus six-voice Unison | Verified. Regenerated from a direct canonical export while retaining node layout, port presentation, and three authored probes. Its reverb is disabled; the corrected IR size is `0.26`. Prepared per-lane pitch playback, Cycle 1's render-boundary frame latch, and deterministic offline parameter settling bring the full MIDI 36–72 matrix to `0.98425–0.99997` correlation with exact repeatability in both engines. |
| accoustic | Broad graph including reverb | Current graph differs in morph/link state, envelope state, reverb size, and IR high-pass; do not use for DSP attribution yet. |
| organ-2 | Spectral layers, envelopes, Unison, IR, delay, reverb | Regenerated from a fresh export while retaining presentation. Its oscillator-through-delay baseline is near-identical and global effect tails now outlive voices. The full Reverb output remains diagnostic. |
| sitar | Three magnitude layers, phase, and persisted Guide noise | Verified. The converter retains Cycle 1's layer modes and Guide seeds. With the deterministic renderer environment fixed, MIDI 36–72 is byte-repeatable in both engines and reaches `0.99905–1.00000` correlation. |

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
    `saw`, `filter-saw`, `fallout`, `shiny`, `guitar-3-g`, and `japan-drum` now
    match fresh canonical
    conversion exactly and preserve their prior presentation. Saw substantially
    matches after reference translation. Filter Saw now establishes the
    evolving magnitude-layer baseline, and Fallout establishes static phase
    rasterization/compositing. Guitar 3 G exposes the next boundary at
    key/velocity-routed envelope preparation before effects can be attributed.
13. Add deterministic seed injection/persistence at the shared render contract
    for Guide noise and any remaining stochastic effect state, then admit one
    fixture for each. In progress: Cycle 1 voice/rasterizer seed injection and
    the equivalent Cycle V2 chained-oscillator Guide mapping are complete.
    Remaining stochastic node families need representative fixtures. Unison
    jitter is the fixed table owned by
    shared `UnisonCore`, not random state. Cycle 1 reverb's wall-clock seed only
    fills an unused legacy noise buffer; both engines build their audible kernel
    through deterministic shared `ReverbKernel`.
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
26. Admit a deterministic magnitude/phase preset. Complete using Fallout:
    it contains one time source, a subtractive magnitude layer, and an additive
    phase layer without envelope, effect, unison, or guide-noise confounds. The
    first comparison diverged at the phase layer and reached only `0.403`
    output correlation. Cycle 1's mature path multiplies authored phase by a
    square-root harmonic-number ramp and enables curve interpolation only for
    phase rasterization; Cycle V2 omitted both contracts. The ramp now lives in
    shared `SpectralLayerCore`, and both engines call it. Cycle V2 also sets the
    authoritative phase interpolation request at its rasterization boundary.
    At MIDI 48/frame 32 the phase raster and range-scaled phase operand are
    byte-identical, the reconstructed-frame residual is `2.5e-6`, and final
    output correlation is `0.999999986` with a `0.00017` gain-matched residual.
    MIDI 36, 48, 60, and 72 all align at zero lag with correlations from
    `0.99974` to `1.00000`; three-note matrix repeatability and an independent
    two-render MIDI 48 check are exact in both engines. Artifacts:
    `/tmp/cycle-fallout-phase-interpolation/comparison.json` and
    `/tmp/cycle-fallout-phase-matrix/comparison.json`.
27. Admit deterministic multi-layer spectral composition. Complete using Shiny:
    strict conversion now accepts one or more magnitude layers while retaining
    its neutral gain/fine-tune checks on every active layer. The checked-in
    direct export contains two multiplicative magnitude layers followed by one
    additive phase layer. At MIDI 48/frame 32 both engines are byte-identical
    from the time frame through FFT, both magnitude operands, phase operand,
    post-layer spectrum, and reconstructed frame. The first numerical
    difference occurs in pitch-clocked Hermite resampling. MIDI 36–72 is
    byte-repeatable in both engines, zero-lag, and reaches correlations from
    `0.999999776` to `0.999999876`, with gain-matched residuals from `0.00050`
    to `0.00067`. Artifacts: `/tmp/cycle-shiny-baseline/comparison.json` and
    `/tmp/cycle-shiny-matrix/comparison.json`.
28. Advance to the envelope/effect graph in Guitar 3 G. In progress:
    Guitar 3 G was re-exported directly from its current Cycle 1 `.cyc` and
    regenerated with the authoritative converter. This corrected the stale
    velocity polarity and restored full-precision mesh values. The comparison
    also found that empty-time promotion exposed a runtime omission: direct
    spectral Trimeshes feeding IFFT skipped their authored range because range
    shaping was inferred only from an intervening Add or Multiply. Direct IFFT
    is now a range-shaped spectral consumer, guarded by a focused configuration
    test. At MIDI 48/frame 32 the magnitude operand is now on the expected scale
    (`0.02328` versus `0.02658` for harmonic one) instead of bypassing shaping
    (`0.55409`). Cycle V2 now synchronously materializes dynamic envelope
    cross-sections before the first sample through the bounded realtime path in
    `cycle-v2-realtime-note-on-envelope-preparation.md`. Direct Cycle 1 export
    inspection showed Guitar 3 G's volume and scratch layers are
    `dynamic=false`; conversion now explicitly pins their legacy 0/0
    cross-section. At MIDI 48/frame zero, both channels' magnitude raster and
    effective morph triple are byte-identical, including the scratch coordinate
    `0.00553`. The magnitude-operand difference was the additive normalization
    count: Cycle 1 passes the note-dependent 169-harmonic region to the shared
    `SpectralLayerCore`, while Cycle V2 passed its 257-slot full-polar storage
    length. The prepared renderer now computes the active harmonic count once
    and uses it consistently for range shaping, capture, and IFFT tail clearing.
    At MIDI 48/frame zero, magnitude and phase operands are byte-identical. The
    reconstructed left frame has a `1.37e-7` normalized residual and the right
    frame is byte-identical. Final output remains a material effect-path
    mismatch at `0.76139` correlation. A fresh two-render run was exact in
    Cycle V2 but not Cycle 1, beginning at sample 3, so effect attribution
    remains gated on isolating that startup nondeterminism. Earlier artifacts:
    `/tmp/cycle-guitar-3-g-frame0/comparison.json` and
    `/tmp/cycle-guitar-3-g-direct-range/comparison.json` and
    `/private/tmp/cycle-guitar-realtime-note-on/comparison.json`; current
    artifact: `/tmp/cycle-guitar-active-harmonics/comparison.json`.
29. Admit a deterministic volume-envelope fixture at multiple note lengths.
    Complete using Simple Bass: direct conversion contains a time layer, one
    multiplicative magnitude layer, and one volume envelope, with no active
    phase, scratch, effect, unison, or guide-noise confounds. The strict subset
    now permits zero active phase layers, matching the converter's established
    FFT-phase bypass. A held MIDI 48 note reaches `0.999999999955` correlation
    and a `9.4e-6` gain-matched residual; a fresh three-render run is
    byte-repeatable in both engines. At 75 ms and 200 ms note lengths,
    correlation originally fell to `0.99878` and `0.99928` specifically across
    note-off. `CycleDsp::VoiceDeclick` now shares the mature ramp construction
    and release-tail alignment between engines. Cycle V2 maps the document
    setting onto the volume-envelope lifecycle boundary, with a neutral volume
    envelope carrying the policy when no authored one is active. The 75 ms and
    200 ms comparisons improve to `0.99989` and `0.99963`; the remaining
    release-window difference was subsequently isolated to the split-rate
    volume-envelope clock and resolved in slice 30. The fixture is verified.
    Artifacts:
    `/tmp/cycle-simple-bass-baseline/comparison.json`,
    `/tmp/cycle-simple-bass-repeat/comparison.json`,
    `/tmp/cycle-simple-bass-note-75/comparison.json`, and
    `/tmp/cycle-simple-bass-note-200/comparison.json`,
    `/tmp/cycle-simple-bass-declick-rate-75/comparison.json`, and
    `/tmp/cycle-simple-bass-declick-200/comparison.json`.
30. Preserve Cycle 1's split-rate volume-envelope clock in legacy-rate renders.
    Complete: native 44.1 kHz Simple Bass renders are zero-lag and effectively
    identical for complete 75 ms and 200 ms notes after the declared constant
    gain fit, proving that the shared envelope playback, release normalization,
    and terminal fade are already authoritative. At a 48 kHz output rate, Cycle
    1 synthesizes at 44.1 kHz but advances its volume envelope with
    `SynthesizerVoice::getSampleRate()` (48 kHz); pitch, scratch, and cycle-time
    updates retain their 44.1 kHz clock. Cycle V2 currently supplies one 44.1 kHz
    normalized-time increment to every envelope, making the volume release about
    8.8% too fast. The stable boundary is a distinct, precomputed volume-envelope
    increment in `AudioVoiceControls`. The offline legacy-rate adapter supplies
    it from the requested output rate; `EnvelopeSignalProcessor` consumes it only
    for volume-purpose envelopes. Native rendering leaves both increments equal.
    No timing policy, curve sampling, or release behavior is duplicated. At 48
    kHz output, complete 75 ms, 200 ms, and 400 ms notes now have zero lag,
    correlation of at least `0.99999999991`, and gain-matched residual no greater
    than `1.32e-5`. A confirming three-process 75 ms run is byte-repeatable in
    both engines. Simple Bass is now a verified parity fixture.
31. Compare translated Output gain at the same boundary. Complete: the
    converter now gives supported graphs a Cycle 1-scaled Output fader, but the
    parity runner still replaces Cycle 1's document master with unity while
    Cycle V2 retains the translated fader. This creates a constant gain fit and
    prevents an otherwise equivalent render from satisfying the raw-float
    contract. For manifests that explicitly declare a V2 Output gain, the
    runner must apply the recorded Cycle 1 master gain to the Cycle 1 capture
    and leave Cycle V2's external capture multiplier at unity. Older manifests
    without an Output fader retain their existing override behavior. The
    expected-gain report includes the graph-owned fader rather than describing
    the deliberate translation as unexplained gain. Simple Bass's 75 ms fit
    moves from `+0.198913 dB` to `-0.0000005 dB`, and the expected candidate
    scale is exactly `1.0`. Raw floats retain the already-localized same-clock
    numerical residual (`5.3e-6`), so exact-sample enforcement remains blocked
    on that DSP boundary rather than output control.
32. Establish Guitar 3 G's effect-free output baseline. Complete: the parity
    runner now disables every declared Cycle 1 effect through its authoritative,
    idempotent `Disable` action, including the impulse-response panel that does
    not expose the generic enable-control target. Disabled Cycle V2 effects now
    preserve a runtime stereo payload even when their statically declared output
    port is mono. The Guitar manifest also declares its already-translated Output
    fader. With waveshaper, IR, EQ, and delay disabled, MIDI 48 is zero-lag with
    `0.9999999997` correlation, a unity gain fit, and a `2.4e-5` normalized
    residual. Enabling the waveshaper lowers correlation to `0.98330` with a
    `0.1820` residual; adding IR lowers it to `0.75736` with a `0.6530` residual.
    The waveshaper is therefore the first material effect boundary and IR adds a
    second larger mismatch. Cycle 1 still fails the exact repeat gate by tiny
    startup values even with every effect disabled. Artifacts:
    `/tmp/cycle-guitar-no-effects-final/comparison.json`,
    `/tmp/cycle-guitar-waveshaper-final/comparison.json`, and
    `/tmp/cycle-guitar-waveshaper-ir-final/comparison.json`.
33. Restore per-channel waveshaper oversampling state. Complete: Cycle 1 owns
    one mature `Oversampler` per audio channel, while Cycle V2 passed both
    channels serially through one stateful instance. Cycle V2 now retains two
    preallocated oversampler lanes and selects them from the unary processor's
    existing channel position; the shared oversampling and transfer algorithms
    remain unchanged. An identical-stereo-input regression guards against FIR
    history crossing channels. Guitar 3 G with only its authored 2x waveshaper
    enabled now reaches `0.999999945` correlation, a unity gain fit, and a
    `0.000332` residual at zero lag. IR is the next material effect boundary.
    Artifact: `/tmp/cycle-guitar-waveshaper-channel-state/comparison.json`.
34. Restore per-channel impulse-response convolution state. Complete: Cycle 1
    owns one mature `BlockConvolver` per audio channel, while Cycle V2 passed
    both channels through one stateful block/traversal pair. Cycle V2 now owns a
    preallocated pair per channel and selects it through the unary processor's
    existing channel position. Impulse rasterization, prefiltering, convolution,
    and post-gain behavior remain delegated to the existing implementations. An
    equal-stereo-input regression guards channel independence. Guitar 3 G with
    waveshaper and IR enabled now reaches `0.9999724` correlation at zero lag,
    with a `+0.0226 dB` fit and `0.00743` residual. This resolves the material IR
    gap while retaining its smaller numerical residual for later localization.
    Artifact: `/tmp/cycle-guitar-ir-channel-state/comparison.json`.
35. Complete the Guitar 3 G effect ladder and re-audit repeatability. In
    progress: adding EQ and delay after the corrected waveshaper and IR does not
    create a material new discrepancy. A full MIDI 36–72 matrix is zero-lag
    except for MIDI 60's `-183` diagnostic alignment; correlations range from
    `0.98647` to `0.99998` and residuals from `0.0066` to `0.1639`, within the
    fixture's current diagnostic thresholds. Accelerate's inverse FFT varied by
    one float ULP across fresh Cycle 1 processes even with identical captured
    spectra. The paired runner now pins `VECLIB_MAXIMUM_THREADS=1`, which makes
    the effect-free graph repeat exactly in both engines. The full graph still
    fails Cycle 1 repeatability at MIDI 36 while MIDI 48, 60, and 72 repeat
    exactly. Do not admit the fixture until the remaining low-note effect-path
    instability is localized; do not weaken the repeat gate. Artifacts:
    `/tmp/cycle-guitar-no-effects-single-veclib/comparison.json`,
    `/tmp/cycle-guitar-through-eq-state-fixed/comparison.json`,
    `/tmp/cycle-guitar-full-deterministic-recheck/comparison.json`, and
    `/tmp/cycle-guitar-full-matrix/comparison.json`.
36. Establish the six-voice Icycle differential fixture. In progress: a fresh
    Cycle 1 canonical export regenerated the graph exactly while the converter
    retained every existing node position, port side, editor dimension, and
    all three authored signal probes. This corrects stale semantic state,
    including inverse-velocity routing, envelope declick, output gain, missing
    static envelope modulation, full-precision meshes, and IR size `0.26`.
    Probe retention is now a tested part of presentation reconciliation. With
    waveshaper, IR, and delay disabled, MIDI 48 reaches `0.99548` correlation,
    a `+0.05 dB` fit, and `0.0949` residual. Its captured time raster/frame and
    all magnitude/phase operands are byte-identical; the forward FFT differs
    only at `8.8e-8` residual, reconstruction at `5.2e-6`, then the first
    material divergence appears in pitch-clocked cycle reconstruction at about
    `0.056`. Cycle 1 differs from itself from frame 40 across fresh processes,
    even with Unison disabled, so the diagnostic fixture cannot be admitted by
    weakening the repeat gate. Artifacts:
    `/tmp/cycle-icycle-unison-baseline/comparison.json`,
    `/tmp/cycle-icycle-no-unison/comparison.json`, and
    `/tmp/cycle-icycle-unison-stages/comparison.json`.
37. Restore prepared pitch-Envelope playback inside oscillator regions.
    Complete: the compiler's 129-point pitch trajectory is presentation data
    for the Unison preview, but the realtime region incorrectly treated it as
    an audio-time buffer and clamped to its last point after 129 samples. The
    existing prepared cycle-envelope bank now also owns the resolved pitch
    source and one mature `EnvelopePlaybackEngine` cursor per Unison lane.
    Chained regions advance it before lane tuning; spectral regions advance it
    at the shared frame-control frontier, matching Cycle 1. No envelope
    interpolation or state machine was copied into the lane runtime. A focused
    Icycle regression poisons the preview trajectory yet retains Cycle 1's
    frame-32 frontier, 341-sample cycle, and left/right starting values. The
    MIDI 48 stage capture is now byte-identical through reconstructed frames;
    pitch-clocked residual falls from about `0.056` to `2.6e-5–4.6e-5`, output
    alignment moves from five samples to zero, correlation rises from
    `0.99548` to `0.99780`, and residual falls from `0.0949` to `0.0662`.
    The full effect graph reaches `0.99825` correlation and `0.0592` residual.
    A MIDI 36–72 effect-free matrix ranges from `0.96604` to `0.99986`, leaving
    low-note lane accumulation and Cycle 1 repeatability open. Artifacts:
    `/tmp/cycle-icycle-prepared-pitch/comparison.json`,
    `/tmp/cycle-icycle-prepared-pitch-full/comparison.json`, and
    `/tmp/cycle-icycle-prepared-pitch-matrix/comparison.json`.
38. Preserve Cycle 1's oscillator render-boundary frame latch. Complete:
    Cycle 1 copies the current reconstructed frame into its past-frame storage
    after each successful `renderInterpolatedCycles()` call. Cycle V2 retained
    the older frame across calls, causing low notes to interpolate against a
    frame Cycle 1 had already latched away. The spectral runtime now performs
    the same narrow state transition after consuming a block. It also retains
    Cycle 1's float-precision neutral frame clock and the fractional lane-cycle
    start used to calculate interpolation position. Pitch-clocked diagnostics
    now retain the complete composed source cycle as their secondary payload,
    which localized the mismatch before Hermite resampling. With effects
    disabled, MIDI 36–72 reaches `0.98602–1.00000` correlation with
    `0.0011–0.1667` residual. The full graph reaches `0.98425–0.99997`
    correlation with `0.0076–0.1768` residual. Cycle V2 repeats exactly and all
    four audio comparisons pass; Cycle 1 intermittently differed from sample 2
    at MIDI 48 in the full run, so the fixture remains diagnostic. The inherited
    latch permits bounded block-partition variation while retaining identical
    shared-frame render counts. Artifacts:
    `/tmp/cycle-icycle-midi36-frame8-composed/comparison.json`,
    `/tmp/cycle-icycle-block-latch-matrix/comparison.json`, and
    `/tmp/cycle-icycle-full-block-latch-matrix/comparison.json`.
39. Make Cycle 1 offline effect startup deterministic and admit Icycle.
    Complete: the live audio device can advance waveshaper, IR, EQ, and master
    smoothing by a timing-dependent number of samples between preset load and
    offline capture. The capture adapter now settles those existing parameters
    to their authored targets after suspending and re-preparing the device; it
    does not change realtime smoothing. Preset-open commands also honor their
    existing `waitForIdle` contract. Three fresh waveshaper-only processes now
    produce byte-identical Cycle 1 output. The complete Icycle MIDI 36–72 matrix
    repeats exactly in both engines and passes every declared audio threshold:
    correlation is `0.98425–0.99997`, residual is `0.0076–0.1768`, spectral
    RMSE is `0.02–0.61 dB`, and cyclogram difference is `0.0065–0.0930`.
    Icycle is now a verified representative for multiple time/magnitude/phase
    layers, three envelope purposes, six-voice Unison, waveshaper, IR, and
    delay. Artifacts:
    `/tmp/cycle-icycle-repeat-waveshaper-settled/comparison.json` and
    `/tmp/cycle-icycle-full-settled-matrix/comparison.json`.
40. Establish Organ 2's semantic and oscillator baseline. Complete: a fresh
    Cycle 1 export regenerated the Cycle V2 graph while retaining its authored
    node, port, editor, and probe presentation. The converter now applies Cycle
    1's pre-1.5 Reverb compatibility override, where the stored former-dry slot
    resolves to a `0.05` high pass. A negative scratch-buffer sentinel no longer
    aliases the prepared pitch Envelope, and synthesis transforms now reuse
    Cycle 1's offset-removing inverse-FFT contract. With Reverb disabled, the
    complete oscillator, IR, and delay path reaches `0.99999` correlation,
    `0.0037` residual, `0.03 dB` spectral RMSE, and `0.0014` cyclogram
    difference at MIDI 48. Cycle 1's offline settlement now also completes its
    existing Reverb kernel and parameter actions deterministically. Full
    Reverb renders repeat exactly, but are not eligible for threshold admission:
    Cycle V2 currently instantiates and gates all effects per voice, whereas
    Cycle 1 mixes voices before running one continuous global chain. Artifacts:
    `/tmp/cycle-organ-2-oscillator-dc-fixed/comparison.json`,
    `/tmp/cycle-organ-2-no-reverb-after-fixes/comparison.json`, and
    `/tmp/cycle-organ-2-full-matrix-candidate/comparison.json`.
41. Restore the live Voice Length contract. Complete: Cycle 1's oscillator panel
    control governs the normalized lifetime used by live voices. Cycle V2's
    identically presented Voice Context control currently changes only preview
    state. The stable boundary is a thread-safe runtime-duration value owned by
    `RealtimeGraphRenderer`; `NodeWorkspace` translates the canvas setting
    into that value without turning session audio configuration into a graph
    mutation. Preview and live rendering consume the same clamped duration. A
    focused active-note sequence changes the duration from 1.0 to 0.1 seconds
    and observes the live Voice Time slope increase by exactly 10x.
42. Restore the global post-voice effects boundary. Complete: Cycle 1's
    `SynthAudioSource::processBlock()` is authoritative: synth voices are mixed,
    then waveshaper, IR/tube, EQ, delay, Reverb, and master gain run once on
    every audio block. Cycle V2 reuses its existing node processors and
    topology unchanged, while graph metadata declares processor lifetime and
    the executor translates voice-boundary buffers into one preallocated global
    execution. No effect DSP or graph traversal will be copied. The stable end
    state removes effect-tail ownership from individual voices: note release can
    retire a synth voice independently, while the persistent global executor
    continues receiving silence and emitting delay/Reverb tails. The intended
    graph end state keeps Reverb and delay intrinsically global, while
    Waveshaper, IR Modeller, and EQ expose authored voice-local/global modes
    (defaulting to global for Cycle 1 ports). Adding those editor selections is
    outside this parity slice; regardless of selection, scope remains monotonic
    downstream and a global signal cannot become voice-local again. Compiler
    and renderer regressions cover downstream scope propagation and a delay echo
    remaining audible for multiple callbacks after its source voice retires.
43. Localize Organ 2's remaining global Reverb output mismatch. Complete:
    moving the existing effect chain to its authoritative lifetime boundary does
    not change the held-note mismatch, as expected for a single active voice.
    With every effect disabled, MIDI 48 remains zero-lag at `0.99999`
    correlation, `0.0041` residual, `0.03 dB` spectral RMSE, and `0.0017`
    cyclogram difference. The full chain remains zero-lag at `0.97076`
    correlation and `0.2400` residual; a 250–750 ms post-note window confirms
    both engines now emit tails but retains a material Reverb-shape difference.
    Kernel contents, configuration, input blocks, convolution framing, and
    first-block dry energy, wet energy, and dry/wet correlation are equivalent.
    Cycle V2 now delegates its wet/dry calculation to a shared primitive that
    is guarded against Cycle 1's mono dry law and stereo width law, while the
    mature Cycle 1 implementation remains unchanged. The mismatch begins as
    the small oscillator-through-delay residual accumulates through the long
    convolution; it is not a different Reverb parameter, kernel, processing
    scope, or wet/dry feature. Organ 2 therefore remains diagnostic pending an
    earlier exact oscillator boundary rather than another Reverb rewrite.
    Artifacts: `/tmp/cycle-organ-2-global-no-effects-fixed/comparison.json`,
    `/tmp/cycle-organ-2-global-full-fixed/comparison.json`, and
    `/tmp/cycle-organ-2-global-tail/comparison.json`. Focused boundary trace:
    `/tmp/cycle-organ-2-reverb-mix-debug-4/`. The behavior-neutral shared-mix
    rerun is `/tmp/cycle-organ-2-shared-v2-reverb-mix/comparison.json`.
44. Establish a deterministic Guide-noise fixture. Complete: the existing
    offline `randomSeed` command now reaches a generic deterministic per-voice
    seed in Cycle V2 while the live lifecycle seed remains unchanged. The
    chained oscillator owns Cycle 1's `+2` Unison-stream translation, consumes
    the same first draw for Guide offsets, and retains the advanced stream for
    per-cycle random positions. A focused Flute regression proves identical
    output for a repeated seed and different output for another seed. Flute was
    regenerated from a direct Cycle 1 export while retaining Cycle V2's node,
    port, and editor presentation. Fresh MIDI 36–72 renders repeat byte-for-byte
    in both engines and pass all declared thresholds: correlation is
    `0.98566–0.99406`, residual is `0.1088–0.1687`, spectral RMSE is
    `0.45–1.41 dB`, and cyclogram difference is `0.0343–0.1389`. Flute is now
    a verified representative for chained Guide noise and IR. Artifact:
    `/tmp/cycle-flute-verified/comparison.json`.
45. Establish deterministic spectral Guide-noise mapping. Completed: Cycle
    1's `SynthFilterVoice` owns a random stream seeded from the offline
    per-voice base plus one. Note preparation consumes its time Guide-offset
    draw from that stream; the magnitude and phase offset seeds belong to the
    parent voice's separate random stream. Subsequent active layer
    rasterizations continue from the filter stream in render order. Cycle V2
    translates that ownership inside `SpectralOscillatorFrameRenderer`, leaving
    its live lifecycle-seed path unchanged. Sitar is the smallest effect-free factory
    candidate with an assigned noisy magnitude Guide and no active envelopes.
    Its first differential render proved the noisy magnitude raster is
    byte-identical, but exposed a separate conversion gap: promoting an empty
    time seed removed the first additive operation, after which Cycle V2
    inferred that root layer's mode from the next downstream multiply and
    omitted additive normalization. The converted graph must retain each Cycle
    1 spectral layer mode explicitly while old graphs retain topology-based
    `auto` inference. A second phase-noise diagnostic also showed that Cycle 1
    initializes only spectral Guide offset slot zero; Cycle V2's prepared
    spectral adapter must retain that mature count without changing general
    node rasterization. The diagnostic also exposed a missing semantic field:
    Cycle 1 persists a base noise-table seed per Guide (falling back to its
    stable Guide index), while Cycle V2 previously rederived it from the string
    resource id. The converter and graph resource now preserve that seed. The
    focused spectral renderer contract is exactly repeatable and responds to
    seed changes. Sitar's first magnitude raster/operand is byte-identical and
    its phase raster/operand is within `4.9e-8` normalized residual, isolating
    its remaining `0.2571` rendered residual to spectral-stack composition
    rather than random-state preparation. Sitar remains diagnostic because
    Cycle 1 also differs by ULPs across repeated captures. Persisted Guide seeds
    improved the already verified Flute matrix to correlation
    `0.99704–0.99975`, residual `0.0222–0.0769`, spectrum `0.05–0.17 dB`, and
    cyclogram `0.0120–0.0504`; both engines are exactly repeatable. Artifacts:
    `/tmp/cycle-sitar-persisted-guide-seeds/comparison.json` and
    `/tmp/cycle-flute-persisted-guide-seeds/comparison.json`.
46. Align multi-layer spectral-stack composition. Completed: an
    occurrence-selectable shared stage recorder localized Sitar's error to its
    third magnitude layer. Cycle V2 had consumed the parent-owned magnitude and
    phase offset draws from the filter RNG, advancing noisy layer seeds by two.
    Keeping those offset calculations separate while advancing the retained
    filter stream only for its time-offset draw makes the third magnitude raster
    and operand byte-identical. The full MIDI 36–72 matrix now reaches
    correlation `0.99905–1.00000`, residual `0.0005–0.0437`, spectrum
    `0.00–1.06 dB`, and cyclogram `0.0003–0.0388`. Cycle V2 repeats exactly at
    every pitch. Sitar remains diagnostic only because Cycle 1 intermittently
    differs by ULPs on repeated MIDI 48 captures. Artifact:
    `/tmp/cycle-sitar-verified/comparison.json`.
47. Resolve the intermittent Cycle 1 spectral-reference repeatability failure.
    Complete: MIDI 48 sometimes repeated exactly and sometimes differed by
    single-precision ULPs despite identical seeded stage operands. Preserve the
    exact repeatability gate; identify the first unstable downstream boundary
    before admitting Sitar as verified. Five plain repeated captures produced
    two stable payload hashes, with one render differing from the other four
    starting at output frame 41. Repeating the same capture five times with the
    stage recorder present was byte-identical, and every captured spectral
    boundary hash matched. The comparison harness now retains stage capture on
    repeat renders so instrumented determinism compares identical execution
    conditions. The remaining variable was the macOS Nano allocator regime:
    disabling it for both child renderers, alongside the existing single-threaded
    vecLib contract, retains the established Cycle 1 payload hash and makes five
    fresh uninstrumented captures byte-identical. The harness now owns that
    process-level determinism boundary. The full MIDI 36–72 matrix repeats
    byte-for-byte in both engines and reaches `0.99905–1.00000` correlation,
    so Sitar is admitted as verified. Artifacts:
    `/tmp/cycle-sitar-reference-repeat-plain/comparison.json`,
    `/tmp/cycle-sitar-reference-repeat-stages/comparison.json`,
    `/tmp/cycle-sitar-repeat-harness/comparison.json`, and
    `/tmp/cycle-sitar-verified-allocator/comparison.json`.
48. Preserve stereo identity through the global Delay boundary. Complete:
    Cycle 1 and Cycle V2 already share `CycleDsp::CycleDelay`, and both effect
    wrappers own one delay state per channel. Add a direct stereo contract and
    a realtime global-boundary contract before changing DSP; any failure must
    be corrected where runtime channel metadata or buffers cross the voice mix,
    without introducing a second delay implementation. Direct independent-input
    and pan-cycle regressions prove that Cycle V2 retains two channels and emits
    complementary echoes. A fresh Icycle MIDI 48 differential render is stereo
    in both engines and reaches `0.99993` correlation with `0.0121` residual.
    There is no justified Delay DSP change; a remaining centered impression is
    preset-specific until an upstream boundary capture proves otherwise.
    Artifact: `/tmp/cycle-icycle-stereo-audit/comparison.json`.
49. Publish live Output gain independently of graph preparation. Complete:
    Output gain is durable graph state, but the realtime renderer currently
    samples the compiled value only when adopting a prepared graph. A parameter
    refresh updates the Output processor configuration while leaving the
    renderer-owned compiled gain stale, and replacing a prepared graph resets
    voices. Keep the graph parameter authoritative, refresh its compiled plan
    field, and translate transient fader movement into a thread-safe renderer
    target so a held note responds without graph replacement. Commit and undo
    remain semantic dispatcher edits. `GraphCompiler` now owns the one Output
    mapping used by initial compilation and parameter-only plan refreshes. The
    workspace polls the dispatcher-owned editing view and publishes its mapped
    value through an atomic renderer target; the audio thread retains the
    existing smoothing and active graph/voice. Focused tests cover compiled
    refresh and a held voice responding without graph replacement.
50. Make live Output meters invalidate their cached node layer. Complete:
    the audio renderer already publishes independent left/right peaks and the
    workspace polls them at 30 Hz. The canvas requests repaint, but the outer
    node-layer cache key omits the live levels and reuses the old Output image.
    Include only the meter state in the Output render-context fingerprint; do
    not duplicate metering or bypass the existing renderer diagnostics. The
    existing live-device fixture now reports nonzero left/right display levels
    (`0.5869` in the verification run), and cache-key coverage guards dynamic
    context invalidation. Artifact: `/tmp/cycle-v2-meter-report.json`.
51. Re-audit older intermittent fixtures under the deterministic allocator
    contract. In progress: Guitar 3 G MIDI 36 repeats exactly across five fresh
    processes and retains `0.99998` correlation, but a subsequent full matrix
    moved the Cycle 1 repeat failure to MIDI 48. That mismatch starts in the dry
    output before any delayed sample can return; disabling Delay produced three
    exact repeats, but this does not identify Delay as the source because its
    presence also changes the process allocation layout. Japan Drum likewise
    repeated exactly five times at MIDI 48, then failed one Cycle 1 repeat at
    the same note inside a later full matrix. Both remain diagnostic. Preserve
    the exact gate and localize the next failure with equivalent allocation and
    capture conditions before changing shared DSP. Artifacts:
    `/tmp/cycle-guitar-repeat-allocator/comparison.json`,
    `/tmp/cycle-guitar-verified-allocator/comparison.json`,
    `/tmp/cycle-guitar-repeat-no-delay/comparison.json`,
    `/tmp/cycle-japan-drum-repeat-allocator/comparison.json`, and
    `/tmp/cycle-japan-drum-verified-allocator/comparison.json`.

Future work: replace the inherited quality-selected control interval with an explicit
control-rate contract that may request sub-cycle synthesis updates. That is a
quality/architecture change, not part of Cycle 1 parity, and must retain the
cycle-clocked envelope boundary rather than returning to blockwise sampling.

The separate output-control gap is resolved: Output owns a Cycle 1-mapped
vertical master fader, while the fixed safety headroom remains a distinct
renderer concern. Slice 31 aligns the comparison harness with that ownership.

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
