# Cycle V2 Envelope Guide Parity

## Status

In progress (2026-09-18). Typed graph targets, shared guide preparation,
Envelope editor controls, runtime provider routing, focused preset repair, and
conversion tests are implemented. Per-Unison guide phase sampling and an
isolated playback comparison against Cycle 1's mature `EnvRasterizer` now pass.
A same-source full-output Cycle 1/Cycle V2 A/B still differs substantially;
the exact legacy PRNG draw position and other full-preset synthesis boundaries
remain open. Provider wiring alone is not an audio-parity claim.

## Evidence and authority

Cycle 1's `VertCube` stores one guide index and gain per dimension. Its
`EnvelopeInter2D` uses the same cube storage, and the mature Envelope
materialization/playback code already accepts a `GuideCurveProvider`. The
canonical `BrassSection.json` pitch Envelope cube 1 assigns guide index 1 to
Time with gain 0.462496251. The current `brass-section.cyclegraph` retains
that cube metadata but has no graph-level Envelope guide assignment. Its file
has unrelated user edits and was patched surgically, never regenerated.

Canonical `Organ.json` includes a Red/key guide on its first magnitude layer;
the corresponding Cycle V2 graph already has a Red assignment. `Vigil.json`
first magnitude layer cube 0 has Time and Curve guides; both assignments are
already present in `vigil.cyclegraph`. Focused tests verify these assignments
and gains are prepared. The Trimesh side-panel previously hid guide controls
for Time, Red, and Blue rows, making valid assignments appear absent; all six
rows now expose them.

## Design

Use graph assignments as authoritative identity and the existing cube gain as
authoritative strength. Extend the typed target to distinguish Trimesh and
Envelope cube components while preserving compatibility with existing graph
files. The serializer, validation, command service, and automation should
accept the Envelope target and reject wrong-family targets. Guide preparation
should reuse the existing `GuideCurveSnapshotProvider` and cube assignment
logic; it must not copy curve evaluation or envelope rasterization algorithms.
Envelope configurations own the prepared provider for their lifetime and pass
it to both the existing `EnvRasterizer` and realtime materialization plan.

### Unison guide phase ownership (2026-09-18)

Cycle 1 calls `EnvRasterizer::ensureParamSize` at note start, then
`EnvRasterizer::updateOffsetSeeds` in `SynthesizerVoice::initialiseEnvMeshes`.
For pitch and scratch Envelopes in one-sample-per-cycle mode, that delegates to
the shared `EnvelopePlaybackEngine::deriveVoiceOffsets`, which gives every
Unison voice a distinct phase and vertical guide offset. The Cycle V2
`PreparedCycleEnvelopeBank` already created the same per-lane playback voices
but did not seed them before this slice. Its guide sampling used zero offsets
for every lane. Cycle V2 also passed the ordinary baked Envelope view to the
per-cycle bank. That view had no guide regions to sample at voice-specific
phases, so adding offset seeds alone could not change the sound.

The bank owns the Envelope playback engines, so it should derive offsets once
per note lifecycle from the owning audio voice's seed. Both chained and
spectral region renderers must seed the bank before the first Envelope advance;
chained mode currently advances before its oscillator random setup. Reuse the
shared offset derivation, with no new guide sampler. Prepare a second,
decoupled Envelope materialization view for the per-cycle bank while retaining
the ordinary baked view for blockwise output. Both use the same mesh and guide
provider, and both track live Red/Blue morph on note preparation. This reuses
the authoritative materializer. Seed work is O(Envelope entries × Unison
lanes) once per lifecycle; movement through a cycle remains
O(1) per lane and does not copy graph or mesh state. The stable end state is
the same bank and shared playback engine, with no compatibility adapter to
delete. Validate distinct lane offsets, stable reseeding with the same seed,
new offsets on a new note seed, and the effect on a guided pitch Envelope.
The bank currently derives one stable seed per attached Envelope from the
audio voice's lifecycle seed; it does not reproduce Cycle 1's exact PRNG draw
position across all volume, pitch, and scratch layers. Exact seed-sequence
parity remains part of the isolated Cycle 1/Cycle V2 comparison.

The Envelope editor exposes meaningful component guide attachments and gains
for the selected logical cube. It must not expose the hidden Time-pole storage
vertices as extra selectable vertices. Trimesh Time/Red/Blue rows expose their
already-supported assignment/gain controls. Each guide edit goes through the
semantic graph command path and triggers the same local invalidation policy as
other guide assignments.

Conversion of a fresh Cycle 1 preset should emit Envelope assignments from
`mainMesh.cubes` as well as Trimesh assignments, retaining the cube gains
unchanged. Do not bulk-regenerate the checked-in preset library. Patch only
reviewed named presets after exact source/target comparison.

## Completion checks

- Focused canonical-source tests cover Organ Red, Vigil first spectral Time
  and Curve, and Brass Section pitch Envelope Time assignment and gains.
- Existing Organ/Vigil assignments remain active and visible; no unnecessary
  preset rewrite occurs.
- Brass Section pitch Envelope guide is graph-assigned and audibly/visibly
  affects the existing Envelope path in editor and realtime playback.
- New conversion and serializer round-trips preserve Envelope guide identity
  and gain. Wrong-family targets fail validation.
- Focused UI automation covers attachment visibility/selection and gain edit.
- Build, focused tests, audio parity, and runtime fixture pass; inspect the
  production diff for any duplicate guide or Envelope DSP logic.

## Verification so far

The targeted Organ/Vigil/Brass C++ checks, graph undo/serialization tests,
converter suite, and Brass Envelope guide-gain UI fixture pass. The Brass
playback test renders a 512-sample guided/unguided pair and observes a real
output difference. The fixture shows G2 with gain 0.4625 on pitch cube 1,
edits the gain, and verifies undo.
The editor screenshot at `/private/tmp/cycle-v2-envelope-guide-os.png` shows the
guide-affected curve. The isolated shared-mesh playback comparison is recorded
below.

### Direct Brass Section audio capture (2026-09-17)

Cycle 1's checked-in `cycle/content/presets/BrassSection.cyc` was exported with
`exportPresetFile` to `/private/tmp/envelope-guide-parity/BrassSection.json`.
Pitch Envelope cube 1 contains Time guide index 1 and gain 0.462496251 in the
export. The normal converter produced
`/private/tmp/envelope-guide-parity/brass-from-v1.cyclegraph` from that exact
source; it contains a typed Envelope Time assignment to `guide2`. The tests
preset's graph migrator had to be rebuilt before the converter could accept
the Envelope target. Conversion then passed without an override, and all 67
converter tests passed.

The standalone Debug Cycle 1 and Cycle V2 apps rendered a 1.3 s, 44.1 kHz,
512-sample-block note with note-off at 0.8 s. Cycle 1 played MIDI 72 and Cycle
V2 played MIDI 60, matching the converter's legacy 12-semitone reference
translation. Separate-process Cycle V2 captures repeated bit-for-bit with the
same seed.
Removing only the pitch Envelope guide assignment from the converted graph
changed the Cycle V2 output: in the 250–750 ms window, the guided-versus-plain
mixdown difference RMS was 0.154534 while the guided RMS was 0.107258. This
confirms a substantial downstream audio effect, in addition to the focused
Envelope playback test. The 16-command Brass guide-gain UI fixture also passed.

The direct Cycle 1/Cycle V2 full-output A/B did **not** meet parity. In the
250–750 ms window, its best correlation within 512 samples of alignment was
0.098, with a gain-matched normalized residual of 0.995. Full-output peak/RMS
were 1.343/0.203 for Cycle 1 and 0.815/0.123 for the freshly converted Cycle
V2 graph. These measurements do not isolate the Envelope: the existing strict
audio-parity subset excludes active pitch envelopes, and the full presets also
cross other synthesis and effect boundaries. Do not use this full-output
failure to adjust the guide algorithm or claim guide-specific cross-engine
parity. Compare the mature rasterizer and prepared Cycle V2 Envelope under
matched morph, note, and guide seed before closing this TDD.

### Per-Unison guide phase repair (2026-09-18)

`PreparedCycleEnvelopeBank` now derives distinct guide phase and vertical
offsets for its Unison lanes once per note seed through the shared
`EnvelopePlaybackEngine`. Both chained and spectral renderers seed it before
the first per-cycle Envelope advance. For guided pitch/scratch Envelopes, the
processor prepares a decoupled cycle playback view with the same mature
materializer and guide provider; its ordinary blockwise output keeps the
baked view. Unguided Envelopes incur no second materialization.

The focused Brass test verifies that the cycle view contains guide regions,
the ordinary view is baked, three guided lanes differ, repeating a seed repeats
their trajectories, changing the seed changes them, and removing the guide
makes the lanes agree. It passes 13 assertions. The existing Brass prepared
playback test passes 17 assertions, and the allocation-free ordinary Envelope
note-preparation test passes 9 assertions. Two fresh-process guided Cycle V2
captures with the same seed have identical raw float SHA-256 hashes. In the
250–750 ms window, the new guided-versus-unguided Cycle V2 output difference
RMS is 0.153510 against guided RMS 0.102588.

An isolated Brass pitch Envelope comparison now runs Cycle 1's mature
`EnvRasterizer` and Cycle V2's prepared bank on the same mesh and guide provider,
with the same explicit offset seed and three Unison lanes. Their 512-cycle
`getSustainLevel`/`pitchValue` trajectories match exactly. This verifies the
materialization and playback boundary under a matched seed; it does not verify
that the two full apps choose the same seed at the same legacy PRNG draw.

The fresh Cycle 1/Cycle V2 full-output comparison remains far from parity:
best correlation in that window is 0.062 and gain-matched normalized residual
is 0.998. Exact Cycle 1 PRNG draw translation and the other full-preset
boundaries must be resolved before marking this TDD complete.
