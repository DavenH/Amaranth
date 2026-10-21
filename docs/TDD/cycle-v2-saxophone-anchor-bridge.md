# Saxophone reference anchors and first bridge

Status: First two anchors and shared bridge implemented; acoustic listening
acceptance remains open. The user approved fitting convincing individual reference
models before unifying their expression. This supersedes extending the inherited
factory timbre with further generic controller routes as the immediate task.

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
