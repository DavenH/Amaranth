# Audio Bug Notes

## Open: Full Cycle V2 suite intermittently cannot create IR fixture waves

Context:

- The complete `CycleV2_tests` run on 2026-09-05 failed both cases using
  `writeImpulseWave()` at `TestImpulseResponseResourcePreparation.cpp:47`
  because `File::createOutputStream()` returned null.
- The cable Pan presentation work does not alter IR resource preparation or
  temporary-file handling; all focused Pan tests pass.

Current status: open; reproduce under the full-suite temporary-file lifecycle
and determine whether fixture filenames or cleanup race with another case.

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
