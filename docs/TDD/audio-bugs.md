# Audio Bug Notes

## Resolved: Organ 2 oscillator parity tests referenced an archived path

During source-rendering optimization on 2026-09-15, two voice-time parity
tests failed at `TestChainedOscillatorRegionRuntime.cpp:91` with
`loaded.succeeded() == false`. `3db65105` already stores this fixture only at
`content/presets/old/organ-2.cyclegraph`. Point these two historical parity
tests at that exact archive; their DSP expectations remain unchanged.

## Resolved: AcidStab3 crashed in uninitialized sinc resampling

Loading the legacy `AcidStab3.cyc` preset and auditioning low notes produced
four repeatable macOS crashes on the realtime audio thread. The preset selects
sinc for realtime resampling. Each crash entered `Buffer<float>::copyTo()` from
`Resampler::resample()` and `CycleBasedVoice::renderInterpolatedCycles()` while
copying a 512- or 1024-sample oscillator cycle.

The immediate cause was a stale `USE_IPP` gate in `CycleBasedVoice`: its sinc
path ran on Apple Silicon, but allocation, initialization, reset, and priming of
the newer JUCE/Accelerate resampler were still omitted there. Uninitialized
source/destination sizes then corrupted the resampler memory layout. Sinc voice
setup and tail finalization now use the existing platform-independent
`Resampler` implementation on every platform.

`Resampler::resample()` also already had an exhausted-source-window rejection
path, but it tested `lastread` only after copying into `source + lastread`. The
capacity test now runs before pointer arithmetic or copying. A focused resampler
regression creates that retained-window condition and proves the second input is
rejected without touching invalid memory.

The repaired path also exposed an independent bug in the oscillator
oversampler's cyclical-tail handling. The IPP implementation used to receive the
number of produced downsampled samples from `ippsSampleDown`, but that count was
left uninitialized when the shared `Buffer::downsampleFrom` call replaced it.
`downsampleFrom` now reports its exact output count on both IPP and Accelerate,
and the oversampler adds only that portion of its tail. A focused test verifies
that the wrapped tail affects exactly the produced prefix.

The AcidStab3 automation fixture opens the preset, immediately auditions low
note 41, requests a four-second realtime capture, and requires non-silent
output.

Artifacts: `/private/tmp/cycle-agent-acidstab3-fixed-report.json` and
`/private/tmp/cycle-agent-acidstab3-offline-final-report.json`. Status:
resolved 2026-09-13.

## Resolved: Subbass realtime fixture asserted the pre-reconciliation octave

The full `standalone-debug` CTest run fails
`Strictly ported subbass fixture renders through the realtime path` because the
test still expects octave `-2`, while the canonical preset reconciliation in
`b2129dc0` deliberately changed `subbass-parity.cyclegraph` to octave `-1` and
moved the remaining legacy reference translation into the comparison manifest.
The failure repeats in isolation and is unrelated to Astral graph publication.
Reconcile the stale structural assertion with the fixture's current audible
parity contract. The assertion now guards octave `-1`; the manifest continues
to own the separate `-12` legacy MIDI-reference translation. Status: resolved
2026-09-13.

## Resolved: one-sided Add bypassed prepared pitch reconstruction

Astral was reported to sound heavily distorted and remain near F2 for every
keyboard note in the standalone Cycle V2 app. Dirty Guitar 2 exhibited the
same behavior. F2 is suspiciously close to the 44.1 kHz / 512-sample callback
cadence (86.13 Hz).

The investigation did find that successful graph loads updated the canvas
synchronously while `NodeWorkspace` deferred publication of the new audio plan
to a 30 Hz timer. Earlier one-shot UI captures therefore rendered the startup
plan while reporting Astral's canvas state.

Graph loads now prepare and publish their audio plan immediately. The remaining
reproduction depended on the exact preset payload: the Astral graph in the
Amaranth2 sister worktree and the canonical Dirty Guitar 2 graph both contain
an Add node with only its right input connected. The ordinary
`BinarySignalProcessor` correctly treats the absent input as zero, but the
prepared oscillator recipes rejected the graph. Preparation failure was not
surfaced, so realtime execution fell back to the ordinary IFFT processor and
emitted raw host blocks. The resulting spectrum was a comb spaced at 86.13 Hz.

Both spectral-frame and chained-cycle recipes now preserve a lone Add operand,
matching the authoritative blockwise Add semantics. The focused regression
inserts the same right-only Add into otherwise supported spectral and chained
graphs and proves that the output is unchanged. Factory regressions cover the
existing Dirty Guitar 2 and Time graphs. In a live 44.1 kHz/512-sample capture
of the exact sister-worktree Astral file, MIDI 72 moved from 10.5 dB below the
callback-frequency component to 31.7 dB above it.

The renderer still needs an explicit contract for any future oscillator region
that compiles but cannot be prepared. Today that failure is silent and permits
the ordinary blockwise processors to run, which can turn another unsupported
recipe into callback-period audio. Publication should reject the plan or
replace that region with a surfaced error/silence path rather than silently
changing its execution model. Status: open architecture follow-up 2026-09-13.

Artifacts: `/private/tmp/cycle-v2-amaranth2-astral-note72.wav` and
`/private/tmp/cycle-v2-amaranth2-astral-fixed-note72.wav`. Status: resolved
2026-09-13.

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

## Resolved for parity: Cycle V2 Voice Context pitch fields

Context:

- The Subbass differential render showed that the compiled Voice Context
  octave never reached `PreparedOscillatorRegion`; Cycle V2 rendered the graph
  one octave above Cycle 1.
- The octave now becomes an integer MIDI-note offset at the prepared region
  boundary and has a focused equivalence test.
- Cycle 1 has no base-pitch or portamento controls. The converter's zero/off
  values are neutral V2 state, not omitted Cycle 1 behavior.
- Realtime oscillator oversampling remains a future V2 feature. Stengah is the
  only factory graph with a non-default imported value, and its pure spectral
  voice never enters Cycle 1's time-cycle oversampling/downsampling branch.
- A separate converter error folded the already translated legacy reference
  note into stored octave. Correcting Cycle 1's actual control rounding updates
  twelve factory graphs and moves Accoustic dry MIDI 48 from `0.19646` to
  `0.99733` correlation; Subbass reaches `0.99997`.

Current status: Cycle 1 parity is resolved. Base pitch, portamento, and audible
oscillator oversampling require a separate feature TDD rather than parity code.

## Open: Cycle V1/V2 exact parity differs beyond live spectral refresh

Context:

- The 2026-09-08 current-branch Subbass comparison no longer reproduces its
  historical verified thresholds. At 48 kHz, MIDI 48, 60, and 72 report
  correlations of `0.96886`, `0.95866`, and `0.95695`; the same trend remains
  at 44.1 kHz.
- Fresh Cycle 1 canonical exports did not convert exactly to several newly
  merged graphs. Accoustic, Icycle, and Organ 2 have now been reconciled while
  preserving their Cycle V2 presentation and explicit global topology.
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
- The paired runner now also disables macOS's Nano allocator for both child
  renderers, in addition to pinning vecLib to one thread. This makes Sitar
  byte-repeatable across its full MIDI matrix. Japan Drum MIDI 48 repeated five
  times under that contract, but one of two Cycle 1 renders differed in a later
  full matrix, so its separate intermittent state remains open. Artifacts:
  `/tmp/cycle-japan-drum-repeat-allocator/comparison.json` and
  `/tmp/cycle-japan-drum-verified-allocator/comparison.json`.
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
`1-Velocity`; future ports now map it to Cycle V2 `inverseVelocity`. Cycle V2
now shares Cycle 1's MIDI 20–127 normalization range. Voice Context octave is
also applied to inherited key-scale modulation as of parity slice 56. Filter
Saw is invariant in red and blue, so neither correction explains this
fixture's remaining residual audio.

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

Recheck 2026-09-15: the grouped `[oscillator-region]` run again fails
`Prepared spectral reconstruction retains the final active harmonic` because
the captured final magnitude is zero. The same run also contains missing
preset-fixture failures and the known split-block tolerance failure. The Spy
traversal fix does not alter `SpectralOscillatorFrameRenderer`, and its focused
PWM Lead regression passes; treat this as an open baseline audit rather than
changing the established harmonic contract. Command:
`build/tests/cycle-v2/CycleV2_tests "[oscillator-region]"`.

Resolved 2026-09-15: `Bundled FFT diagnostic graph preserves its sawtooth probe
through IFFT` reconstructed sample zero as `3.17889` from `-0.999597` (maximum
error `4.17849`) because `GraphAudioExecutor` treated every processor inside an
oscillator region as its materialization step. Restricting materialization to
the compiled region's `materializationStepIndex` preserves both FFT outputs and
the IFFT round trip. A follow-up also restored the oscillator boundary's
zero-DC contract before Unison composition; the diagnostic traversal now
matches the DC removal already used by Cycle 1 visual transforms and Cycle 2's
realtime spectral renderer. It also converts Cycle 2's full-bipolar runtime
grid to Cycle 1's half-bipolar visual scale before lane composition. The
focused bundled and direct FFT/IFFT regressions pass with the expected centred,
half-scale reconstruction.

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
  FFT. Disabling macOS's Nano allocator also makes five full-effect MIDI 36
  renders exact, but a later full matrix failed at MIDI 48. The difference is
  already present in the dry output before a Delay tap can return. Disabling
  Delay produced three exact repeats, but Delay is not yet identified as the
  source because its presence changes allocation layout before rendering.

Artifacts:

- `/tmp/cycle-guitar-no-effects-final/comparison.json`
- `/tmp/cycle-guitar-waveshaper-final/comparison.json`
- `/tmp/cycle-guitar-waveshaper-channel-state/comparison.json`
- `/tmp/cycle-guitar-waveshaper-ir-final/comparison.json`
- `/tmp/cycle-guitar-ir-channel-state/comparison.json`
- `/tmp/cycle-guitar-full-matrix/comparison.json`
- `/tmp/cycle-guitar-midi36-repeat-recheck/comparison.json`
- `/tmp/cycle-guitar-repeat-allocator/comparison.json`
- `/tmp/cycle-guitar-verified-allocator/comparison.json`
- `/tmp/cycle-guitar-repeat-no-delay/comparison.json`

Current status: material waveshaper and IR gaps addressed. Their smaller
numerical residuals remain open. EQ and delay add no material discrepancy, but
Cycle 1's intermittent full-effect repeatability still blocks fixture admission.

## Resolved: Icycle pitch-clocked Unison reconstruction and repeatability

Context:

- Icycle was regenerated from a direct Cycle 1 canonical export while retaining
  its hand-authored Cycle V2 node presentation and three signal probes.
- With waveshaper, IR, and delay disabled, MIDI 48 reaches `0.99548`
  correlation and `0.0949` gain-matched residual at a five-sample diagnostic
  lag.
- The stage capture is byte-identical through time raster/frame and every
  magnitude/phase operand. Forward FFT and reconstructed-frame differences are
  only `8.8e-8` and `5.2e-6`; pitch-clocked cycle reconstruction increases the
  residual to about `0.056`.
- Unison group layout and jitter come from shared `UnisonCore`. All assigned
  Guides have zero noise, offset, and phase, so lifecycle seed mapping does not
  explain this preset's difference.
- Cycle 1 changes from frame 40 across fresh processes even with Unison and all
  effects disabled. Cycle V2 repeats exactly. This independently blocks fixture
  admission until the Cycle 1 startup instability is localized.

Artifacts:

- `/tmp/cycle-icycle-unison-baseline/comparison.json`
- `/tmp/cycle-icycle-no-unison/comparison.json`
- `/tmp/cycle-icycle-unison-stages/comparison.json`

Update: Cycle V2 had treated the 129-point Unison preview trajectory as an
audio-time pitch buffer. Prepared oscillator regions now resolve the pitch
Envelope into the existing cycle-envelope bank and advance one mature playback
cursor per lane. At MIDI 48, every captured stage through reconstructed frames
is byte-identical; pitch-clocked residual falls to `2.6e-5–4.6e-5`, final
alignment becomes zero-lag, and the full effect graph reaches `0.99825`
correlation with `0.0592` residual.

Update: Cycle 1 latches its current reconstructed frame into past-frame storage
after each successful oscillator render call. Cycle V2 retained the older past
frame across calls. Restoring the mature boundary raises the effect-free MIDI
36–72 matrix to `0.98602–1.00000` correlation and lowers residuals to
`0.0011–0.1667`. The full graph reaches `0.98425–0.99997` correlation with
`0.0076–0.1768` residual. The stage capture now includes the complete composed
cycle, confirming that the earlier frame-8 divergence entered before Hermite
resampling. The neutral shared-frame clock also retains Cycle 1's float
frequency precision and fractional lane-cycle starts.

Final update: the remaining Cycle 1 variation came from the live device
advancing effect and master parameter smoothing by a timing-dependent amount
before offline capture. The capture boundary now settles the existing
waveshaper, IR, EQ, and master parameters to their authored targets after the
device is suspended and prepared. Realtime smoothing is unchanged. Three fresh
waveshaper-only renders now match byte-for-byte, as does the complete two-render
MIDI 36–72 matrix in both engines. Icycle is admitted as a verified fixture;
artifact: `/tmp/cycle-icycle-full-settled-matrix/comparison.json`.

## Resolved: preserve Cycle 1's effective Envelope cross-section

Context:

- `dynamic=false` applies to live rerasterization, not to the red/blue grammar.
  Removing the explicit zero source was therefore structurally plausible, but
  it did not preserve the mature renderer's audible behavior.
- Cycle 1's `EnvRasterizer::updateValue()` updates smoothed targets. Its
  `SynthesizerVoice::updateSmoothedParameters()` is empty, so the current
  red/blue values consumed by rasterization remain at their initialized zero.
- Cycle V2's general Envelope inputs retain their intended absolute Voice
  Context modulation. Imported factory graphs represent the Cycle 1 defect
  explicitly with one `legacyEnvelopeMorph` constant-zero source.

One-note diagnostic reruns after the correction produced:

- Icycle MIDI 48: zero lag, `0.97464` correlation, `0.2238` gain-matched
  residual, `1.64 dB` spectral error, and `0.1009` cyclogram difference.
- Guitar 3 G MIDI 48: `-451` sample candidate lag, `0.30399` correlation,
  `0.9527` residual, `10.51 dB` spectral error, and `0.9325` cyclogram
  difference.

Those failing artifacts are `/private/tmp/cycle-icycle-envelope-ownership-rerun-fresh/comparison.json`
and `/private/tmp/cycle-guitar-envelope-ownership-rerun-fresh/comparison.json`.

After migrating all 191 affected graphs, Guitar 3 G MIDI 48 is zero-lag at
`1.00000` correlation, `0.0008` residual, `0.03 dB` spectral error, and
`0.0006` cyclogram difference. Icycle is zero-lag at `0.99996` correlation,
`0.0087` residual, `0.15 dB` spectral error, and `0.0117` cyclogram difference.
Cycle V2 repeats exactly; Cycle 1 retains its separately documented
process-level nondeterminism. Current artifacts are
`/private/tmp/cycle-guitar-legacy-envelope-rerun/comparison.json` and
`/private/tmp/cycle-icycle-legacy-envelope-rerun/comparison.json`.

Current status: graph/runtime semantics corrected. Icycle is close but below
its declared correlation/residual thresholds; Guitar 3 G has a material open
parity gap. Neither prior parity claim is readmitted.

Product update (2026-09-13): the factory `legacyEnvelopeMorph` compatibility
nodes described above are intentionally removed. Normal implicit Voice Context
modulation now takes precedence over exact preservation of Cycle 1's stuck
Envelope cross-section.

## P2: Grouped Waveshaper tests can lose the traversal grid

Context:

- A grouped `[waveshaper]` Cycle V2 run on 2026-09-12 failed
  `Stengah Waveshaper post gain changes stereo traversal and downstream audio`
  because `lowShape.traversalGrid.isValid()` was false (Catch seed
  `1509597871`).
- The Waveshaper editor layout regression passes in isolation and does not
  touch graph-audio traversal state.

Current status: open; reproduce the grouped ordering independently of the UI
layout work and isolate the missing preview traversal grid.

## Resolved: preset-port manifest source location

Context:

- The Cycle 1 `saw.cyc` and `organ-2.cyc` fixtures moved under the preset
  library's `old/` directory.
- Manifest tests now resolve those tracked fixtures at their current location.

Current status: resolved; the full converter test module passes.

## P2: Astral realtime fixture still references the pre-migration preset path

Context:

- A fresh `standalone-debug` build on 2026-09-15 passed the focused prepared
  realtime suite (9 cases, 118 assertions), but the broader
  `[cycle-v2][audio-device][realtime]` run failed one of 16 cases at
  `TestRealtimeGraphRenderer.cpp:66` because `loaded.succeeded()` was false.
- `renderAstralRealtimeNote()` loads
  `cycle-v2/content/presets/astral.cyclegraph`, while the tracked legacy file is
  now `cycle-v2/content/presets/old/astral.cyclegraph` and the current variants
  have distinct names.
- The failure occurs during fixture loading before realtime rendering.

Current status: open; choose the intended Astral parity fixture and update the
test path without weakening its audio assertions.

## P2: Offline spectral parity fixtures have stale expectations and paths

Context:

- On 2026-09-15, `Offline spectral capture records equivalent harmonic
  boundaries` produced a time-raster secondary value of `0.225972116` instead
  of the asserted `0.7148094`. The failure reproduces with block-time spectral
  transfer resolution restored, so it is independent of prepared transfer
  binding. The older value is also discussed in the open same-clock spectral
  residual investigation above.
- `Offline guide seed controls spectral oscillator noise deterministically`
  fails while loading `content/presets/sitar.cyclegraph`; that file now lives
  at `content/presets/old/sitar.cyclegraph`, alongside distinct current Sitar
  variants.

Current status: open; reconcile the time-raster assertion with the current
frontier contract and choose the intended Sitar fixture before updating its
path.

Update 2026-09-15: a broad randomized runtime batch (seed `1479899913`) also
failed `Spectral frame refresh count is independent of Unison order` for the
64-sample partition: maximum difference `0.023058094` exceeded the existing
`0.02` tolerance. Focused realtime executor and prepared-context tests pass;
retain this as an open oscillator-region partition/parity issue.
