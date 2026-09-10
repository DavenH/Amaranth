# Audio Bug Notes

## Open: spectral reference amplitude assertions no longer match output scaling

The full test suite on `cycle2/fix-audio-parity-2` fails the existing
`Spectral reference content remains harmonic after realtime reconstruction`
and `Exact-period spectral reconstruction repeats one stable cyclogram row`
checks. Their structural and cyclogram comparisons remain accurate, but the
fixed-frame fundamental is `0.00359–0.00542` and the exact-period fundamental
is `0.00717`, below the older absolute `0.1`/`0.22` thresholds. This is
unrelated to document declick: neither test graph contains the new declick
parameter or volume-envelope path. Reconcile the assertions with the current
Output gain/headroom contract without weakening their harmonic-ratio checks.

Current status: open; reproduced independently after the declick-focused tests
passed.

## P1: Spectral Voice Context Japan Drum is not bit-exact across host blocks

Status: open, discovered 2026-09-09 during factory preset no-op cleanup.

Promoting `japan-drum.cyclegraph` from an empty waveform mesh/FFT seed to direct
spectral Voice Context preserves the intended signal topology but exposes a
small host-block dependency in the prepared oscillator path. The existing
partition matrix fails for all four MIDI notes at 4096 samples; the maximum
left-channel differences are approximately `1.1e-8` to `3.0e-8`, while frame
counts and the shorter 1024-sample cases remain exact. A focused rerun produces
the same four failures. This contradicts the earlier byte-identical matrix
claim above and must be resolved at the existing prepared spectral oscillator
timing/rasterization boundary rather than by weakening the exact test.

Artifacts:

- `/private/tmp/cycle-v2-preset-cleanup-full-tests.log`
- `/private/tmp/cycle-v2-preset-cleanup-partition-rerun.log`

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

## Open: Cycle V1/V2 exact parity differs beyond live spectral refresh

Context:

- The 2026-09-08 current-branch Subbass comparison no longer reproduces its
  historical verified thresholds. At 48 kHz, MIDI 48, 60, and 72 report
  correlations of `0.96886`, `0.95866`, and `0.95695`; the same trend remains
  at 44.1 kHz.
- Fresh Cycle 1 canonical exports do not convert exactly to several newly
  merged graphs. `accoustic` differs in morph/link state, envelope state,
  reverb size, and IR high-pass; `Icycle` and `organ-2` differ in reverb size.
  These are preset-input failures, not yet DSP verdicts. `guitar-3-g` and
  `japan-drum` were regenerated from live exports and now match the converter
  exactly while retaining their prior presentation.
- Some legacy documents omit session-owned controls entirely. Subbass does not
  persist its morph-panel state, so Cycle 1 inherits the startup document's
  values while a context-free conversion currently uses defaults. Artifact
  hashes cannot prove equivalent input until the harness pins that state.
- Cycle 1 reverb still seeds its noise from `Time::currentTimeMillis()`, while
  Cycle V2's shared reverb kernel derives a stable seed. Guide-noise and Unison
  jitter seed equivalence have not yet been proven across applications.
- Cycle 1's per-voice rasterizer RNG was also wall-clock seeded. The offline
  parity command now injects a fixed test seed without changing realtime
  behavior. Japan Drum repeated across fresh processes in one corrected-note
  run but not in a later run, so Cycle 1 still has intermittent startup state.
  The corrected-note Guitar 3 G comparison also differs at tiny pre-note
  effect-tail levels in Cycle 1 and is not yet deterministic.
- End-to-end exact output was initially masked by Cycle 1 master gain and
  Cycle V2's separate Output fader and safety headroom. Output gain is now
  translated into the graph, and the parity runner applies the same recorded
  master gain at the Cycle 1 capture boundary. Simple Bass's fitted gain is
  effectively unity; its remaining raw difference is the already-localized
  same-clock numerical residual.
- The paired runner recorded Cycle 1's `-12` legacy MIDI reference but omitted
  it from scheduled note events. Comparisons produced before the 2026-09-08
  runner fix were therefore an octave apart and are not DSP evidence.
- The regenerated static `saw` pair is the first useful minimal baseline. With
  corrected note scheduling it reaches `0.98850` to `0.99844` correlation at
  MIDI 36–72 and closely matches the expected `1/n` harmonic ratios. It is not
  exact: Cycle 1 starts roughly three samples later at the low notes, runs at
  its declared master gain versus Cycle V2 headroom, and has small
  pitch-dependent reconstruction differences. Its first 992 output samples
  also differed across two fresh Cycle 1 processes even though the steady
  render converged exactly; this startup smoothing/state boundary remains open.
- The exact regenerated `power` port is excluded from audio parity because its
  active Cycle 1 time layer has no authored waveform geometry and renders
  silence.

Artifacts:

- `/tmp/cycle-subbass-current/comparison.json`
- `/tmp/cycle-subbass-44100/comparison.json`
- `/tmp/cycle-guitar-3-g-raw-parity/comparison.json`
- `/tmp/cycle-guitar-3-g-reconciled/comparison.json`
- `/tmp/cycle-japan-drum-parity/comparison.json`
- `/tmp/cycle-japan-drum-notes/comparison.json`
- `/tmp/cycle-saw-midi-reference-fixed/comparison.json`
- `/tmp/cycle-saw-reference-fixed-notes/comparison.json`
- `/tmp/cycle-japan-drum-reference-fixed/comparison.json`
- `/tmp/cycle-guitar-3-g-reference-fixed/comparison.json`
- `/tmp/cycle-filter-saw-reference-fixed/comparison.json`
- `/tmp/cycle-filter-saw-reference-fixed-notes/comparison.json`
- `/tmp/cycle-filter-saw-log-region-fix/comparison.json`
- `/tmp/cycle-filter-saw-log-region-fix-notes/comparison.json`

After correcting MIDI scheduling, the admitted pairs still expose a valid DSP
mismatch beyond the minimal time oscillator. Effect-free `japan-drum` reached
only `0.51389` correlation at MIDI 48 in a repeatable run; Cycle V2's cyclogram
was stable while Cycle 1's evolved strongly. A later Cycle 1 run was not
repeatable, so this fixture also exposes intermittent startup state. The
effect-heavy `guitar-3-g` reaches `0.19678` and its Cycle 1 render is likewise
not repeatable. The next diagnostic must capture Cycle 1's time-cycle, FFT,
post-layer spectral, and IFFT boundaries so the first divergent Japan Drum
stage can be compared with Cycle V2 probes.

The smaller `filter-saw` pair locates that boundary more precisely. It contains
one time mesh followed by one subtractive magnitude mesh and no phase or effect
processing. Both engines repeat byte-for-byte. Before correction, correlation
fell from `0.94389` at MIDI 36 to `0.83398` at MIDI 72, while normalized residual
rose from `0.3303` to `0.5518`. Cycle 1 samples the magnitude mesh using
`LogRegions::getRegion(noteState.lastNoteNumber)`; Cycle V2 passes its region
MIDI note through `TrimeshBlockwiseDsp::setFrequencyMidiNote()`. The shared
`LogRegionMapping` already applies a legacy note bias internally. Translating
the standard Cycle V2 oscillator note at that boundary, bounding mesh sampling,
and clearing IFFT bins through `SpectralLayerCore` improves correlation to
`0.95298`, `0.93823`, `0.91239`, and `0.89154` at MIDI 36, 48, 60, and 72. The
normalized residuals improve to `0.3030`, `0.3460`, `0.4093`, and `0.4529`.

The remaining Filter Saw discrepancy was time-dependent: 20 ms windows at MIDI
48 ranged from `0.85560` to `0.99423` correlation. Source inspection showed
that Cycle 1 recalculated its magnitude raster per cycle, whereas Cycle V2
rendered one prepared frame after note reset. That implementation gap was
resolved on 2026-09-08: prepared spectral frames now sample live morph and
scratch controls and rerasterize at synthesis-cycle frontiers.

The deterministic Cycle V2 runtime matrix now covers Saw, Filter Saw, PWM,
Dunk 2, and Japan Drum at MIDI 36, 48, 60, and 72, note lengths of 1024 and
4096 samples, and host blocks of 64, 127, 256, and 512 samples. Audio, scratch
signals, and frame-render counts are byte-identical across block partitions.
An automation fixture also verifies Dunk 2 evolution, PWM duty-cycle and
scratch evolution, repeated fully released notes, and byte-identical WAV files
from fresh Cycle V2 processes.

Fresh post-fix comparisons remain repeatable within both engines but are not
equal at the captured effect-free final voice-output boundary. Filter Saw now
reports correlations of `0.94219`, `0.91977`, `0.88743`, and `0.84886` at MIDI
36, 48, 60, and 72. Japan Drum reports `0.22121`, `0.34862`, `0.24436`, and
`0.23510`. The artifacts are:

- `/private/tmp/cycle-filter-saw-live-modulation-final/comparison.json`
- `/private/tmp/cycle-japan-drum-live-modulation-final/comparison.json`
- `/private/tmp/cycle-v2-time-evolution-final-rerun/summary.json`

The shared spectral-stage recorder now resolves boundaries 1–4 directly. For
Filter Saw at MIDI 48, frame 0 is already non-exact but very close: the time
frame has `0.00022` normalized residual, post-layer magnitude has `0.00223`,
and the reconstructed frame has `0.00803`. The material evolving mismatch
appeared at the magnitude-layer boundary. At selected frame 32, the forward
FFT still had only `0.00077` gain-matched residual and `0.9999997` correlation,
while the post-layer spectrum had `0.51` gain-matched residual and `0.86`
correlation; the IFFT added no meaningful additional error.

The investigation also found two narrower legacy-contract discrepancies:

- Cycle 1 rasterizes nonwrapping magnitude and phase meshes over
  `[-0.05, 1.05]`; Cycle V2 used `[0, 1]`.
- Cycle 1 derives unscripted voice time from the absolute synthesis-cycle
  frontier and applies yellow directly. Cycle V2 restarted a float voice-time
  ramp per host block and smoothed yellow with red and blue.
- Cycle V2's envelope processor advanced every authored envelope with
  `1 / sampleRate`, ignoring the voice-duration-derived normalized time
  increment already carried by `AudioVoiceContext`. Filter Saw therefore played
  its scratch envelope over one second instead of its authored `0.47689545`
  seconds.

The spectral margin has an exact adapter guard. Prepared spectral renderers now
derive Voice Time from the absolute voice sample frontier, apply yellow
directly, and retain byte-identical output across host block partitions. The
graph compiler also supplies default yellow/red/blue modulation to oscillator
region members; it previously inferred only downstream nodes. Depth controls
snap to their routed note-start values as in Cycle 1. The envelope processor
now passes the same normalized voice-time increment to the shared playback
engine, with a focused duration regression. Prepared frame calls carry their
absolute synthesis frontier explicitly, including when Unison renders ahead of
the current host block. Realtime execution skips region-internal fallback
processors and materializes each prepared region once, preserving the existing
zero-allocation contract when live morph inputs are present.

These corrections move Filter Saw at MIDI 48 from `0.91977` to `0.99937`
correlation and reduce gain-matched residual from approximately `0.39` to
`0.0356`. At frame 32 the raw magnitude raster now has `0.99901` correlation
and `0.04448` gain-matched residual; its effective time coordinate is `0.71481`
in Cycle 1 and `0.72422` in Cycle V2. The post-layer spectrum has `0.99755`
correlation and `0.06989` gain-matched residual. The remaining difference is
small and begins before reconstruction, not in IFFT.

The shaped-operand boundary confirms that both engines use the same mature
nonlinear magnitude transfer. At frame 0 its gain-matched residual is only
`0.00068`. At equal frame index 32, the raw coordinate difference is amplified
by shaping to `0.14866` residual before compositing. However, Cycle 1 frame 32
matches Cycle V2 frame 31 almost exactly: scratch is `0.7148094` versus
`0.7148041`, the raw raster residual is `0.0000101`, the shaped operand residual
is `0.0000111`, and the reconstructed-frame residual is `0.0001167`. The final
audio analyzer independently chooses a `-370`-sample lag, approximately one
synthesis cycle. The remaining material discrepancy is therefore a
pitch-clocked frame-latency convention, not a different magnitude-shaping
algorithm.

The pitch-clocked capture then identified the exact convention. Cycle 1 uses
the cycle-start interpolation position when compositing its previous and
current fixed frames; Cycle V2 used the post-advance cycle end. At 44.1 kHz,
Cycle 1 pitch-cycle frame 32 therefore matched pre-fix Cycle V2 frame 30 with
`0.999878` correlation, and their frontiers differed by one 337-sample cycle.
Cycle V2 now evaluates the shared `CyclicFrameLaneRenderer` at cycle start, as
the authoritative Cycle 1 caller does. The same-rate final render now aligns
within one sample at effectively `1.00000` correlation and `0.00049`
gain-matched residual. This resolves the evolving synthesis discrepancy.

At the normal 48 kHz device rate, correlation is `0.99888` with `0.0474`
gain-matched residual because Cycle 1 still synthesizes internally at 44.1 kHz
and crosses its Hermite output-rate converter, while Cycle V2 synthesizes
directly at the device rate. The former `+18.66 dB` difference was the ratio of
Cycle 1's persisted `1.0711173` master gain to Cycle V2's production `0.125`
output headroom. Both offline automation renderers now accept an explicit
output-gain policy, and the parity runner requests unity from both rather than
normalizing after capture. This removes the gain difference while preserving
production defaults. Cycle V2's explicit compatibility policy now reuses the
extracted Cycle 1 block clock and the existing shared Hermite converter while
leaving production rendering at the native device rate.

The first converted render reduced the 48 kHz residual from `0.0474` to
`0.0158`, revealing a fractional delay rather than a converter mismatch. Cycle
V2 reset each oscillator FIFO with an extra zero, while Cycle 1's active
Hermite oscillator path resets the FIFO and immediately writes its first
cycle. Removing that non-authoritative pad makes both 44.1 and 48 kHz Filter
Saw renders align at zero lag with effectively `1.00000` correlation and
`0.00049` residual. The remaining non-exact samples already exist at the raw
time-frame boundary and are now the next localization target.

A separate converter audit also found that legacy modulation input 2 means
`1-Velocity`; future ports now map it to Cycle V2 `inverseVelocity`. The
remaining Voice Context key coordinate is `0.3738318` in Cycle 1 because its
legacy range is MIDI 20–127, versus `0.3779528` in Cycle V2's current 0–127
default. Filter Saw is invariant in red and blue, so those coordinate
differences do not explain this fixture's residual audio.

New artifacts:

- `/tmp/cycle-filter-saw-stages/comparison.json`
- `/tmp/cycle-filter-saw-frame-1/comparison.json`
- `/tmp/cycle-filter-saw-frame-8/comparison.json`
- `/tmp/cycle-filter-saw-frame-32/comparison.json`
- `/tmp/cycle-filter-saw-envelope-duration/comparison.json`
- `/tmp/cycle-filter-saw-absolute-frontier/comparison.json`
- `/tmp/cycle-filter-saw-shaped-operand/comparison.json`
- `/tmp/cycle-filter-saw-shaped-frame0/comparison.json`
- `/tmp/cycle-filter-saw-shaped-frame31/comparison.json`
- `/tmp/cycle-filter-saw-pitch-cycle-44100/comparison.json`
- `/tmp/cycle-filter-saw-pitch-cycle31-44100/comparison.json`
- `/tmp/cycle-filter-saw-pitch-cycle30-44100/comparison.json`
- `/tmp/cycle-filter-saw-cycle-start-44100/comparison.json`
- `/tmp/cycle-filter-saw-unity-gain-44100/comparison.json`
- `/tmp/cycle-filter-saw-unity-gain-48000/comparison.json`
- `/tmp/cycle-filter-saw-cycle-start-48000/comparison.json`
- `/tmp/cycle-filter-saw-legacy-rate-48000/comparison.json`
- `/tmp/cycle-filter-saw-no-extra-pad-44100/comparison.json`
- `/tmp/cycle-filter-saw-no-extra-pad-48000/comparison.json`

Current status: open for the remaining same-clock numerical residual; evolving
synthesis, gain, integer latency, and output-rate policy are resolved. Stage
capture is implemented for the rasterized time frame, FFT, raw magnitude
operand plus effective morph, post-layer spectrum, reconstructed frame, and
pitch-clocked cycle. The first byte difference is in the time frame at very low
residual, so that boundary is the next investigation. Canonical input
reconciliation, deterministic seed control, startup state, and remaining Voice
Context fields must still be separated before enabling `exactSamplesRequired`.

The same-clock residual is now localized below the time frame. A new
`time-raster` capture records both the raw mesh output and its effective morph.
At Filter Saw MIDI 48/frame 32, Cycle 1 and Cycle V2 use identical blue and the
same legacy-range red after separating the audible oscillator note from the
translated control note. Yellow remains `0.7148094` versus `0.7242211`.
Regenerating the time, magnitude, and scratch meshes with shortest-round-trip
vertex values reduces serialization drift without multiline JSON churn, but
does not remove this timing difference. The exact-model 48 kHz render reaches
`0.99999991` correlation with `0.000424` gain-matched residual.

Source comparison identifies the missing contract. Cycle 1 configures scratch
and pitch rasterizers for low-resolution curves and advances the shared
`EnvelopePlaybackEngine` once per synthesis frame in one-sample-per-cycle mode;
it samples the current decoupled value before advancing. Cycle V2 now restores
the purpose-specific low-resolution preparation, but prepared meshes still
sample the ordinary per-sample envelope block at the frame frontier. A trial
one-frame history made the selected raw frame nearly exact but broke the live
frame and host-partition contracts, so it was removed. The open fix is the
cycle-clocked envelope playback boundary specified in parity TDD slice 22, not
a buffer-history approximation.

New artifacts:

- `/tmp/cycle-filter-saw-split-note-48000/comparison.json`
- `/tmp/cycle-filter-saw-exact-models-frame32/comparison.json`

Current status: open at the cycle-clocked scratch-envelope boundary.

Update: the cycle-clocked boundary is implemented. Prepared oscillator regions
now share a dedicated one-sample-per-cycle `EnvelopePlaybackEngine` cursor for
each compiled envelope attachment, including lifecycle and live prepared-morph
adoption. Spectral frame refresh follows Cycle 1's high-quality
`round(16 / cyclePeriod)` stride. At Filter Saw MIDI 48/frame 32, both the raw
time raster and all three morph coordinates are byte-identical between engines.
The first stage difference is now five magnitude bins with an `8.8e-8`
normalized residual.

Correct scratch timing exposed a distinct downstream scheduling issue: Cycle
V2 emitted each cycle from the frame at the same frontier, while Cycle 1 first
prepares the future frame and uses it to synthesize the preceding control
interval. The old one-cycle-early scratch sampling had masked this mismatch.
Cycle V2 now follows that current/future ownership. MIDI 48, 60, and 72 all
align at zero lag with correlations of at least `0.9999999919` and normalized
residuals from `5.6e-5` to `1.27e-4`; the captured MIDI 48 pitch-clocked cycle
has the same frontier and length in both engines. Artifact:
`/tmp/cycle-filter-saw-future-frame-control16/comparison.json`.

Current status: scratch and spectral output scheduling boundaries resolved;
remaining deterministic numeric residual tracked under parity TDD slice 24.

Update: the magnitude-raster residual came from logarithmic harmonic sampling
positions, not output gain. Cycle 1 reads a contiguous, precomputed all-note
`LogRegions` bank. Cycle V2 regenerated a separately aligned position vector;
Accelerate's vector logarithm differed by one ULP at five positions. Cycle V2
now reuses the authoritative default bank. Filter Saw MIDI 48/frame 32 is
byte-identical through magnitude rasterization, range shaping, and post-layer
spectrum. The inverse FFT is now the first unequal stage.

Update: the inverse-FFT residual exposed a bin-index boundary error. Cycle 1's
legacy magnitude and phase arrays omit DC, whereas Cycle V2's full-polar arrays
include DC at index zero. Cycle V2 cleared the spectrum above the legacy active
harmonic count without translating it, erasing the final active harmonic. The
renderer now retains one additional full-polar slot, with a focused regression
test proving that the last legacy harmonic survives and the following harmonic
is cleared. The current Filter Saw MIDI 48/frame 32 reconstructed-frame residual
is `2.6e-7`; the remaining `5.6e-5` output residual is introduced primarily by
pitch-clocked Hermite resampling. Artifacts:
`/tmp/cycle-filter-saw-final-harmonic/comparison.json` and
`/tmp/cycle-filter-saw-final-harmonic-notes/comparison.json`.

Current status: spectral reconstruction boundary resolved; remaining
deterministic output residual is tracked at pitch-clocked cycle resampling.

## Open: Guitar 3 G effects diverge after an equivalent voice output

Context:

- Guitar 3 G now matches Cycle 1 through magnitude and phase operands. With
  waveshaper, IR, EQ, and delay disabled, MIDI 48 is zero-lag at
  `0.9999999997` correlation with a unity gain fit and `2.4e-5` residual.
- Enabling only the authored 2x waveshaper originally lowered correlation to
  `0.98330` and raised residual to `0.1820`. Cycle V2 incorrectly shared one
  stateful oversampler across both channels; matching Cycle 1's per-channel
  ownership raises correlation to `0.999999945` with a `0.000332` residual.
- Adding the impulse response originally lowered correlation to `0.75736` and
  raised residual to `0.6530`. Cycle V2 also shared one stateful convolver across
  both channels; matching Cycle 1's per-channel ownership raises correlation to
  `0.9999724` with a `0.00743` residual and `+0.0226 dB` fit.
- Cycle 1 still fails fresh-process exact repeatability at tiny startup samples
  even when every effect is disabled. Pinning Accelerate to one thread makes the
  effect-free graph repeat exactly, localizing that variation to the inverse
  FFT. The full effect graph still fails at MIDI 36 while MIDI 48, 60, and 72
  repeat exactly.

Artifacts:

- `/tmp/cycle-guitar-no-effects-final/comparison.json`
- `/tmp/cycle-guitar-waveshaper-final/comparison.json`
- `/tmp/cycle-guitar-waveshaper-channel-state/comparison.json`
- `/tmp/cycle-guitar-waveshaper-ir-final/comparison.json`
- `/tmp/cycle-guitar-ir-channel-state/comparison.json`
- `/tmp/cycle-guitar-full-matrix/comparison.json`
- `/tmp/cycle-guitar-midi36-repeat-recheck/comparison.json`

Current status: material waveshaper and IR gaps addressed. Their smaller
numerical residuals remain open. EQ and delay add no material discrepancy, but
Cycle 1's low-note full-effect repeatability still blocks fixture admission.
