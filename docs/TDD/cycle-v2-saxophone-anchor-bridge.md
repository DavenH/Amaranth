# Saxophone reference anchors and first bridge

Status: Compact reed-source/body-filter prototype implemented; acoustic listening
acceptance and the full expressive instrument remain open. The user approved the earlier two-reference fitting
experiment, then directed a simpler synthesis model that can generalize across
notes. Dense harmonic fits remain discovery evidence and comparison captures.

## Listening revision: a compact reed pulse and shared body response

The first pair follows instantaneous timbre and amplitude reasonably closely,
but sounds sterile and misses breath/grain, a faint lower-octave impression and
the louder attack's brief instability. The user's next direction is a small,
editable time-domain reed source feeding a body response. More independently
fitted harmonic poles are outside this revision.

Author a few visible normalized pulse knots in one Trimesh, with two full-scale
copies over its cycle. Voice Context runs one octave below the played note:
identical copies restore the nominal fundamental. During onset, make the
copies deliberately asymmetric and fade them to exact symmetry for the settled
body. This gives the transient's odd half-harmonics a low-dimensional cause;
it does not assert that the measured pitch has a large octave glide. Keep the
source's amplitude normalization and the pulse's useful shape controls explicit.

Put the existing factory component-guide grains directly on the reed mesh,
with phase/timing chosen around the pulse. Use existing interpolation and guide
machinery. Grain is part of the excitation, rather than a separate added noise
branch or a reconstructed recording residual. CC2 should move an audible reed
shape/pressure dimension; optional CC3 drives a meaningful instability dimension
through a local source connection. Controller names alone do not establish
useful expression.

Feed that source into the existing per-voice Impulse Response node, using a
fixed-Hz body kernel and canonical imported-audio resources. The body stage is
strictly multiplicative filtering: it can emphasize or attenuate source energy,
but cannot supply harmonics absent from the reed. Avoid an additive harmonic
bed disguised as a body response. Keep the same compact source and body model
across the first register trials; do not create a separate response per note.

Test multiple low/middle/high notes early, alongside both reference layers,
intermediate CC2 values and held controller sweeps. Expose failures of onset,
noise, pitch and register behavior while the model is still small. Retain raw
source comparisons, frozen harmonic-only captures and exact render identities;
spectral improvement supports the work, while resemblance and taste require
listening. Shared endpoint, reload and seeded-repeatability checks still apply.

The graph should read as reed -> body IR -> breath gain -> articulation -> Voice
Out, with gain controls below their multipliers, local instability beside the reed, shared
Voice Context/controls above-left and separate global I/O. Reserve every actual
card and output spy footprint. Verify the native scene for visible cards and
short unobstructed joins. The twelve-node model includes separate CC2 gain:
narrowing a normalized pulse reduces its energy, so pressure needs an explicit
level mapping rather than sacrificing visible source geometry.

The original sections below document the completed dense harmonic experiment
and its frozen evidence. Its spectral authoring and stereo-routing details are
historical, rather than requirements for the current reed/body implementation.

## Narrow excitation and rail-Guide revision

The user accepts the compact core and specifically likes its sub-octave
behavior, but rejects the initial tone as a saxophone match. The next direction
is a near-impulse/narrow-PWM source with broad harmonic support and a visible
multiplicative spectral canvas. Preserve the useful instability while refining
the reference tone; successful symmetry tests are not acoustic acceptance.

The source now uses two .005-cycle narrow kicks, with crests aligned to 0/.5.
The initial wider kick suppressed harmonic 40 by roughly 21–39 dB in the
analytic probe. The actual revised clean D3 source retains H40 within 0.82 dB
of H1. Alignment also avoids the peak misses exposed by pitch-derived point
sampling (queue M13). Source amplitude stays visibly normalized and gain is
separate. No new synthesis or resampling algorithm is introduced.

Three sparse onset Guides replace the redundant yellow-time cube subdivision.
They control second-pulse lag, crest depth and grain sharpness; neutral guards
settle them by 120 ms. Delay translates the whole pulse, preserving its narrow
width. The source drops from 20 cubes/120 vertices to 10 cubes/80 vertices.
An added spectral mesh uses only two cubes and one simple Guide: quiet upper
attenuation fades to exact neutral at full CC2. Its six curve points replace
independent harmonic poles. The pipeline is pulse -> FFT -> magnitude Multiply
-> IFFT -> common body IR -> breath -> articulation. FFT phase passes through;
no additive magnitude or retired stereo nodes are used.

The bipolar product is authored using the inspected exponent D and gain g:
raw amp = 0.5 * desiredGain^(1/D) / g. The additive unit coefficient is not
used. Spectral frequency labels are calibrated at the inspected effective
oscillator note; the existing body IR supplies the genuinely fixed-Hz stage.

The current body emphasizes D3's lower resonance at 600 Hz/BW250, with a
2200 Hz/BW1000 upper mode at relative peak 0.25. D3 loud H2/H1 improves to
1.72 versus reference 2.15, and its centroid is 821 versus 875 Hz. The lower
and upper register still fail parts of the reference balance; quiet D3's H2
is excessive. Mean harmonic RMSE across the six probes is 5.61 dB versus the
initial core's 4.90, while broad-band RMSE improves from 3.65 to 3.16 dB.
This is a D3-focused refinement, rather than an overall harmonic improvement.
Alternative flat and broader modal bodies are retained for listening; no full-range or
taste acceptance is claimed. Queue M14 records the shared reed-edge/register
relationship needed next.

Artifacts are under `.context/sax-reed-refinement/aligned-focused-body/`.
With grain disabled, settled odd/even power is −99.56 dB before the body and
−100.08 dB after it. The filtered onset is −15.01 dB and retained CC3 asymmetry
is −9.10 dB. Seeded repetition, immediate/reopened Guide edits, grain ablation
and zero-breath silence pass. The preset has 16 nodes, 17 cubes and 209 total
mesh/curve vertices, including its five Guides and 53-point modelled body.

Native verification finds all 16 cards and the output spy visible with zero
overlap, clean compilation and working phrase PLAY/stop. The phase bypass
runs below its magnitude multiplier; color and pressure enter locally from
above/below. Graph/editor screenshots and `native-review-proof.json` are in
the artifact folder. The browser audit plays all 32 original references, six
current source/synth pairs and ten raw clips including the phrase and three
D3 body alternatives. The app is left on the installed preset, stopped, with
CC2 at 96 and no unsaved document edits. Native device playback is 44.1 kHz;
the analysis captures are 48 kHz. Queue M11's existing IR sample-rate policy
limits direct equivalence between their body color.

A separate like-for-like Debug benchmark isolates the rail change before the
new FFT/body work: three interleaved 1/8-voice runs, same app/presets/device,
1.2-second measurement windows. Mean rasterization cost per operation is
148.66 -> 135.30 microseconds at one voice and 72.11 -> 69.56 at eight;
waveform segments per rasterization stay unchanged. Mean callback times are
0.406 -> 0.393 ms and 2.151 -> 2.198 ms respectively, with coarse p95 buckets
unchanged. This supports reduced interpolation work, not a large or reliable
whole-callback speedup claim. Raw distributions, operation counts and workload
are in `performance/baseline-vs-rails.json` under the refinement folder.

## Initial compact core implementation and evidence

`cycle-v2/content/presets/reed-saxophone.cyclegraph` contains one normalized
five-point kick repeated twice, one common body IR, separate breath gain and
volume articulation. There is no spectral mesh/IFFT or retired stereo node.
CC2 narrows the closure and raises breath gain; CC1 changes attack speed; CC3
retains alternating closure. The embedded thirteen-second phrase covers low,
middle and high notes, pressure sweeps and the instability control. These
controls demonstrate the core, not completed connected articulation or vibrato.

The shared response has two damped modes, centered at 500/2200 Hz with
bandwidths 450/1000 Hz and relative modal peaks 1/0.4. The offline recipe
authors response data; existing IR preparation owns trimming and editable-curve
modelling, and existing convolution owns sound generation. Semantic automation
now imports that resource in one undoable edit. The actual imported result is
215 retained samples, a 256-sample kernel and 37 curve vertices. Keep the
48 kHz authoring/playback limitation explicit (queue M11).

The complete preset has twelve nodes, ten edges, 160 mesh vertices/25 cubes
and 39 curve vertices including the Guide. The preceding dense bridge had
4,624 vertices/928 cubes. All three tested notes (standard MIDI 48/62/72) use
the identical reed geometry and IR at CC2 32/127; there are no per-note fits.

Final artifacts are under `.context/sax-reed-core/ready/`. Verification checks
exact seeded repeats, grain ablation, repeated Guide edits after reopening,
zero breath silence and deliberate CC3 asymmetry. With Guide noise zero, the
settled odd/even half-frequency-grid power ratio is −98.08 dB before the IR and
−101.53 dB after it. The filtered onset is −14.75 dB and persistent CC3 closure
asymmetry gives −14.67 dB. Convolution therefore preserves the settled missing
bins; transient odd energy comes from the reed. Grain changes actual PCM.

The six consumed developmental references yield approximately 4.90 dB
RMS-matched harmonic RMSE and 3.65 dB broad-band RMSE in the common body
interval. Pressure increases both RMS and centroid at every tested pitch.
The upper loud register remains too dark (982 Hz centroid versus 1,534 Hz in
its source); quiet low-register color also needs refinement. Measurements are
in `.context/sax-reed-core-analysis/ready-runtime-assessment.json`. They are
not independent validation or perceptual acceptance.

The listening page retains all 32 full originals and adds six paired first
1.5-second excerpts. Each synth receives one declared scalar RMS match over
0.4–1.4 seconds; raw synth excerpts and seven full raw auditions, including the
phrase, retain actual levels. There is no time or frequency alignment. The
original frozen harmonic captures and unsuccessful dense texture experiments
remain under `.context/`; unused dense texture authoring code was removed.
The original anchor fitter retains only a next-harmonic zero stop-pole fix,
preventing its final fitted partial from extending across unused high bins.

Reproduce after building the exact app:

```sh
.context/pr7-audition-reports/.context/venv/bin/python scripts/create_cycle_v2_reed_saxophone.py \
    --no-build --output-dir .context/sax-reed-core/new-run
.context/pr7-audition-reports/.context/venv/bin/python scripts/cycle_saxophone_reed_audit.py \
    --results .context/sax-reed-core/new-run/results.json \
    --profiles .context/sax-reed-core-analysis/reed-core-analysis.json \
    --output-dir .context/sax-reed-core/new-run
python3 scripts/create_saxophone_reference_audit.py --include-reserved \
    --comparisons .context/sax-reed-core/new-run/comparisons.json
```

The six reference paths and identities are required local developmental input;
the installed preset embeds its body resource and needs no source recordings.

Final native checks opened the installed preset in owned PID 1653: all twelve
nodes and the output spy are visible without overlap; main audio joins are
horizontal and pressure enters vertically. The editor shows the two visible
reed pulses. Native PLAY/stop exercises the embedded phrase with nonzero output
and no dropped MIDI, then returns to zero active voices. The app remains open
with CC2 preview at 96 and playback stopped. Screenshots and state proof are
`ready/finalgraph.png`, `ready/reed-editor.png` and `ready/native-review-proof.json`.
Browser checks pass all 32 references, six pairs, seven raw auditions, seeking,
notes/export and compact responsive layout. Focused IR import, canonical
resource-publication and analysis tests pass; build, architecture, code-map
and diff checks pass. Listening acceptance remains open.

## Musical target

Fit the VCSL tenor Non-Vibrato D3 layer-2 and layer-3 recordings independently,
then reconcile their differences into one continuous, playable control. These
are source layer labels, not calibrated dynamics or MIDI velocities. Both are
developmental references now; the old acquisition partition remains historical.
Keep the existing expressive saxophone, user edits and frozen studies intact.

Deliver two saved anchor presets, a shared bridge preset, and source/synthesis
comparisons on the existing compact listening page. Fit attack, sustained
harmonic evolution, pitch and ending, with explicit limits for remaining noise
or recording-room differences. Use measured frequency frames from canonical
reference analysis; never infer sounding pitch from filename octaves.

First fit and bridge only this pair. Test nearby pitches and intermediate
settings before expanding the anchor bank. Quantitative improvement is evidence
of fitting, while convincing resemblance and musical taste still need the
user's audition. Keep unsuccessful experiments and mismatches visible.

## Owners and implementation

- Reference decode and pitch: existing ReferenceLibrary/PitchedSample exports
  and cycle_reference_study. Original audio, labels and hashes stay immutable.
- Offline measurement: reuse existing waveform/spectrum helpers; vectorized
  harmonic-envelope measurements may reduce canonical frequency observations
  into targets. Do not add a second pitch tracker or realtime synthesis path.
- Sound generation: existing graph nodes, shared mesh/envelope interpolation,
  magnitude/phase synthesis and noise. Author model/control points through
  applyNodeEdits and graph commands; no raw graph mutation in production UI.
- Render and provenance: canonical exact-instance coordinator, captureAudio
  and performance-suite/artifact helpers. Use a separate owned app so the
  user's currently open/edited app remains untouched.
- Listening: extend create_saxophone_reference_audit with an optional comparison
  manifest. Keep compact rows, one player, original references, browser notes
  and explicit source/render identities. Default reference-only builds remain
  supported; local media stays outside Git.

The anchor pair should use corresponding components and shared phase/timing
conventions before interpolation. Prefer a compact harmonic/noise explanation
with temporal envelopes to storing unrelated waveforms at every control value.
Record any required engine or authoring feature in the existing modelling queue
with reproduction evidence. Existing M02 region-analysis work is avoided for
this first isolated-note pair, not declared solved.

## Stereo routing constraint

Use cable panning for channel management. The user explicitly excludes retired
Stereo Split/Join nodes. Each independently fitted IFFT feeds an ordinary
time-domain Add through a hard-left/right cable pan. The durable representation
is the existing internal `spectralLayer` transport and its incoming/outgoing
edges; scene presentation renders these as one cable with an inline headset.
Its absence from generic `addNode` is intentional, rather than a missing
standalone-node feature. Installed pan values use existing parameter commands.

For the current complete-content fixture, declare that canonical transport
while assembling topology, then load/validate through the existing graph
boundary and author fitted models semantically. A future semantic cable-pan
command should delegate to `NodeCanvasAuthoring::insertPanIntoEdge` and
`removePanFromCable`; no new runtime node or retired channel-node revival is
needed. Earlier Split/Join experiments are superseded by regenerated cable-pan
captures and do not serve as final routing evidence.

## Canonical spectral authoring discovery

`inspectNodeControls` accepts an optional standard `midiNote` and `cycleFrames`
(default 2048) for a spectral Trimesh. Its read-only `spectralAuthoring` result
uses the existing `LogRegionMapping`, FFT bin-count boundary, `SpectralLayerCore`
and key normalization rather than copying their algorithms into the host fitter.
Ordinary node inspection and audio execution retain their existing behavior.

The result exposes harmonic positions and count, region size, mapping sample
rate, sounding frequency, key position, unipolar additive unit scale/exponent
and bipolar phase scales. Coefficients include the inspected node's gain/range.
For unipolar additive magnitudes, author `amp=(target/unitScale)^(1/exponent)`;
for absolute phase in radians, author `amp=0.5+target/(2*phaseScale)`. The former
applies before other graph operands and the latter includes harmonic weighting.
These recipes assume a unipolar magnitude mesh feeding an additive/IFFT input
and an absolute-mode phase mesh; bipolar additive operands, multiplicative
operands and phase-velocity integration require their own interpretation. The
exported coefficients do not assert that every inspected graph uses this recipe.
Ordinary FFT-bin magnitude is half the cosine peak amplitude under the shared
`DivFwdByN` transform convention; DC is removed and the Nyquist endpoint has a
different real-bin convention. Current anchors use ordinary harmonics below it.
These assumptions are machine-inspectable as `requiredPolarity`,
`requiredPhaseMode`, `fftNormalization`, `ordinaryHarmonicPeakToMagnitude`,
`dcMagnitude` and `nyquistPeakToMagnitude` in the same result.

The mapping is fixed at 44.1 kHz even when the audio device uses another rate.
At MIDI 62, the full region has 76 harmonics; a 64-frame FFT caps output at 32
without changing those harmonics' display positions. Callers supply the actual
render frame size; this metadata does not infer it from a Trimesh alone.
Maximum sharpness is the existing finite curve-table limit, so rendered knot
accuracy remains part of fitting evidence. Nearby-pitch mapping can be authored
through shared red/key poles; resemblance at those notes still needs audition.

Focused tests compare exported coordinates and operand coefficients with the
canonical implementations, invert several target magnitudes through the actual
transfer, exercise FFT capping, and reject invalid notes/frame sizes. This
closes the numerical discovery gap in queue item M05 without adding a second
spectral renderer, interpolation rule or realtime policy.
The exact app build and focused spectral-authoring test passed (201 assertions).

## First fit results

Final captures and identities are in
`.context/sax-anchor-fit/ready/results.json`; the repeated nine-case
suite and independent response measurements are under `bridge-renders/` and
`bridge-assessment.json` in the same directory. The source audio remains
byte-identical to the pinned VCSL downloads. The three new presets are installed
under `cycle-v2/content/presets/tenor-anchor-*.cyclegraph`; the earlier expressive
preset and frozen studies remain intact.

| Measurement | Layer 2 | Layer 3 |
| --- | ---: | ---: |
| Source canonical median f0 | 293.6638 Hz | 293.6444 Hz |
| Raw full-note RMS error | −0.130 dB | −0.013 dB |
| Settled audible harmonic mean absolute error | 1.531 dB | 0.534 dB |
| Earlier expressive preset, same harmonic metric | 9.460 dB | 7.642 dB |

The harmonic metric uses H1–32 above −45 dB relative to the source fundamental
and one full-note RMS gain; it does not measure perceptual similarity. Attack
10/50/90-percent crossings match within the 10 ms measurement resolution.
Layer-2 final −40 dB tail crossing is approximately 50 ms early. Tail harmonic
errors remain larger (2.51/3.60 dB), and remaining-band power is not a separated
noise stem.

The final cable-pan suite renders each case twice with exact PCM repeatability.
CC2 endpoints equal the corresponding fixed-anchor captures sample for sample.
CC2 0/32/64/96/127 increases common-interval RMS from 0.02095 to 0.06203 and
power centroid from 539 to 874 Hz. The held sweep has no silent 10 ms frames or
same-age endpoint RMS overshoot; its largest adjacent RMS step is 0.401 dB.
Nearby MIDI 60/64 are finite and non-silent. Their spectral bands assume expected
pitch; neither these checks nor the bridge have independent acoustic targets.

The first fit uses 16 nodes, 4,624 vertices and 928 cubes: corresponding
48-harmonic channel models with nine magnitude time knots and static settled
phase. It contains no sample assets or playback bank, but its geometry is not
yet simplified. Fine pitch fluctuations, breath/noise, evolving phase and wider
register fidelity remain open. Further fitting should follow listening feedback
and reduce redundant shape detail before expanding the anchor bank.

The compact listening page now retains all 32 originals, adds Source/Synth
buttons on the two fitted rows, and provides endpoints, intermediate settings,
sweep, nearby notes and the embedded phrase. Browser verification covers every
reference and new clip, seeking, one shared player, compact rows, notes,
provenance, export and narrow layout. The fixed presets explicitly state that
CC2 is inactive and contain a full-note demonstration; the bridge has the
13-second MIDI/CC phrase.

Reproduce input collection, measurement, authoring and the page using the
[study instructions](../../cycle-v2/content/studies/expressive-sax/README.md#reference-anchors-and-first-bridge).
The current run reuses earlier hash-verified canonical pitch exports; a fresh
run can collect the same two originals with `anchor-references.json`.

Verification: exact CycleV2 app and focused spectral-authoring tests pass;
offline measurement and reference-page tests pass. Browser checks exercise all
32 originals, both synth matches and nine bridge clips. The architecture audit
introduces no new size trigger: changed production owners are the 306-line
synthesis discovery adapter, 405-line workspace command adapter and 27-line
header. Runtime/DSP and interaction complexity are unchanged. The fit runner
keeps capture orchestration separate from offline measurements and authored
model construction. Generated preset geometry accounts for most diff volume.

## Acceptance and evidence

- Two original identities and canonical measured pitches are recorded. Full-note
  comparisons retain source offsets, raw captures and declared level matching.
- Both anchors improve harmonic-envelope and amplitude/timing comparisons over
  the current expressive preset, with attack/body/tail failures reported.
- Shared control endpoints reproduce the anchor construction; 25/50/75 percent
  and a held-note sweep produce finite audio and documented spectral/amplitude
  trajectories. Nearby-note checks expose local-fitting limitations.
- Saved presets reload and the shared bridge has an embedded MIDI/CC audition.
  Captures retain exact engine/preset/event identities and repeatability proof.
- Final presets contain no Stereo Split/Join nodes. Independent channel paths
  use cable panning and an ordinary Add, with captures from this final routing.
- The listening page exposes individual Source/Synth comparisons and bridge
  auditions with notes, provenance and export intact. Browser checks exercise
  the compact rows and new comparison controls.
- Reread changes for reuse, complexity and ownership; run focused checks and
  applicable audits, commit coherent completed slices and record actual results.

No new runtime node, neural training infrastructure or sample-playback bank is
planned. If the current model cannot capture a salient reference component,
report the residual and queue the missing capability instead of equating more
control routes with improved acoustic fidelity.
