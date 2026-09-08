# Audio Bug Notes

## Resolved: Cycle 1 reverb never prepared its convolution buffers

Context:

- Cycle 1 accepted reverb parameter edits, but every audio block returned
  before convolution because the reverb output buffers remained empty.
- Legacy's audio manager forwarded the device block size to both convolution
  effects during preparation. The current impulse modeller retained equivalent
  `AudioHub` wiring, but the reverb forwarding call was lost in the port.
- The Cycle test source glob also omitted tests nested below
  `Audio/Effects/tests`, leaving the existing equalizer test and the new
  reverb boundary test undiscovered.

Resolution:

- `SynthAudioSource::prepareToPlay()` now publishes the current block size to
  the reverb's existing pending-action boundary, matching legacy ownership and
  keeping allocation out of the preparation caller.
- The direct effect test feeds a stereo Dirac impulse through the production
  kernel and convolver and requires a finite, nonzero, multi-block stereo tail.
- `scripts/test_cycle1_reverb_tail.py` renders Anasound dry and wet through the
  full application at 128-, 512-, and 1024-sample device blocks. The dry signal
  is silent after its 10 ms declick; the wet signal consistently measures
  `0.0264` RMS from 110–200 ms and `0.00136` RMS from 160–200 ms.
- Cycle now derives nested test sources from the authoritative application
  source list. The previously dormant equalizer test explicitly completes its
  smoothed parameter transition before measuring its final response.

Current status: resolved on 2026-09-08.

## Resolved: No-release volume envelopes truncated the declick tail

Context:

- Anasound has declick enabled and an active volume envelope whose sustain
  marker is at the end, so it has no authored release segment.
- Cycle 1 rendered the 10 ms fade into a buffer shortened to the remaining
  ramp length, then added it to the unshortened MIDI render-segment view.
  Accelerate rejects unequal `Buffer::add` lengths, so the entire final fade
  block was discarded; the legacy IPP path happened to process the prefix.
- The audible tail therefore varied with note-off position in the audio block.
  At 48 kHz, the 800 ms case emitted only seven samples after note-off instead
  of the intended roughly 480-sample declick.
- The legacy project has the same note-off ordering defect.

Resolution:

- The final voice mix now narrows the destination to the rendered fade length,
  preserving the existing envelope and declick lifecycle unchanged. The same
  boundary correction protects shortened oscillator-latency flushes.
- `scripts/test_cycle1_anasound_declick.py` covers 50, 150, 400, and 800 ms
  notes at the preset's authored gain and requires signal through the middle of
  the declick interval plus a continuous terminal transition.

Current status: resolved on 2026-09-07.

## Resolved: Cycle 1 phase offsets were discarded on Accelerate

Context:

- Acidic loads two valid phase layers panned hard left and right, but its dry
  output channels were bit-identical.
- Both layers rasterized distinct offsets and accumulated distinct channel
  spectra. The voice then added each active-bin buffer to the full maximum-size
  phase allocation. Accelerate rejects that buffer-size mismatch, so the add
  was a no-op; the legacy IPP path happened to process the shorter prefix.

Resolution:

- Phase offsets are added through the existing note-sized `phaseBufs` views.
- `scripts/test_cycle1_spectral_phase.py` now requires Acidic to retain
  side-channel energy. Its side/mid RMS ratio is `2.45` at 44.1 kHz and `2.40`
  at 48 kHz, up from exactly zero.

Current status: resolved on 2026-09-07.

## Resolved: Cycle 1 unison growth invalidated prepared voice states

Context:

- Ping was silent with its authored seven-voice unison and audible when unison
  alone was bypassed. Delay and the preset's oscillator/envelope data were not
  involved.
- Voice rasterizers were prepared while the unison order was one. Note start
  then grew the per-unison cycle-state collection on the audio thread, after
  snapshot preparation, so every multi-voice rasterization failed safely to
  silence.

Resolution:

- Unison order changes now resize cycle storage and reprepare both Cycle 1
  oscillator rasterizers under the existing audio lock, before realtime note
  rendering. Note start observes the prepared count and performs no allocation.
- The spectral integration renders Ping with its authored effects and requires
  audible output. Its steady RMS is `0.0689` at both 44.1 and 48 kHz.

Current status: resolved on 2026-09-07.

## Resolved: Cycle 1 retained FFT bins above the note's harmonic limit

Context:

- Calming produced note-dependent high-frequency buzzing on its displayed A1,
  G1, F1, and E1 keys despite its low-harmonic magnitude surface. The strongest
  unintended content clustered near Nyquist.
- Cycle 1 copied only the note-valid magnitude and phase prefix into its
  reusable transform before inverse FFT. Bins above that prefix retained the
  unfiltered forward-transform content. Legacy explicitly zeroes those bins,
  and the current visual DSP already preserved that behavior independently.

Resolution:

- The mature visual tail-clear operation now lives in `SpectralLayerCore` and
  is shared by the realtime voice and visual inverse transforms.
- `scripts/test_cycle1_calming_spectrum.py` renders A1, G1, F1, and E1 at 44.1
  and 48 kHz, requires audible steady output, and limits power above 3 kHz.
- At 44.1 kHz, the four high-band ratios fell from between `5.66e-6` and
  `2.94e-4` to between `1.52e-9` and `4.31e-9`.

Current status: resolved on 2026-09-07.

## Resolved: Cycle 1 voice-time slices remained on the first yellow plane

Context:

- PWM rendered nearly stationary harmonic ratios, Dunk2 did not audibly leave
  its short initial cubes, and BrightLead3's scratch-enabled and linear-time
  variants were much more alike than their authored surfaces imply.
- The voice implementations copied each layer's `MorphPosition` and assigned
  the new time to its `SmoothedParameter`. That assignment changes only the
  smoothing target; the rasterizer reads the unchanged current value, so every
  cycle continued slicing at time zero.
- The same mistake affected ordinary time layers and the filter voice's
  time-domain source. Legacy constructs each raster position with the sampled
  voice time as its current value.
- Note initialization also calculated the interpolation stride from the
  previous note's retained period, or zero on the first note, before installing
  the new note's period. This inherited legacy defect gave first and subsequent
  notes different rasterization cadences.

Resolution:

- Time-domain voice rasterizers now use the existing `MorphPosition::withTime`
  boundary, which creates a position whose current time is the sampled scratch
  or linear voice time.
- Interpolation stride now derives from the current note's middle period.
- `scripts/test_cycle1_time_evolution.py` verifies early-to-late spectral
  evolution in Dunk2 and PWM, material scratch-envelope differences in PWM and
  BrightLead3, and correspondence between two PWM notes in one process.

Current status: resolved on 2026-09-07.

## Resolved: Visible spectral domain contaminated layer enablement checks

Context:

- Disabling the last phase layer while the Phase domain was visible silenced a
  spectral-only preset, even though its independent magnitude layer remained
  enabled and its powered icon still appeared active after switching domains.
- `Spectrum3D::haveAnyValidLayers()` accepted a magnitude/phase selector but
  ignored it and inspected the currently visible layer group for both queries.
  Voice enablement therefore depended on editor presentation state.
- Legacy selects the magnitude or phase collection directly from the query.

Resolution:

- Spectral validity now selects `GroupSpect` or `GroupPhase` from the requested
  domain, independent of the visible editor mode.
- The focused OohAah automation disables its phase layer while Phase remains
  visible and requires audible magnitude-only output.

Current status: resolved on 2026-09-07.

## Resolved: Cycle 1 spectral voice omitted time-rasterizer preparation

Context:

- Acidic and Anasound2 produced exact silence, while Baroque Flute became a
  thin residual with its phase layer active. Disabling that phase layer raised
  Baroque Flute's steady-state power by roughly `864x`.
- The extracted `VoiceRasterizer` requires its mesh and retained storage
  capacity to be prepared before realtime rendering. `SynthesizerVoice`
  prepared only the direct/unison voice and omitted the spectral-filter voice.
- Acidic and Anasound2 depend on time-domain meshes followed by subtractive
  spectral layers. Their failed time rasterization therefore supplied a zero
  spectrum which subtractive processing could not restore. Baroque Flute's
  additive layers left only a phase-sensitive residual.
- The polar FFT itself preserves signal norm under arbitrary phase changes;
  the phase transform was not the source of the lost harmonic power.

Resolution:

- The existing voice-preparation lifecycle now prepares both oscillator
  implementations, reusing the shared `CycleBasedVoice` preparation path.
- `scripts/test_cycle1_spectral_phase.py` asserts audible dry output from
  Acidic and Anasound2 and compares Baroque Flute's steady-state power with its
  phase layer enabled and disabled at both 44.1 and 48 kHz.
- Baroque Flute's phase-disabled/enabled power ratio is now `1.04` to `1.06`
  across those rates, while both formerly silent presets render nonzero audio.

Current status: resolved on 2026-09-07.

## Resolved: Cycle 1 spectral notes accumulated DC across voice reuse

Context:

- The first OohAah note after launch sounded correct, but a second equal note
  in the same process became severely distorted even after the first release
  reached silence. With Delay and Unison disabled, the pre-capture peak rose
  from about `0.16` to `4.30` at a master value of `0.1`.
- The prior OohAah release integration launched a fresh process for every note
  length, so it could not exercise reuse of the same synth and FFT instances.
- Legacy cleared the packed DC slot before each inverse spectral transform.
  The reimplementation's reusable `Transform` instead retained the preceding
  inverse transform's first time-domain value and interpreted it as DC on the
  next cycle.

Resolution:

- Cycle 1's synth-owned spectral transforms now use the existing DC-removal
  mode, restoring the legacy zero-DC inverse-transform contract.
- `scripts/test_cycle1_ooh_aah_repeat_note.py` renders two equal OohAah notes
  per process and compares their 5 ms RMS amplitude contours. Its default
  matrix covers three note lengths at both 44.1 and 48 kHz.

Current status: resolved on 2026-09-07.

## Resolved: Cycle 1 volume-envelope completion hard-cut a nonzero tail

Context:

- The OohAah release was continuous at MIDI note-off, but the envelope's last
  discrete sample remained nonzero before `SynthesizerVoice` retired the voice.
  At the preset's quiet test gain, the final jump to exact silence measured
  between `0.00055` and `0.00196` across 50, 150, 400, and 800 ms notes.
- The previous integration check used one note length and only the maximum
  adjacent delta over the complete render, so oscillator content could hide
  this smaller event-specific discontinuity.
- OohAah stores `Declick = 0`; its authored volume release must therefore end
  continuously without relying on the optional immediate note-off declick.

Resolution:

- Cycle 1 now aligns the existing release-declick curve with the final samples
  of an authored volume release. This terminal continuity rule is independent
  of the optional note-boundary declick setting and does not allocate on the
  audio thread.
- Offline capture metrics now report the second difference at each scheduled
  note-off and the final nonzero-to-zero delta separately.
- `scripts/test_cycle1_ooh_aah_release.py` launches a fresh Cycle process for
  each note length to avoid shared offline-render state. All four OohAah cases
  retain a smooth note-off and reduce the terminal delta below `1e-8`.

Current status: resolved on 2026-09-07.

## Resolved: Cycle V2 keyboard note-off could retain voices indefinitely

Context:

- The realtime renderer treated every Envelope processor as the audible voice
  tail owner, including scratch, pitch, and general control envelopes.
- An envelope without a release curve ignored note-off and remained active.
  A graph with no volume envelope could consequently retain the keyboard voice
  after mouse-up when another envelope processor existed.

Resolution:

- The compiler now marks only volume envelopes as voice-tail owners, and the
  realtime executor bases release retirement on that semantic marker.
- Envelope processors without a release curve become inactive at note-off.
  Graphs without a volume tail receive an immediate sample-offset reset;
  volume-envelope graphs remain alive only until their release completes.
- Focused processor and renderer tests cover no-release, no-volume, and normal
  release completion. The performance-keyboard mouse-down/drag/mouse-up fixture
  also returns `performance.activeVoiceCount` to zero.

Current status: resolved on 2026-09-07.

## Resolved: Bipolar envelope release scaling introduced a note-off discontinuity

Context:

- Legacy normalized a release curve to the envelope level held at note-off, but
  its `0.5` denominator floor assumed the synthetic release point created for
  unipolar envelopes.
- The extracted playback policy retained that floor while also serving bipolar
  envelopes, whose real release-start value may legitimately be below `0.5`.
  A release beginning at `0.25` therefore jumped to half the held level.

Resolution:

- Release scaling now divides by the actual sampled release-start value. A zero
  release value retains unity scaling because no finite scale can make a zero
  start continuous with a nonzero held value.
- A focused bipolar playback regression holds an envelope at `0.75`, releases
  into a curve beginning at `0.25`, and requires the first release sample to
  remain exactly `0.75`.

Current status: resolved on 2026-09-07.

## Resolved: Cycle 1 ignored volume-envelope amplitude and clicked at note boundaries

Context:

- OohAah advanced its active volume envelope and used its completion to stop
  the voice, but `SynthFlag::haveVolume` was never enabled. The rendered audio
  therefore stayed at full amplitude until a one-sample stop at the end of the
  release.
- The dormant multiplication path referenced an `EnvRenderContext` buffer that
  was never populated instead of the existing `EnvRasterizer` playback output.
- Cycle generation also advanced the volume envelope before the sample-rate
  amplitude stage, skipping most of OohAah's authored attack before the first
  audible block.

Resolution:

- The sample-rate voice boundary now renders and immediately applies the first
  local volume envelope from `EnvRasterizer`; volume is excluded from the
  pitch/scratch cycle-update loop so it has one playback owner.
- Envelope timing uses the prepared voice sample rate, and layer property
  lookups preserve each local context's actual library index.
- The OohAah regression measures the first 50 ms and the maximum adjacent
  sample delta. At 44.1 kHz the latter fell from 0.152 to about 0.018 at the
  quiet test gain, with a gradual attack and release.

Current status: resolved on 2026-09-06.

## Resolved: Cycle 1 generalized envelope groups lost legacy playback contracts

Context:

- `CycleBasedVoice::initialiseNote()` gated the pitch envelope's initial sample
  on `SynthFlag::havePitch`, but the generalized envelope initialization never
  assigned that flag.
- The generalized volume renderer also reset a note when no local volume
  envelope was enabled. Legacy bypassed volume multiplication and allowed the
  oscillator to continue in that case.
- Pitch and volume property translation must retain the source layer index;
  the local rasterizer-vector position is not a library layer index.

Resolution:

- Envelope initialization now records whether the first local pitch envelope
  is sampleable, and pitch updates use that state plus the context's source
  layer index.
- An absent, inactive, or unsampleable volume envelope bypasses multiplication
  without stopping the voice. Only completion of an enabled rendered volume
  envelope ends the note.
- Focused renders keep a Saw note with no active volume envelope audible through
  the final 50 ms and keep the active pitch-envelope Drunkard preset audible.

Current status: resolved on 2026-09-06.

## Resolved: Cycle 1 voice-mode changes stopped held notes

Context:

- Legacy transferred oscillator state with `stealNoteFrom()` when an edit
  changed a playing voice between the time-only and spectral implementations.
- The reimplementation retained `stealNoteFrom()` but commented out its only
  caller and stopped the note instead.
- In the focused live-device reproduction, Horn produced RMS 0.413 before its
  magnitude layer was enabled and exact silence afterward while the same UI
  keyboard key remained held.

Resolution:

- `enablementChanged()` again uses the legacy `stealNoteFrom()` path for a held
  note and retains the stop behavior for a releasing voice.
- A focused African Horn live-device fixture holds one UI-keyboard note while
  disabling its spectral layer. RMS remains nonzero before and after the
  spectral-to-time voice switch.

Current status: resolved on 2026-09-06.

## Resolved: Legacy resampler could replay carried MIDI

Context:

- Both legacy and current `convertMidiTo44k()` retain MIDI when an output block
  produces no internal 44.1 kHz samples.
- The retained messages are copied into the next nonempty internal block but
  are not cleared after consumption, so another consecutive nonempty block can
  receive the same events again.
- Normal device block sizes do not exercise the zero-internal-sample case; a
  useful fix needs a tiny-block, high-output-rate scheduling fixture.

Resolution:

- The internal-rate block boundary now owns cumulative sample conversion and
  deferred MIDI as one lifecycle object below `SynthAudioSource`.
- Consecutive zero-internal-sample blocks append their MIDI in order. The next
  nonempty block consumes those messages once and clears the carry.
- Current-block event positions now use the legacy nearest-sample conversion
  directly instead of adding an extra half sample before `roundToInt()`.
- Focused tests exercise consecutive empty internal blocks at 192 kHz, ordered
  note/controller carry, absence of replay, position conversion, and an exact
  cumulative 44,100 internal samples for one second at 48 kHz.

Current status: resolved on 2026-09-07.

## Resolved: Legacy global scratch mixed output and internal sample-rate domains

Context:

- Both trees calculate the global scratch delta at 44.1 kHz but render an
  output-device block's sample count before internal-rate synthesis.
- At device rates other than 44.1 kHz, global scratch therefore advances by
  the wrong duration relative to local scratch and oscillator processing.
- No current factory parity fixture uses a global scratch layer.

Resolution:

- `SynthAudioSource` now passes the internal-rate block length returned by the
  shared block adapter into a dedicated global-envelope render boundary.
- Global and local scratch therefore advance on the same 44.1 kHz timeline,
  independent of device sample rate; neither performs a second rate conversion.
- The internal-rate timing regression accumulates irregular 48 kHz output
  blocks and proves they advance global scratch by exactly one second and
  44,100 samples.

Current status: resolved on 2026-09-07.

## Resolved: Cycle 1 standalone keyboard produced silent device buffers

Context:

- Cycle 1's UI keyboard registered the held MIDI note and its audio-device
  callback advanced, but a callback capture remained exactly zero.
- `AudioSourceProcessor::getNextAudioBlock()` passed JUCE's `startSample` as
  the external buffer constructor's channel count. The normal zero offset
  therefore presented zero channels to every Cycle 1 realtime processor.
- Offline renders call `processBlock()` with an owned stereo buffer and bypassed
  this bridge, so they could not reveal the standalone failure.

Resolution:

- The shared bridge now preserves the device buffer's channel count and passes
  the offset through the four-argument external-buffer constructor.
- Cycle 1 no longer requires an unused audio input device to initialize its
  output-only synth path.
- A focused bridge test checks channels and offset placement. The live Subbass
  keyboard fixture captured 500 ms from 44 callbacks at 44.1 kHz with peak
  0.588 and RMS 0.268.

Current status: resolved on 2026-09-06.

## Open: Cycle V2 tests cannot create IR fixtures in the default temp directory

Context:

- The complete `CycleV2_tests` run on 2026-09-05 failed both cases using
  `writeImpulseWave()` at `TestImpulseResponseResourcePreparation.cpp:47`
  because `File::createOutputStream()` returned null.
- The cable Pan presentation work does not alter IR resource preparation or
  temporary-file handling; all focused Pan tests pass.
- The failure reproduced in a focused `[ir-resource]` run on 2026-09-07 under
  the workspace sandbox, so it is not dependent on full-suite ordering.
- Full-suite log: `/private/tmp/cycle-v2-full-tests.log`.

Current status: open as a sandbox/test-fixture path issue. Give the fixture an
explicitly writable test root and retain the existing resource-preparation
contract.

## Open: Full library order corrupts rasterizer comparison curve resolution

Context:

- The complete `AmaranthLib_tests` run on 2026-08-28 failed
  `TestFXRasterizerPointSource.cpp:158` and `TestWaveformBake.cpp:116` through
  `RasterizerCompare.h:114`.
- The comparisons reported `curveRes` mismatches of `0 == 32` for curve 5 and
  `0 == 975335533` for curve 7. The latter value resembles uninitialized or
  corrupted state.
- Both cases pass independently with 262 and 1252 assertions. The failures are
  therefore full-suite-order dependent and are unrelated to the IR domain-axis
  change, which does not alter a rasterizer implementation.
- The failing full log is `/private/tmp/ir-ticks-lib-full-tests.txt`.

Current status: open; identify the preceding test or shared rasterizer state
that leaves `curveRes` uninitialized before restoring the full library gate.

## Resolved: Native Envelope edit smoke exported non-finite audio samples

Context:

- The default-graph Envelope native sequence encounters non-finite audio before
  its editor gesture. Automation capture now rejects the buffer explicitly
  instead of serializing samples as JSON `null`.
- The original diagnostics inspected only the primary channel and therefore
  misidentified `reverb` as the producer when its stereo mix propagated a
  non-finite secondary channel.
- `assert_audio_changed` consequently failed while comparing the initial and
  post-edit 2048-frame captures. The issue reproduces when the Envelope
  sequence runs alone and is independent of the Stengah Pan/cursor work.

Resolution:

- Resolved linked Trimesh outputs now initialize both concrete stereo channels.
- FFT now expands a linked input across both channels when its outputs resolve
  to a stereo pair, including traversal grids.
- Automation finite checks now inspect both channels. The native Envelope
  sequence passes both audio captures and proceeds to its later UI assertions.

Current status: resolved on 2026-08-16.

## Open: Stengah phase-pan swap parity no longer matches the bundled preset

Context:

- The full `CycleV2_tests` run on 2026-08-04 failed
  `TestChainedOscillatorRegionRuntime.cpp:276` in `Stengah phase layer pans
  survive spectral materialization`.
- Swapping the two authored phase-layer pans produced an L2 channel mismatch of
  `1.810257196` rather than the required value below `1.0e-5`. After restoring
  pitch-log Trimesh sampling on 2026-08-09, the mismatch remains open but is
  reduced to `0.612827003`.
- The failure reproduces in isolation and is outside the spectral traversal
  column normalization changed for probe previews.

Current status: open; reconcile the bundled Stengah phase-layer state and
spectral materialization parity before restoring this full-suite gate.

## Open: Stengah preview omits the authored phase-layer probe

Context:

- The full Cycle V2 suite and a focused rerun on 2026-09-07 fail
  `Stengah scratch topology changes every authored source-layer traversal` at
  `TestGraphAudioExecutor.cpp:1800` because preview results do not contain
  authored probe `probe7`.
- The preceding runtime comparisons pass: scratch changes each expected
  traversal, and the two phase layers retain their opposite stereo channels.
- The migration work does not modify the protected bundled Stengah graph or
  probe-preview construction. Full-suite log:
  `/private/tmp/cycle-v2-full-tests.log`.

Current status: open; trace why `GraphPreviewExecutor` omits this connected
phase probe after successful audio execution before changing the preset or
preview expectations.

## Addressed: Curve FX processor tests omitted required model state

Context:

- The complete `CycleV2_tests "[cycle-v2]"` run on 2026-08-21 reports eight
  failures at `TestNodeAudioProcessor.cpp:206`: the Waveshaper and IR tests
  call `NodeDspConfigurationFactory::create` with no typed model and receive
  `nullptr`.
- The failures cover the existing Waveshaper/IR processor tests, not Guide
  resources; focused Guide graph tests and the migrated Baroque Flute guide
  runtime test pass.

Current status: addressed on 2026-08-21 by supplying canonical typed
Waveshaper/IR models in the shared fixture helper; the full Cycle V2 suite now
passes.
## Resolved: Cycle 1 offline 48 kHz capture used an uninitialized resampler

Context:

- The Cycle 1/Cycle 2 differential render first attempted a 48 kHz capture of
  `filter-saw` on 2026-09-06.
- Cycle 1 crashed in `CircleBuffer::write()` through
  `HermiteState::resample()` because `SynthAudioSource::prepareToPlay()` did
  not initialize its non-44.1-kHz resampling storage.
- The reproduction artifacts are
  `/private/tmp/cycle-filter-saw-parity/midi-36/cycle-v1.log` and
  `/private/tmp/cycle-filter-saw-parity/midi-36/cycle-v1.log.ips`.

Resolution:

- `SynthAudioSource::prepareToPlay()` now initializes the existing Hermite
  resampler whenever the requested rate is not 44.1 kHz. This uses the mature
  `initResampler()` allocation/reset path before the first offline block.

Current status: resolved on 2026-09-06; the paired 48 kHz render is the
integration regression.

## Open: Cycle V2 compiled Voice Context pitch fields are only partially consumed

Context:

- The Subbass differential render showed that the compiled Voice Context
  octave never reached `PreparedOscillatorRegion`; Cycle V2 rendered the graph
  one octave above Cycle 1.
- The octave now becomes an integer MIDI-note offset at the prepared region
  boundary and has a focused equivalence test.
- The neighbouring fractional `pitchSemitones`, `portamento`, and oscillator
  oversampling fields remain compiled without a corresponding realtime
  oscillator consumption path. They are outside the strict Subbass fixture but
  represent the same incomplete Voice Context adoption.

Current status: open for the remaining pitch/glide/oversampling semantics; the
octave path is addressed on 2026-09-06.

## Open: Cycle V1/V2 exact-parity inputs and deterministic seeds are incomplete

Context:

- The 2026-09-08 current-branch Subbass comparison no longer reproduces its
  historical verified thresholds. At 48 kHz, MIDI 48, 60, and 72 report
  correlations of `0.96886`, `0.95866`, and `0.95695`; the same trend remains
  at 44.1 kHz.
- Fresh Cycle 1 canonical exports do not convert exactly to several newly
  merged graphs. `guitar-3-g` differs in envelope weights/sustain and IR size;
  `accoustic` differs in morph/link state, envelope state, reverb size, and IR
  high-pass; `Icycle` and `organ-2` differ in reverb size. These are preset-input
  failures, not yet DSP verdicts.
- Some legacy documents omit session-owned controls entirely. Subbass does not
  persist its morph-panel state, so Cycle 1 inherits the startup document's
  values while a context-free conversion currently uses defaults. Artifact
  hashes cannot prove equivalent input until the harness pins that state.
- Cycle 1 reverb still seeds its noise from `Time::currentTimeMillis()`, while
  Cycle V2's shared reverb kernel derives a stable seed. Guide-noise and Unison
  jitter seed equivalence have not yet been proven across applications.
- Cycle 1's per-voice rasterizer RNG was also wall-clock seeded. The offline
  parity command now injects a fixed test seed without changing realtime
  behavior; Subbass and `guitar-3-g` are repeatable across fresh processes in
  both engines with that override.
- End-to-end exact output is also masked by Cycle 1 master gain and internal
  44.1 kHz conversion versus Cycle V2's fixed `0.125` output headroom.

Artifacts:

- `/tmp/cycle-subbass-current/comparison.json`
- `/tmp/cycle-subbass-44100/comparison.json`
- `/tmp/cycle-guitar-3-g-raw-parity/comparison.json`

Current status: open. Reconcile each candidate against a fresh canonical
conversion, add explicit deterministic seed control, and remove output-policy
differences before enabling `exactSamplesRequired`.

## Resolved: Cycle 1 and Cycle V2 use different MIDI reference notes

Context:

- The first Subbass differential render appeared to show a dominant Cycle 1
  subharmonic and unstable expected-period cyclogram.
- Cycle 1's legacy `NumberUtils::noteToFrequency()` defines A440 as MIDI 81;
  Cycle V2's shared `UnisonCore` correctly uses standard MIDI 69.
- The preset converter preserved the displayed octave control but initially
  omitted this one-octave boundary translation.

Resolution:

- Strict preset conversion now subtracts one additional octave and records the
  legacy reference offset in the equivalence manifest.
- The corrected four-note comparison reaches at least 0.99990 correlation and
  no more than 0.0143 gain-matched residual. This disproves half-cycle carry as
  the cause of the observed result.

Current status: resolved on 2026-09-06.
