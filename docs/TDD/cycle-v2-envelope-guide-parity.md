# Cycle V2 Envelope Guide Parity

## Status

In progress (2026-09-17). Typed graph targets, shared guide preparation,
Envelope editor controls, runtime provider routing, focused preset repair, and
conversion tests are implemented. Direct audible A/B parity against Cycle 1
remains to be captured; provider wiring alone is not an audio-parity claim.

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
guide-affected curve. A direct audio A/B comparison to Cycle 1 remains open.
