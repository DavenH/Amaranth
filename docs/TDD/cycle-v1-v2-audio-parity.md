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
| saw | One static time mesh; no envelopes, effects, unison, or guide noise | Verified. Restoring its missing Voice Output cable, inverse-velocity mapping, canonical mesh, and Output gain makes MIDI 36–72 deterministic and zero-lag above `0.9999999999` correlation. |
| filter-saw | One time layer and one subtractive magnitude layer; no phase, effects, unison, or guide noise | Regenerated exactly and byte-repeatable in both engines. After restoring cycle-clocked scratch, frame ownership, shared log regions, and the final active harmonic, MIDI 36–72 is zero-lag with correlation of at least `0.9999999919`. |
| fallout | One time layer, one subtractive magnitude layer, one additive phase layer, and output gain; no envelopes, effects, unison, or guide noise | Diagnostic. Its captured time, magnitude, and phase raster/operand boundaries are byte-identical at MIDI 48/frame 32, and MIDI 36–72 remains zero-lag with `0.99974–1.00000` correlation. A fresh Cycle 1 MIDI 48 repeat selected a different floating-point payload. |
| shiny | One time layer, two multiplicative magnitude layers, one additive phase layer, and output gain; no envelopes, effects, unison, or guide noise | Regenerated exactly from a direct canonical export. At MIDI 48/frame 32 it is byte-identical from the time frame through reconstructed spectral output. MIDI 36–72 is deterministic, zero-lag, and reaches `0.999999776–0.999999876` correlation. |
| simple-bass | Time layer, one multiplicative magnitude layer, and a volume envelope; no active phase, scratch, effects, unison, or guide noise | Diagnostic. Shared document declick and the legacy split-rate volume-envelope clock are restored, and complete 75/200/400 ms notes at 48 kHz have zero lag and at least `0.99999999991` correlation. A fresh Cycle 1 MIDI 48 repeat selected a different floating-point payload. |
| power | Time layer plus volume envelope | Regenerated exactly but rejected as an audio oracle: Cycle 1 renders silence because the active time layer has no authored waveform geometry. |
| Subbass | Time mesh and volume Envelope; empty spectral/scratch layers are simplified away | Verified. The canonical octave, control interval, Envelope ownership, morph defaults, and Output value make MIDI 36–72 deterministic and zero-lag above `0.9999999998` correlation. |
| guitar-3-g | Empty time bypass + spectral, phase pan, volume/scratch, 2x oversampling, waveshaper, IR, EQ, delay | Regenerated exactly from a direct canonical export while retaining node presentation. Per-channel waveshaper and IR state now match Cycle 1 ownership. MIDI 36–72 meets the diagnostic audio thresholds; EQ and delay add no material gap. MIDI 36 still fails Cycle 1's raw repeat gate, so the fixture is not admitted. |
| japan-drum | Two time layers, two magnitude layers, phase, volume envelope, five guide assignments | Regenerated exactly; all four guides have zero noise/offset/phase. One corrected render repeated exactly, but a later run did not repeat in Cycle 1. Its large evolving mismatch remains diagnostic until that intermittent startup state is isolated. |
| Icycle | Broad synthesis/effects plus six-voice Unison | Diagnostic. Regenerated from a direct canonical export while retaining node layout, port presentation, and three authored probes. Its reverb is disabled; the corrected IR size is `0.26`. Prepared per-lane pitch playback and Cycle 1's render-boundary frame latch bring the full MIDI 36–72 matrix to `0.98425–0.99997` correlation. Cycle 1's IR-enabled output intermittently selects one of two floating-point payloads across fresh processes, so the exact repeat prerequisite is not yet met. |
| astral | Three magnitude layers, phase pan, volume/scratch envelopes, and delay | Regenerated from a fresh live export while retaining the existing Cycle V2 node presentation. The former hand-authored graph rendered effectively silent in the differential harness. At MIDI 48 the regenerated graph is zero-lag with `1.00000` correlation, `0.0011` gain-matched residual, and `0.35 dB` spectral RMSE. Cycle V2 repeats exactly; Cycle 1 does not, so the fixture remains diagnostic. |
| accoustic | Time, two magnitude layers, two phase layers, volume/scratch envelopes, IR, delay, and reverb | Diagnostic. Regenerated from a live canonical export while retaining every node position and editor/port presentation. Its migrated 256-sample control interval aligns evolving morph-frame frontiers. At MIDI 48 the dry graph reaches `1.00000` correlation and `0.01 dB` spectral error, and the full 44.1 kHz graph does the same. The 48 kHz Reverb tail still amplifies the tiny upstream/output-rate residual beyond the spectral threshold. |
| organ-2 | Spectral layers, envelopes, Unison, IR, delay, reverb | Regenerated from a fresh export while retaining presentation. Its oscillator-through-delay baseline is near-identical and global effect tails now outlive voices. The full Reverb output remains diagnostic. |
| sitar | Three magnitude layers, phase, and persisted Guide noise | Diagnostic. The converter retains Cycle 1's layer modes and Guide seeds, and MIDI 36–72 reaches `0.99905–1.00000` correlation. A fresh Cycle 1 MIDI 48 repeat again selected a different floating-point payload while Cycle V2 remained exact. |

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
    `0.99974` to `1.00000`. A 2026-09-12 fresh admission recheck still passes
    every audio threshold, but Cycle 1 selected a different floating-point
    payload for the MIDI 48 repeat while Cycle V2 remained exact. Fallout is
    therefore diagnostic rather than verified. Artifacts:
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
    `dynamic=false`. A semantic review correctly established that this flag
    concerns live time/yellow evolution rather than the red/blue cable grammar,
    but initially drew the wrong audio conclusion and removed the explicit
    `0/0` routing. The source trace and migration later in this section show
    that Cycle 1's routed smoothed values never reach the rasterizer; imported
    factory graphs now preserve that audible `0/0` cross-section explicitly.
    The magnitude-operand difference was
    the additive normalization
    count: Cycle 1 passes the note-dependent 169-harmonic region to the shared
    `SpectralLayerCore`, while Cycle V2 passed its 257-slot full-polar storage
    length. The prepared renderer now computes the active harmonic count once
    and uses it consistently for range shaping, capture, and IFFT tail clearing.
    At MIDI 48/frame zero, magnitude and phase operands are byte-identical. The
    reconstructed left frame has a `1.37e-7` normalized residual and the right
    frame is byte-identical. That intermediate effect-path result reached
    `0.76139` correlation; the subsequent waveshaper, IR, and full-chain slices
    below resolve it. A fresh two-render run was exact in Cycle V2 but not Cycle
    1, beginning at sample 3. Earlier artifacts:
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
    volume-envelope clock and resolved in slice 30. A 2026-09-12 admission
    recheck remains sample-equivalent at all four pitches, but Cycle 1 selected
    a different floating-point payload for the MIDI 48 repeat while Cycle V2
    remained exact. The fixture is therefore diagnostic rather than verified.
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
    both engines. Simple Bass was admitted at this point; the later fresh
    admission recheck in slice 29 returns it to diagnostic status because the
    Cycle 1 MIDI 48 repeat selected a different floating-point payload.
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
41. Restore the live Voice Length contract. Complete, then superseded by slice
    54's durable preset work: the initial repair made the preview control reach
    `RealtimeGraphRenderer` through a thread-safe session override, proving with
    an active-note sequence that changing 1.0 to 0.1 seconds increases the live
    Voice Time slope by exactly 10x. Slice 54 removes that editor side channel;
    the same renderer now reads the compiled Voice Context duration, while the
    explicit override remains only for offline differential requests.
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
    so Sitar was admitted as verified. A 2026-09-12 fresh admission recheck
    again selected a different Cycle 1 floating-point payload at MIDI 48 while
    Cycle V2 remained exact and every cross-version audio threshold passed.
    Sitar is therefore diagnostic pending a repeatability boundary that holds
    across fresh runs. Artifacts:
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

    Icycle exposes the same remaining boundary. A five-process effect ladder
    repeats exactly with IR disabled and first becomes unstable when IR is
    enabled. The shared static-kernel preparation retained the oversampler's
    FIR delay from an earlier preparation; `rasterizeIrImpulse` now resets that
    history and a shared-core regression proves that consecutive preparations
    with one oversampler are byte-identical. This removes a real state leak and
    improves the full MIDI 48 comparison to zero lag, `0.99996` correlation,
    `0.0084` residual, and `0.15 dB` spectral RMSE. It does not eliminate the
    older process-level Cycle 1 variability: four of five plain captures can
    share one payload while the fifth selects a second payload, differing from
    frame 2 by floating-point-scale values. With stage capture enabled, all
    five renders repeat exactly and every oscillator boundary matches. An
    attempted extra offline IR settlement did not change the result and was
    removed. Icycle therefore returns to diagnostic status without weakening
    the exact gate. Artifacts:
    `/private/tmp/cycle-icycle-repeat-audit-dry/`,
    `/private/tmp/cycle-icycle-repeat-audit-waveshaper-ir/`,
    `/private/tmp/cycle-icycle-ir-reset-repeat/`, and
    `/private/tmp/cycle-icycle-ir-reset-stages/`.

    Further boundary hashes rule out IR as the origin of those two payloads.
    Every Cycle 1 IR kernel rebuild is byte-identical across both output modes,
    while the first nonzero block entering IR already has a mode-specific hash;
    each input then maps deterministically through convolution. Resetting the
    Waveshaper's FIR history at capture startup also leaves the same two output
    hashes, so that attempted change was removed. A current full-precision
    Filter Saw recheck provides the allocation-equivalent negative control:
    this preset has no effects, Unison, Envelopes, or noise, yet Cycle 1 still
    fails one of three fresh MIDI 48 repeats while MIDI 36, 60, and 72 repeat
    exactly. MIDI 48 remains zero-lag at `1.00000` correlation with a `0.0001`
    residual. This localizes the open issue to Cycle 1's mature spectral/audio
    numerical path rather than any Cycle V2 effect implementation. Keep the
    affected fixtures diagnostic; do not approximate or alter shared DSP to
    manufacture exact equality. Artifacts:
    `/private/tmp/cycle-icycle-ir-kernel-hashes/`,
    `/private/tmp/cycle-icycle-ir-nonzero-*.log.raw`, and
    `/private/tmp/cycle-filter-saw-admission-recheck/`.
52. Preserve real linked-stereo payloads through global Delay. Complete:
    real compiled time-signal ports use `ChannelLayout::LinkedStereo`, while
    the runtime payload predicate recognizes only `StereoPair`. The prior Delay
    test manually supplied `StereoPair` and therefore bypassed the production
    failure. The runtime now separates a port's declared stereo arity from the
    presence of materialized secondary samples: `LinkedStereo` preserves a real
    second block but still permits existing scalar payloads to expand safely,
    while `StereoPair` remains explicitly two-channel. Guard the complete
    compiled path with a Stereo Split/Join immediately before Delay so a real
    `LinkedStereo` payload with deliberately different channels crosses Delay
    and Output. Reverting the predicate collapses that regression to identical
    channels; the corrected path passes. Reuse the shared `CycleDelay`; do not
    add Delay-local channel adaptation.

    Paired three-note Icycle fixtures additionally render Cycle 1 and Cycle V2
    for three seconds, with overlapping note lifetimes and a 2.05-second window
    after the first note-off. Both engines retain strongly non-mono output and
    uninterrupted release/effect energy: side RMS remains `0.06680` in Cycle 1
    and `0.07304` in Cycle V2 during 2.0-2.95 seconds. First-note cross-engine
    correlation remains approximately `0.999` per channel, then declines
    during overlap and the late tail. This is a remaining multi-voice/tail
    DSP-parity diagnostic, not a processing-ownership regression: the current
    Icycle Waveshaper, IR,
    Delay, and Output steps are all global, and the node definitions also keep
    EQ and Reverb global. Selectable Waveshaper/IR/EQ scope remains the explicit
    future architecture described in Slice 42. Fixtures:
    `scripts/fixtures/cycle-agent-icycle-note-sequence.json` and
    `scripts/fixtures/cycle-v2-agent-icycle-note-sequence.json`. Artifacts:
    `/tmp/cycle-v1-icycle-note-sequence-report.json`,
    `/tmp/cycle-v2-icycle-note-sequence-report.json`, and their `.f32le` audio.
53. Localize Icycle's post-note release drift. Complete: rerendering the
    three-note sequence with Waveshaper, IR, and Delay disabled leaves the
    mismatch in the dry voice sum, so it is not caused by global effect state.
    Isolated MIDI 48, 55, and 60 renders remain between `0.9979` and `0.99999`
    correlation per channel while held, then fall to `0.57-0.83` after
    note-off. This reproduces without overlapping voices and rules out a
    polyphonic ownership failure.

    A post-release stage capture at oscillator frame 64/frontier 21575 first
    differs at the time raster. Its red and blue morph coordinates remain
    identical, while the time coordinate is `0.62508` in Cycle 1 and `0.68939`
    in Cycle V2. Icycle's prepared scratch Envelope overrides that time morph.
    The initial lookahead diagnosis was incomplete: matched held-note captures
    at frames 0, 16, 63, 64, and 65 show that float-rounded V2 time increments
    can select the wrong sample for one precision-sensitive loop transition.
    Cycle 1 computes a float inverse duration and retains the subsequent
    sample-rate division as a double. V2 now retains that same arithmetic
    boundary for both cycle and volume Envelope clocks; frame 64 then matches.

    The remaining tail gap was a V2-only lifecycle rule. Icycle's active scratch
    cross-section has no release curve. Mature `EnvRasterizer::setNoteOff()`
    requests release but leaves a no-release pitch/scratch Envelope advancing
    normally; `PreparedCycleEnvelopeBank` instead deactivated it and froze its
    last value at `0.689390`. The prepared bank now preserves active playback
    when the shared engine reports no release curve, while Reset remains the
    hard-stop boundary. At post-note frame 80/frontier 26969 its time morph is
    the Cycle 1 value `0.576615`. The dry 1.2-second release comparison improves
    from lag 372, correlation `0.85890`, residual `0.5121`, and spectral RMSE
    `6.56 dB` to zero lag, correlation `0.99989`, residual `0.0151`, and
    spectral RMSE `0.11 dB`; both engines are repeatable. No envelope playback
    or oscillator algorithm was duplicated. Artifacts:
    `/private/tmp/cycle-icycle-precision-fix-stage64/`,
    `/private/tmp/cycle-icycle-release-continued/`,
    `/tmp/cycle-icycle-sequence-notes-dry/`, and
    `/tmp/cycle-icycle-start-note-dry/`.
54. Restore live Voice Context and preset-control parity. Complete. The
    live octave defect is complete: a
    parameter-only refresh updates execution-step configurations but leaves the
    compiled Voice Context snapshot unchanged. Consequently octave edits can
    publish a newly prepared graph that still carries the old oscillator MIDI
    offset. Reuse `GraphCompiler`'s authoritative Voice Context compilation on
    DSP-configuration refreshes; no editor-to-audio side channel was added.
    A focused test guards the durable edit, refresh, published plan, and changed
    rendered output without topology recompilation. The keyboard sub-slice is
    also complete: JUCE's authoritative horizontal-keyboard hit testing keeps
    top-to-bottom velocity increasing, V2 no longer caps its bottom edge at
    `0.8`, and the dock now exposes C3-C5 at the existing 25-by-100 pixel white
    key geometry. The widened 451-by-112 dock and full MIDI-range clamping are
    covered by component and focused automation tests; visual artifact:
    `/tmp/cycle-v2-keyboard-parity-after.png`. The temporary runtime ownership
    badges exposed the compiler's current downstream-promotion model, but the
    resulting `VOICE` labels add noise and do not express the intended authored
    graph boundary. [`cycle-v2-global-audio-graph.md`](cycle-v2-global-audio-graph.md)
    supersedes that presentation: voice-local processing is unmarked, a
    global-only icon replaces text labels, Global Input roots a disjoint graph,
    and selectable effect ownership becomes durable graph state. The badges
    remain a deletion target of that TDD rather than the final parity UI.
    Icycle spy traversal is also complete. Its authored scratch Envelope and
    both mature rasterizers were dynamic, but the diagnostic traversal used
    `sampleAtIntervals`, whose intentional all-or-silence contract rejected the
    complete request when the last `127 / 128` cursor exceeded that prepared
    waveform's `0.987672` upper bound. The resulting zero scratch grid replaced
    every attached Trimesh time coordinate with zero, making otherwise dynamic
    grids and their Spies appear pinned. Envelope diagnostics now reuse the
    mature cursor-aware scalar sampler for each requested position, retaining
    valid columns while leaving only positions outside the prepared waveform at
    zero. A bundled-Icycle regression requires the scratch rollout, all attached
    time/magnitude/phase mesh grids, and all three authored Spy grids to change
    across voice time; the existing two-dimensional Spy renderer remains
    unchanged. Focused UI coverage confirms all three real preset probes remain
    connected, nonempty two-dimensional grids in
    `scripts/fixtures/cycle-v2-agent-icycle-spies.json`; visual artifact:
    `/private/tmp/cycle-v2-icycle-spies.png`. Next, audit
    every factory `.cyc`/`.cyclegraph` pair for voice length, octave, pitch,
    portamento, oversampling, and migrated envelope ownership while preserving
    authored Cycle V2 node presentation. Keyboard range/velocity, runtime
    global-graph architecture and Astral audio remain separate observable slices
    under this item.

    The factory-wide control audit now uses fresh live Cycle 1 exports as its
    authority. Of 228 filename-matched presets, 216 contain both Cycle 1
    oscillator controls and one Cycle V2 Voice Context; all 216 omit the Cycle
    1 voice length, 50 carry a different octave, and Stengah alone carries a
    different realtime oversampling factor. Three Cycle 1 documents omit the
    oscillator-control payload and nine matched Cycle V2 documents have no
    Voice Context, so those twelve are explicit migration gaps rather than
    candidates for guessed values.

    Voice length must become durable Voice Context state. The existing editor
    row writes an application preference through `NodeEditorResources`, while
    octave, pitch, portamento, and oversampling use semantic graph commands.
    Add the normalized Cycle 1 duration control to the Voice Context definition,
    compile its mapped seconds into `CompiledVoiceContext`, and make the live
    renderer use the audible oscillator region's compiled context. Keep the
    explicit offline duration override for the differential harness. Delete the
    editor-to-canvas-to-audio callback bridge and retain no second mutable
    authority. This slice does not alter effect ownership, global routing, or
    node placement, which are owned by the parallel global-audio TDD.

    The durable-control sub-slice is complete. Voice Length now participates in
    the same dispatcher-owned parameter gesture as the other Voice Context
    controls, survives graph serialization, refreshes the compiled context, and
    drives the realtime Voice Time and volume-envelope clocks. The former
    application preference and editor/canvas/audio callback path are no longer
    runtime authorities; offline comparison requests retain their explicit
    duration override. The compact node summary also reads the node parameter.
    All 216 unambiguous factory pairs now carry their Cycle 1 duration, octave,
    pitch, portamento, and realtime oversampling values without changing node
    positions or edges. Five V2 graphs without authoritative Cycle 1 controls
    retain the prior one-second default explicitly, as do the three bundled
    resource graphs. The post-migration audit reports zero mismatches and twelve
    documented source/schema gaps; report:
    `/private/tmp/cycle-parity-voice-controls-final.json`. Live parameter
    gestures also schedule a final durable-graph refresh after their causal
    local-state commit, so an in-flight transient refresh cannot leave the
    canvas on the preceding Voice Context configuration. The focused Voice
    Context fixture verifies two slider updates, the committed node value, and
    the downstream duration preview; artifact:
    `/private/tmp/cycle-v2-voice-context-attachments.png`.

    A precision follow-up is complete. The converter and first audit rounded
    Cycle 1's normalized Voice Length to five decimal places. That small
    duration error can move a cycle-clocked Envelope across a loop boundary,
    as the Icycle frame-64 diagnostic demonstrated. Voice Length now preserves
    the canonical Cycle 1 JSON number unchanged. All 216 eligible factory
    graphs were updated only at that Voice Context scalar; an object-level diff
    confirms no node presentation, edge, probe, or other parameter changed.
    Every checked-in audio-equivalence manifest now identifies its resulting
    graph exactly, and the final audit again reports zero mismatches with the
    same twelve explicit source/schema gaps. Report:
    `/private/tmp/cycle-parity-voice-controls-precision-final.json`.

    The Astral preset sub-slice is complete. The existing graph was a stale,
    rounded hand port: it inverted Cycle 1's inverse-velocity morph, omitted
    spectral layer modes and authored static Envelope morph values, represented
    phase pan through a different node, and omitted the master gain. It also retained
    only rounded mesh coordinates. Regenerating through the authoritative
    converter restores those values and topologies while preserving positions,
    port sides, editor sizes, and probes for every retained node. The two new
    boundary nodes are placed below the existing graph without overlap. This
    preset-only migration leaves Delay routing unchanged for the parallel
    global-audio TDD. The old graph is effectively silent under the MIDI 48
    differential request; the regenerated graph is zero-lag with `1.00000`
    correlation, `0.0011` gain-matched residual, `0.35 dB` spectral RMSE, and
    exact Cycle V2 repeatability. Cycle 1 fails the repeat-render prerequisite,
    so `scripts/fixtures/cycle-astral-audio-equivalence.json` remains diagnostic.
    Artifacts: `/private/tmp/cycle-astral-before-port/` and
    `/private/tmp/cycle-astral-after-port/`.

    The factory Envelope-ownership migration is complete. Its first pass
    corrected 49 active-volume declick flags, eleven neutral declick fallback
    routes, 35 authored base-morph values, and two stale active-purpose routes.
    A differential rerun then disproved the assumption that removing the old
    zero-morph cables preserved Cycle 1 audio. Cycle 1 writes routed red/blue
    values only to `SmoothedParameter` targets, while its empty
    `SynthesizerVoice::updateSmoothedParameters()` never advances the current
    values used by `EnvRasterizer`; factory Envelopes therefore render at their
    initialized `0/0` cross-section. This is an inherited Cycle 1 defect, not
    the intended V2 Envelope contract.

    V2 keeps normal implicit Voice Context modulation for newly authored
    Envelopes. The 191 affected factory graphs instead express Cycle 1
    compatibility explicitly through one zero-valued `legacyEnvelopeMorph`
    source connected to 361 active Envelopes. Existing nodes and edges, canvas
    positions, port sides, editor sizes, probes, Guides, audio bindings, and
    global topology are byte-equivalent to the pre-migration objects; all new
    nodes are collision-free. The final audit reports zero findings across 219
    eligible pairs and nine boundary-only skips. Guitar 3 G MIDI 48 improves
    from lag `-451`, correlation `0.30556`, and residual `0.9522` to zero lag,
    correlation `1.00000`, and residual `0.0008`. Icycle improves from
    correlation `0.97486` and residual `0.2228` to zero lag, correlation
    `0.99996`, and residual `0.0087`. Cycle V2 repeats exactly; the documented
    Cycle 1 process-level variation keeps both fixtures diagnostic. Reports:

    Product update (2026-09-13): exact preservation of Cycle 1's stuck zero
    Envelope morph is no longer desired. The `legacyEnvelopeMorph` nodes and
    routes are removed from factory content and conversion tooling in favor of
    normal implicit Voice Context modulation. Audio comparisons involving those
    Envelopes should expect this intentional divergence.
    `/private/tmp/cycle-parity-envelope-legacy-morph-migration.json` and
    `/private/tmp/cycle-parity-envelope-legacy-morph-final.json`. Audio:
    `/private/tmp/cycle-guitar-legacy-envelope-rerun/comparison.json` and
    `/private/tmp/cycle-icycle-legacy-envelope-rerun/comparison.json`.

    Fresh full MIDI 36–72 admission checks after this migration retained
    verified status for Flute and Shiny. Fallout, Simple Bass, and Sitar passed
    every cross-version audio threshold, but each produced a different Cycle 1
    floating-point payload on the MIDI 48 repeat while Cycle V2 remained exact;
    their manifests are consequently diagnostic. Artifacts:
    `/private/tmp/cycle-fallout-legacy-envelope-verified/comparison.json`,
    `/private/tmp/cycle-flute-legacy-envelope-verified/comparison.json`,
    `/private/tmp/cycle-shiny-legacy-envelope-verified/comparison.json`,
    `/private/tmp/cycle-simple-bass-legacy-envelope-verified/comparison.json`,
    and `/private/tmp/cycle-sitar-legacy-envelope-verified/comparison.json`.

    The final octave audit corrected a quantization error in the converter.
    Cycle 1's `OscControlPanel::scaleOctave()` applies `roundToInt()` to
    `4 * (unit - 0.5) + 0.5`; the converter instead folded the separately
    translated legacy MIDI reference into the stored Voice Context octave.
    That happened to preserve the common centered value but lowered twelve
    non-centred factory presets by one octave. The converter is now the single
    authority for that rounding, and the factory audit reuses it. Only the
    Voice Context octave changed in the other eleven graphs; Accoustic was
    separately regenerated from its canonical source below. All 216 eligible
    pairs again report zero control mismatches, with the same twelve explicit
    source/schema gaps. Report:
    `/private/tmp/cycle-parity-voice-controls-octave-final.json`.

    The correction is independently audible. Accoustic MIDI 48 originally
    compared Cycle 1's adjusted MIDI 72/256-sample time raster against Cycle
    V2's MIDI 48/512-sample raster and reached only `0.19646` dry correlation.
    With octave one, both use the same pitch boundary and the dry comparison is
    zero-lag at `0.99733` correlation and `0.0730` residual. Subbass similarly
    improves to zero-lag `0.99997` correlation and `0.0077` residual when its
    graph octave moves from `-2` to `-1`. Artifacts:
    `/private/tmp/cycle-accoustic-octave-one-dry/comparison.json` and
    `/private/tmp/cycle-subbass-octave-corrected/comparison.json`.

    Cycle 1 has no base-pitch or portamento control, so the imported `0` and
    disabled values are deliberate neutral V2 state rather than missing parity
    behavior. Realtime oscillator oversampling remains an unimplemented V2
    feature, but it does not create a current factory-audio gap: Stengah is the
    only non-default imported case and its pure spectral voice never enters
    Cycle 1's time-cycle oversampling/downsampling branch. Implementing those
    three controls as new V2 features belongs in a separate TDD.

55. Reconcile Accoustic against its current Cycle 1 document. Complete: the
    authoritative implementation is the canonical live export interpreted by
    `port_cycle_v1_preset.py`; mesh rasterization, spectral modes, Envelope
    behavior, and effect DSP remain unchanged. The translation replaces stale
    rounded mesh/Guide data and persisted effect/output values while retaining
    every existing node position, port side, editor dimension, probe, and the
    merged global graph topology. No compatibility adapter or copied DSP is
    introduced; the stable end state is the normal converted graph, and this
    preset-specific reconciliation has no deletion target.

    The regenerated graph restores explicit additive/multiplicative spectral
    modes, Cycle 1's 512-sample IR size (canonical V2 value `2/7`),
    full-precision Reverb controls, Output gain `0.557251908`, full-precision
    meshes/Guides, and the corrected octave one.
    With IR, Delay, and Reverb disabled, MIDI 48 is repeatable in both engines,
    zero-lag, and reaches `0.99733` correlation; the first numerical difference
    is the time raster. With the complete global chain enabled, both engines
    remain repeatable and reach `0.98559` correlation, `0.1692` residual, and
    `0.1300` cyclogram difference, but the `10.94 dB` spectral RMSE exceeds the
    declared threshold. The checked-in manifest is therefore diagnostic.
    Artifacts: `/private/tmp/cycle-accoustic-dry/`,
    `/private/tmp/cycle-accoustic-octave-one-dry/`, and
    `/private/tmp/cycle-accoustic-octave-fixed-full/`.

56. Apply Voice Context octave to its inherited key-scale modulation. Complete:
    Cycle 1 routes `normalizeKey(adjustedMidiNote)` after applying its octave,
    while Cycle V2 previously applied octave only at oscillator materialization.
    The compiler now records the owning Voice Context's note offset on each
    synthesized default-modulation buffer, and the existing modulation renderer
    applies it without allocating or copying voice state. Explicit standalone
    Modulation Source nodes remain in standard MIDI space; this translation is
    limited to defaults inherited from a Voice Context and remains correct when
    a graph contains multiple contexts with different octaves.

    For Accoustic MIDI 48, the red key coordinate now matches Cycle 1 at
    `0.4859813` instead of `0.3738318`. The time raster/frame, FFT, magnitude
    rasters/operands, and phase rasters/operands are byte-identical; the first
    primary difference is inverse reconstruction at `1.58e-7` residual. Dry
    output improves to `0.99840` correlation, `0.0565` residual, and `4.57 dB`
    spectral RMSE. The complete graph at 44.1 kHz reaches `0.99870`
    correlation, `0.0510` residual, and `4.52 dB` spectral RMSE. At 48 kHz its
    long Reverb still accumulates the remaining upstream/output-rate residual
    to `0.98564` correlation and `9.81 dB` spectral RMSE, so the fixture remains
    diagnostic as described by the established Organ 2 Reverb boundary in
    slice 43. Across the complete 48 kHz MIDI 36–72 matrix, both engines repeat
    exactly and correlations remain `0.98483–0.99552`; three notes exceed only
    the spectral threshold. Artifacts:
    `/private/tmp/cycle-accoustic-octave-keyscale-dry/`,
    `/private/tmp/cycle-accoustic-octave-keyscale-full/`, and
    `/private/tmp/cycle-accoustic-octave-keyscale-full-44100/`, and
    `/private/tmp/cycle-accoustic-octave-keyscale-full-matrix/`.

57. Compile the Cycle 1 control interval through Voice Context. Complete:
    Cycle 1 stores `ControlFreq` as a power-of-two order and computes the number
    of oscillator cycles between shared-frame refreshes as
    `max(1, round((1 << ControlFreq) / neutralCyclePeriod))`. Cycle V2 currently
    implements that same cycle-clocked refresh and interpolation behavior, but
    hard-codes the interval to 16 samples. Accoustic stores order 8, so its V2
    morph frames advance at twice the Cycle 1 rate at MIDI 48: captured frame 32
    begins near sample 5393 instead of 10787.

    The stable contract is an explicit Voice Context `controlInterval`
    parameter containing the realized sample interval (`16`, `64`, `256`, or
    `1024`), not the legacy exponent or an approximate Hz label. The compiler
    owns translation from graph text to a prepared integer; prepared spectral
    regions receive it once at construction. The shared `OscillatorLaneCore`
    owns the exact stride calculation and both Cycle 1 and Cycle V2 call it.
    Existing authored V2 graphs default to 16, preserving their current sound.
    Factory migration translates canonical `ControlFreq` with `1 << order` and
    edits Voice Context parameters only, preserving positions, port sides,
    editor/probe state, and global/voice-local topology.

    The shared-DSP, compiler, prepared-runtime, converter, node-factory, and
    editor tests cover the contract. Migration added only this Voice Context
    field to 216 eligible factory graphs: 154 use 64, 59 use 256, and 3 use 16.
    A comparison against pre-migration graph copies found no changes to
    positions, port sides, editor/probe state, edges, or global/voice-local
    topology. The final audit has no mismatches; its 12 gaps remain the existing
    missing/singular-Voice-Context conversion gaps. Three old factory graphs
    without canonical oscillator controls use Cycle 1's default 256 interval;
    two diagnostic presets and all three bundled starter resources explicitly
    retain V2's former 16 interval. Every shipped Voice Context now stores the
    contract. Report:
    `/private/tmp/cycle-parity-voice-controls-control-interval-final.json`.

    Accoustic frame 32 now begins at sample frontier 10787 in both engines,
    rather than V2's former frontier 5393. The time frame and forward FFT are
    exact; evolving raster-coordinate differences are below `8.5e-8` normalized
    residual. With IR, Delay, and Reverb disabled, MIDI 48 reaches `1.00000`
    correlation, `0.0002` residual, and `0.01 dB` spectral RMSE. The complete
    44.1 kHz graph reaches `1.00000`, `0.0001`, and `0.01 dB`, respectively,
    and both engines repeat exactly. At 48 kHz the complete graph improves to
    `0.98627` correlation and `7.61 dB` spectral RMSE, but remains diagnostic
    because Reverb still magnifies the tiny output-rate-boundary difference.
    Artifacts: `/private/tmp/cycle-accoustic-control-interval-frame32/`,
    `/private/tmp/cycle-accoustic-control-interval-full-44100/`, and
    `/private/tmp/cycle-accoustic-control-interval-full/`.

    This slice deliberately retains Cycle 1's minimum-one-cycle boundary for
    shared spectral frames. Cycle 1's time-only `SynthUnisonVoice` remains a
    distinct per-lane chained renderer and does not consume `ControlFreq`; V2's
    `ChainedPerLane` strategy already preserves that contract. A future
    higher-rate design may add sub-cycle updates without changing this stored
    control or the parity strategies.

58. Preserve engine-realized scalar precision in differential manifests.
    Complete: Cycle 1 reads the persisted Voice Length and master level through
    float-valued DSP controls before applying the shared exponential mappings.
    The Cycle V2 compiled graph does the same, but the preset converter computed
    the offline `voiceDurationSeconds` and `v1MasterGain` overrides directly in
    Python double precision. For Organ 2, the stored duration unit `0.404580153`
    therefore became `1.2669864718837067` seconds in the manifest rather than
    the engine-realized `1.26698637008667`. The one-float-ULP clock difference
    is already visible in the frame-32 scratch coordinate and is the first
    unequal time-raster input; the long Reverb makes it material at the 48 kHz
    compatibility boundary.

    The converter now quantizes persisted unit values and mapped scalar results
    exactly where the C++ float boundaries occur. All fourteen equivalence
    manifests received only those realized duration/gain values; factory graphs,
    node presentation, global routing, and DSP remain unchanged. Focused
    converter coverage guards the exact Organ 2 values.

    Organ 2's frame-32 scratch coordinate is now byte-identical, removing the
    manifest error as a raster input. The time raster retains a smaller first
    mismatch (`3.16e-5` normalized residual), so the complete 48 kHz Reverb
    result is materially unchanged and remains diagnostic. This correctly moves
    the next investigation into the mature time-raster boundary rather than the
    Reverb implementation. The complete 44.1 kHz graph passes every audio
    threshold at `0.99986` correlation, `0.0165` residual, and `0.07 dB`
    spectral RMSE; both engines repeat exactly. Artifacts:
    `/private/tmp/cycle-organ-2-realized-scalars-stages/`,
    `/private/tmp/cycle-organ-2-realized-scalars-full-48000/`, and
    `/private/tmp/cycle-organ-2-control-interval-full-44100/`.

59. Restore canonical Organ 2 mesh and guide precision without disturbing the
    merged graph.
    Complete: the Organ 2 graph still contained the shortened decimal payload
    from the initial preset port even though the converter and the canonical
    Cycle 1 export now preserve authored floating-point text. The loss was as
    small as a few times `1e-8` in the synthesis meshes and about `5e-6` in
    guides and the IR curve, but it was the first oscillator-stage mismatch and
    Reverb amplified it at the 48 kHz compatibility boundary.

    Regeneration used the direct Cycle 1 canonical export as the model and
    parameter authority and the current Cycle V2 graph as the presentation
    authority. Node identities, cable endpoint sets, positions, port sides,
    editor sizes, probes, and the merged global/voice-local boundary are
    unchanged. The global Impulse Response remains attached to Global Input;
    Delay and Reverb remain on the same global chain. Removed `link.*` values
    were default-valued presentation properties, and explicit additive
    spectral modes are equivalent to the existing Add-node topology.

    At frame 32, the time raster, time frame, FFT, magnitude and phase rasters
    and operands, post-layer spectrum, and reconstructed frame are now
    byte-identical. The first difference moves to pitch resampling at only
    `5.90e-8` normalized residual. The dry 48 kHz render improves to `0.99999`
    correlation, `0.0041` residual, and `0.03 dB` spectral RMSE. With all
    effects active at 48 kHz, correlation improves from `0.94272` to `0.97076`,
    residual from `0.3336` to `0.2400`, and spectral RMSE from `16.79 dB` to
    `9.48 dB`; both engines repeat exactly, but the Reverb-amplified boundary
    remains diagnostic. At 44.1 kHz the complete graph reaches `1.00000`
    correlation, `0.0031` residual, and `0.03 dB` spectral RMSE, with exact
    repeats in both engines. Artifacts:
    `/private/tmp/cycle-organ-2-canonical-precision-stages/` and
    `/private/tmp/cycle-organ-2-canonical-precision-full-48000/`, and
    `/private/tmp/cycle-organ-2-canonical-precision-full-44100/`.

60. Restore canonical Icycle model precision while preserving the authored
    canvas and global graph. Complete: a fresh converter reconciliation against
    the current Cycle 1 document found shortened decimals in Icycle's meshes,
    two Guide curves, Envelopes, and Impulse Response. The converter remains the
    translation authority; no rasterizer, Envelope, effect, or spy behavior is
    copied or changed.

    The refresh preserves all 27 node identities, every cable endpoint, all
    node positions and port sides, editor dimensions, and the three authored
    probes. The Impulse Response remains global and the existing Global Input
    through IR, Delay, and Output chain is unchanged. Explicit spectral modes
    replace equivalent `auto` inference, while omitted `link.*` properties keep
    their default-true presentation behavior.

    The frame-64 dry diagnostic is byte-identical at every captured oscillator
    stage and remains zero-lag at `0.99996` correlation, `0.0087` residual, and
    `0.15 dB` spectral RMSE. With the full effect graph at 48 kHz it reaches
    `0.99996`, `0.0084`, and `0.15 dB`; Cycle V2 repeats exactly while the
    already documented Cycle 1 process-start variation remains. This is a
    source-fidelity correction rather than a new runtime feature. Artifacts:
    `/private/tmp/cycle-icycle-canonical-precision-stages/` and
    `/private/tmp/cycle-icycle-canonical-precision-full-48000/`.

61. Restore canonical Astral control, mesh, and Guide precision. Complete: the
    previous semantic regeneration fixed Astral's missing and inverted state,
    but a later compact serialization retained shortened mesh/Guide values and
    rounded its authored red morph from `0.373831778764725` to `0.37383`.
    Regenerating from the current, hash-matched Cycle 1 document restores the
    converter's authoritative values without changing DSP implementations.

    All 18 node identities, cable endpoints, positions, port sides, editor
    dimensions, probes, and the merged Global Input/Delay/Output graph are
    preserved. Default-true `link.*` presentation fields are omitted, and the
    phase-pan node records its explicit additive mode instead of equivalent
    topology inference.

    At oscillator frame 32, magnitude and phase rasters and operands plus the
    post-layer spectrum are byte-identical. The first difference is the shared
    inverse-FFT boundary at `1.79e-7` normalized residual. Astral's complete
    MIDI 48 comparison improves from the prior `0.0011` residual and `0.35 dB`
    spectral RMSE to `0.0002` and `0.00 dB`, with `1.00000` correlation and
    zero lag. Cycle V2 repeats exactly; Cycle 1 retains its documented
    process-start variation. Artifacts:
    `/private/tmp/cycle-astral-canonical-precision-stages/` and
    `/private/tmp/cycle-astral-canonical-precision-full-48000/`.

62. Reconcile Japan Drum's remaining canonical controls and authored payload.
    Complete: unlike the preceding precision-only refreshes, the audit found
    three audible stale-port errors. Japan Drum used direct velocity where
    Cycle 1 uses inverse velocity, left Output at its neutral value instead of
    the authored `0.58778626`, and retained shortened values throughout its two
    time layers, three magnitude layers, phase layer, volume Envelope, and four
    assigned Guides. The converter is the authority for all translations.

    The regenerated graph preserves all 15 node identities, every cable
    endpoint, positions, port sides, editor dimensions, probes, and its existing
    Global Input/Output boundary. Explicit additive or multiplicative modes
    replace equivalent operation-node inference; omitted `link.*` values use
    their defaults. The manifest now declares the translated Output control so
    the differential harness compares the master stage at equal gain.

    At frame 32, all mesh rasters, spectral operands, and the post-layer
    spectrum are byte-identical. The shared inverse FFT first differs at only
    `1.50e-7` normalized residual. The complete 48 kHz MIDI 48 render is
    zero-lag with `0.9999999998` correlation, `0.0000177` residual, and
    `0.000163 dB` spectral RMSE. Cycle V2 repeats exactly; Cycle 1 retains the
    already documented fresh-process variation, so the fixture remains
    diagnostic. Artifacts:
    `/private/tmp/cycle-japan-drum-canonical-precision-stages/` and
    `/private/tmp/cycle-japan-drum-canonical-precision-full-48000/`.

63. Reconcile Filter Saw's Envelope payload and Output control. Complete: the
    current converter found that the checked-in graph still carried the older
    scratch-Envelope representation, shortened mesh values, and a neutral
    Output value instead of Cycle 1's authored `0.511450382`. The refreshed
    graph uses the canonical Envelope conversion and records the translated
    Output control in the manifest; it introduces no new playback or
    rasterization behavior.

    All 14 node identities, cable endpoints, canvas positions, port sides,
    editor dimensions, probes, and the Global Input/Output boundary are
    unchanged. The magnitude layer's explicit multiplicative mode replaces
    equivalent Multiply-node inference, and removed `link.*` values are
    presentation defaults.

    Frame 32 matches through spectral reconstruction and first differs in
    pitch resampling at `7.56e-5` normalized residual. Across MIDI 36, 48, 60,
    and 72 at 48 kHz, every comparison is zero-lag with correlation above
    `0.99999999`; residual ranges from `0.0000363` to `0.0001271`, and spectral
    RMSE from `0.0000647` to `0.000159 dB`. Cycle V2 repeats exactly at every
    pitch. Cycle 1 varies only on the MIDI 48 repeat, so the fixture remains
    diagnostic without weakening its repeatability gate. Artifacts:
    `/private/tmp/cycle-filter-saw-current-converter-stages/` and
    `/private/tmp/cycle-filter-saw-current-converter-full-48000/`.

64. Refresh Flute's canonical model payload without weakening its verified
    fixture. Complete: the current converter replaces shortened values in the
    time mesh and Impulse Response and brings the volume Envelope onto the same
    canonical representation used by the rest of the migrated library. No
    routing, processing mode, or DSP implementation changes.

    All ten node identities, every cable endpoint, positions, port sides,
    editor dimensions, probes, and the Global Input through global IR boundary
    are preserved. The omitted `link.*` fields retain default-true presentation
    behavior.

    The complete 48 kHz MIDI 36, 48, 60, and 72 matrix passes every existing
    audio and repeatability check in both fresh processes. Correlation ranges
    from `0.99702` to `0.99976`, residual from `0.0220` to `0.0771`, and
    spectral RMSE from `0.02` to `0.17 dB`. Flute therefore remains a verified
    fixture. Artifact:
    `/private/tmp/cycle-flute-current-converter-full-48000/`.

65. Refresh Shiny's canonical multi-layer and Guide precision. Complete: the
    converter restores the current Cycle 1 values in its time, magnitude, and
    phase meshes plus the assigned Guide curves. Explicit multiplicative and
    additive modes replace equivalent operation-node inference; no renderer or
    spectral-composition behavior changes.

    All 14 node identities, cable endpoints, positions, port sides, editor
    dimensions, probes, and the Global Input/Output boundary are unchanged.
    Default-valued `link.*` presentation fields are omitted.

    The complete 48 kHz MIDI 36, 48, 60, and 72 matrix passes every audio and
    repeatability check in both engines. Every pitch is zero-lag with
    correlation above `0.99999977`; residual ranges from `0.000498` to
    `0.000669`, and spectral RMSE remains below `0.00292 dB`. Shiny remains a
    verified deterministic multi-layer fixture. Artifact:
    `/private/tmp/cycle-shiny-current-converter-full-48000/`.

66. Refresh Guitar 3 G's canonical Envelope and effect-curve payload. Complete:
    the current Cycle 1 export restores full authored values in its magnitude
    and phase meshes, scratch and volume Envelopes, Waveshaper curve, and
    Impulse Response. Explicit additive modes replace equivalent graph
    inference. No effect DSP or Envelope playback logic changes.

    All 17 node identities, cable endpoints, positions, port sides, editor
    dimensions, probes, and the merged Global Input through Waveshaper, IR, EQ,
    Delay, and Output chain are unchanged. Waveshaper and IR remain explicitly
    global. Default `link.*` presentation values are omitted.

    A 1.2-second 48 kHz MIDI 48 render covers the held note, release, and 800 ms
    of continuing global effect tail. It is zero-lag at `0.99999992`
    correlation, `0.000400` residual, `0.00443 dB` spectral RMSE, and both apps
    repeat exactly. The fixture remains diagnostic pending its existing full
    pitch-matrix admission check; this slice does not infer admission from one
    pitch. Artifact:
    `/private/tmp/cycle-guitar-3-g-current-converter-full-48000/`.

67. Refresh Simple Bass's canonical Guide and Envelope payload. Complete: the
    current converter restores engine-distinct values in both assigned Guides
    and the volume Envelope, plus canonical time, magnitude, and phase mesh
    serialization. Explicit spectral modes replace equivalent operation-node
    inference; no DSP behavior is reimplemented.

    All 15 node identities, cable endpoints, positions, port sides, editor
    dimensions, probes, and the Global Input/Output boundary are unchanged.
    Default `link.*` presentation fields are omitted.

    Frame 32 matches through reconstructed spectral output and first differs at
    the pitch-resampling boundary. The complete 48 kHz MIDI 48 render is
    zero-lag at `0.99999999996` correlation, `0.00000945` residual, and
    `0.0000214 dB` spectral RMSE; both apps repeat exactly in this run. The
    fixture remains diagnostic pending a fresh full-pitch admission matrix,
    because earlier matrices exposed intermittent Cycle 1 process variation.
    Artifacts: `/private/tmp/cycle-simple-bass-current-converter-stages/` and
    `/private/tmp/cycle-simple-bass-current-converter-full-48000/`.

68. Refresh Sitar's canonical Guide and spectral-stack payload. Complete: the
    current Cycle 1 document restores engine-distinct values in six Guides and
    its four magnitude layers plus phase layer. These models continue through
    the shared mature Guide preparation and spectral-stack implementation; no
    local approximation is introduced.

    All 14 node identities, cable endpoints, positions, port sides, editor
    dimensions, probes, and the Global Input/Output boundary are unchanged.
    Omitted `link.*` fields retain their presentation defaults.

    At frame 32, all captured mesh rasters and spectral composition stages are
    byte-identical; pitch resampling first differs at `0.000674` normalized
    residual. The complete 48 kHz MIDI 48 render is zero-lag at `0.99996`
    correlation, `0.0088` residual, and `0.39 dB` spectral RMSE, with exact
    repeats in both apps in this run. The fixture remains diagnostic pending a
    fresh full-pitch admission matrix because Cycle 1 has varied in previous
    matrices. Artifacts: `/private/tmp/cycle-sitar-current-converter-stages/`
    and `/private/tmp/cycle-sitar-current-converter-full-48000/`.

69. Repair Saw's voice/global boundary and admit the minimal fixture. Complete:
    the merged graph retained the time mesh and Voice Output nodes but lost the
    cable between them. Its voice-local graph therefore generated no signal for
    the disjoint global graph. The stale preset also used direct velocity and a
    neutral Output control instead of Cycle 1's inverse velocity and authored
    `0.534351145` value. Regeneration from the hash-matched current document
    restores those authoritative semantics and the full-precision time mesh.

    Existing node identities and positions, port sides, editor dimensions,
    probes, and the Global Input → Output cable are preserved. The only topology
    addition is the required `timeLayer1.out → voiceOutput.time` boundary cable;
    no global node is connected back into the voice-local graph.

    The complete 48 kHz MIDI 36, 48, 60, and 72 matrix passes every audio and
    repeatability check in both apps. Every pitch is zero-lag above
    `0.9999999999` correlation; residual ranges from `0.00000167` to
    `0.0000132`, and spectral RMSE remains below `0.000100 dB`. Saw is now a
    verified minimal time-mesh fixture. Artifact:
    `/private/tmp/cycle-saw-current-converter-full-48000/`.

70. Replace the stale handcrafted Subbass parity graph with the current
    canonical translation. Complete: the old diagnostic retained an octave of
    `-2`, control interval `16`, voice length `0.375`, noncanonical morph
    constants, a neutral Output, and a pre-migration `volumeEnvelope` model.
    The hash-matched Cycle 1 export instead requires octave `-1`, interval `64`,
    voice length `0.215392441`, the canonical Envelope model plus the explicit
    zero-valued legacy morph, and Output `0.496183206`.

    This is a representation migration, not a new synthesis implementation.
    Existing node presentation remains authoritative: the compatibility
    boundary maps the legacy singular Envelope IDs to their canonical first
    indexed IDs for position, port-side, and editor-size transfer only. It
    never carries parameters or models across that boundary, and focused
    coverage guards the negative semantic boundary. The old Envelope's
    `2050,180` position is retained; the new legacy-morph node is collision-free.
    Existing Voice Output and Global Input → Output routing is unchanged.

    Frame 32 is byte-identical at every captured oscillator stage. The complete
    requested MIDI 36, 48, 60, and 72 matrix at 48 kHz passes every audio and
    repeatability check in both apps. Correlation remains above
    `0.9999999998`, residual ranges from `0.00000206` to `0.0000145`, and
    spectral RMSE remains below `0.0000584 dB`. Subbass is now verified.
    Artifacts: `/private/tmp/cycle-subbass-current-converter-stages/` and
    `/private/tmp/cycle-subbass-current-converter-full-48000/`.

71. Share Cycle 1's interpolated-frame position boundary. Complete: Cycle
    1's mature `CycleBasedVoice::renderInterpolatedCycles()` computes a
    single-lane position from integer cycle count and a multi-lane position by
    rounding the shared-frame interval to float before division. Cycle V2
    independently reconstructed the multi-lane expression with a double
    denominator. Evolving Organ 2 frames expose the resulting one-ULP composed
    cycle mismatch before pitch resampling, and 48 kHz Reverb amplifies it.

    Extract the existing Cycle 1 expression unchanged into
    `OscillatorLaneCore`; both engines will call it. Cycle V2 translates its
    lane cycle count and shared-frame positions at that boundary and retains no
    local interpolation formula. The shared core owns arithmetic only; frame
    scheduling, buffer lifecycle, rendering, and effects remain with their
    existing owners. The duplicated V2 formula is deleted, focused shared-core
    and host-partition tests pass, and Cycle 1's captured stage payload is
    byte-identical before and after extraction.

    Organ 2 frame 32 is now byte-identical at every captured stage, including
    the composed and pitch-clocked cycle that previously differed around
    `5.9e-8` normalized residual. Shiny's verified MIDI 36–72 matrix remains
    deterministic and within its prior near-exact bounds. The complete Organ 2
    dry and Reverb renders remain numerically unchanged, which falsifies this
    primary-lane interpolation mismatch as the source of the audible 48 kHz
    Reverb gap. The next localization boundary is the uncaptured non-primary
    Unison lanes and their mix before global effects; do not change Reverb DSP
    on this evidence. Artifacts:
    `/private/tmp/cycle-organ-2-shared-frame-portion-stages/`,
    `/private/tmp/cycle-organ-2-shared-frame-portion-full-48000/`, and
    `/private/tmp/cycle-shiny-shared-frame-portion-full-48000/`.

72. Observe individual Unison lanes at the existing spectral capture boundary.
    Complete: Organ 2 is exact through the shared reconstructed frame and
    its first pitch-clocked lane, while its complete dry render retains a small
    residual that Reverb amplifies. The current diagnostic sink suppresses all
    non-primary lanes in both engines, so it cannot distinguish lane synthesis
    from the downstream mix.

    Extend `SpectralStageCaptureRecorder` with a selected lane index and carry
    that index as capture metadata. The mature renderers continue to own lane
    synthesis and merely publish each pitch-clocked lane to the diagnostic
    sink; the recorder remains the sole owner of selection, allocation, and
    serialization. Shared pre-lane stages retain lane zero for backward
    compatibility. Both automation endpoints and the comparison harness
    translate one optional `stageCaptureLaneIndex` value at the boundary. This
    is diagnostic-only: it must not alter scheduling, oscillator state, mixing,
    effect processing, or production graph semantics. Delete the renderer-side
    lane-zero filters, cover selection and metadata in the shared recorder, and
    compare every Organ 2 lane before changing DSP.

    At frame 32, lanes zero and one are byte-identical in both channels. Lanes
    two and three are the first unequal boundary: their Cycle 1/V2 frontiers
    are respectively `10411/10748` and `10361/10698`, and their resampled-cycle
    residuals are about `0.0250` and `0.0242`. Their composed-cycle residuals
    remain below `0.00045`. This rules out Reverb, panning, and the summation
    itself as the first divergence and identifies the non-primary lane
    scheduling boundary. Artifacts:
    `/private/tmp/cycle-organ-2-lane-0-stages/` through
    `/private/tmp/cycle-organ-2-lane-3-stages/`.

73. Align multi-lane saturation with Cycle 1's shared-frame scheduler.
    Complete: Cycle 1 renders every Unison lane, in lane order, until each has
    reached the current future-frame position; only the primary lane then
    permits the next shared frame to advance. Cycle V2 instead selects the
    chronologically earliest lane and asks the shared renderer to look one
    complete frame interval ahead before every cycle. Faster non-primary lanes
    can therefore consume a later shared-frame pair than Cycle 1.

    Extract Cycle 1's existing saturated-frame and within-frame predicates to
    `OscillatorLaneCore` and call them from the mature renderer unchanged.
    Cycle V2 will use those shared predicates to orchestrate multi-lane groups
    in Cycle 1 lane order while retaining its existing single-lane path, ring
    buffers, envelope bank, cycle compositor, Hermite resampler, and output
    mix. Delete the chronological earliest-lane selection for multi-lane
    spectral regions. The intended stable state is one shared owner for the
    scheduling predicates and app-local lifecycle/buffer orchestration; no DSP
    transfer function or effect behavior is copied.

    Organ 2's frame-32 lanes now share the Cycle 1 frontiers: lanes two and
    three move from `10748/10698` to `10411/10361`; lane three is byte-identical
    and lanes one and two retain only about `2e-7` normalized payload residual.
    With Reverb disabled, the complete MIDI-48 output improves from roughly
    `0.0031` residual to `3.97e-7`, `0.99999999999995` correlation, and
    `0.000081 dB` spectral RMSE, with both engines byte-repeatable. The six-lane
    Icycle MIDI 36–72 matrix improves to `2.18e-6–7.89e-6` residual and below
    `0.0011 dB` spectral RMSE; Cycle 1 retains its previously documented
    intermittent MIDI-48 process variability. Organ 2's full Reverb output is
    still diagnostic because that global processor amplifies the remaining
    sub-micro residual. Artifacts:
    `/private/tmp/cycle-organ-2-top-saturation-lane-1-stages/` through
    `/private/tmp/cycle-organ-2-top-saturation-lane-3-stages/`,
    `/private/tmp/cycle-organ-2-top-saturation-dry/`, and
    `/private/tmp/cycle-icycle-shared-saturation-full/`.

74. Guard Astral's realtime pitch against host-block cadence.
    Complete. The investigation exposed two independent failures. First,
    `NodeWorkspace::loadGraphFromFile()` changed the canvas document but
    deferred audio-plan publication to its 30 Hz timer. The earlier one-shot UI
    captures therefore exercised the startup graph, not Astral, and were
    invalid evidence about Astral's live behavior.

    Successful document loads now publish the newly compiled plan immediately.
    Timer-driven publication remains authoritative for subsequent semantic
    edits and device-preparation changes. The focused
    `cycle-v2-agent-astral-live-publication.json` fixture executes open, note-on,
    and a real audio-device capture in one automation turn, where the message
    timer cannot run. It asserts that the adopted plan has Astral's 17
    executable nodes and one oscillator region. The captured MIDI-60
    fundamental exceeds the 86.13 Hz callback component by 48.3 dB, and the
    same corrected path distinguishes MIDI 72.

    The remaining audible reproduction used the Astral graph from the
    Amaranth2 sister worktree rather than this branch's canonical Astral. That
    graph, canonical Dirty Guitar 2, and Time contain Add operations with one
    input intentionally unconnected. The ordinary block processor treats the
    missing Add input as zero, but both prepared oscillator recipes previously
    required two inputs. Their silent preparation failure exposed the ordinary
    IFFT processor at host-block cadence, producing a comb at 44.1 kHz / 512 =
    86.13 Hz. Prepared spectral and chained Add operations now preserve either
    lone operand. The exact sister Astral MIDI-72 capture moves from 10.5 dB
    below the callback component to 31.7 dB above it. Generic identity tests
    and Dirty Guitar 2/Time factory tests guard the prepared path. Artifacts:
    `/private/tmp/cycle-v2-astral-live-publication.wav` and
    `/private/tmp/cycle-v2-astral-live-publication-report.json`, plus
    `/private/tmp/cycle-v2-amaranth2-astral-note72.wav` and
    `/private/tmp/cycle-v2-amaranth2-astral-fixed-note72.wav`.

75. Restore Cycle 1 individual-Unison and Visual DSP load behavior.
    Complete. `Blinding.cyc` migrated ten individual voices with alternating
    hard-left/hard-right pan, but the generic post-load singleton reset invoked
    `Unison::reset()` through `SingletonAccessor` and erased them. The preset
    loader now calls the explicitly named `resetParameters()` while lifecycle
    reset remains the inherited no-op. Selecting an individual voice now only
    presents its controls and no longer republishes unchanged DSP parameters
    while the complete voice array is pending. The focused live capture retains
    all ten voices and reports a left/right difference RMS of `0.11789` instead
    of bit-identical channels.

    The mature pre-extraction graphic rasterizer selected the current morph
    axis for every render through its primary-dimension provider. The narrow
    Cycle 1 wrapper remains the compatibility boundary: it now translates the
    live setting into the shared rasterization request for both publishing and
    render-only entry points, without copying slicing or Guide behavior. This
    is the stable adapter end state; the shared trilinear slicer and Guide policy
    continue to own all domain behavior. Visual DSP's magnitude and phase
    column loops also reuse the established direct morph-update contract from
    `TimeColumnRasterizer`, so each red-axis column updates the current value
    rather than only a smoothed target. A Cello regression verifies that the
    authored phase surface changes across red and that removing its attached
    Guide changes the rendered intercepts.

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
