# Cycle V2 Preset Model Cache Coherency

Status: Complete

## Problem

Cycle V2 reuses DSP configuration publishers across graph compilations. Presets
commonly reuse node ids and begin model revisions at one. The configuration key
currently identifies a model only by schema and revision, so distinct immutable
models from consecutive presets can collide. The old prepared Trimesh then
drives previews, probes, and audio even though the durable graph contains the
new preset model.

## Design

`NodeEditorHost::bindingFingerprint` is the authoritative precedent for
document-local model revisions: it combines schema, schema version, revision,
and immutable model object identity. `NodeDspConfigurationFactory::keyFor`
will use the same identity tuple.

The pointer is an in-process cache identity only; it is not serialized or used
as durable model state. Graph copies retain their shared immutable model pointer
and therefore keep O(1) cache reuse. A model edit or newly decoded preset owns a
new immutable model object and therefore prepares a new configuration without
serializing, comparing, or copying the mesh merely to form the key.

Preset installation remains owned by `GraphDocument` and presentation/audio
publication remains unchanged. The stale configuration is removed at the
configuration-cache boundary rather than patched independently in previews,
spies, or the realtime renderer.

## Completion Criteria

- [x] Reusing one compiler/presentation model across two presets with the same
  Trimesh node id and model revision produces the second preset's exact time
  traversal grid.
- [x] The second preset's probe payload agrees with a clean standalone load.
- [x] Existing same-model configuration reuse remains intact and O(1).
- [x] Focused tests, the preset-transition automation fixture, standalone build,
  style checks, and `git diff --check` pass.

## Deletion Target

- The schema-and-revision-only model fragment in DSP configuration keys.

## Verification

- The focused same-instance presentation test passes 10 assertions, including
  exact time traversal, Spy 1 payload, and final audio equality against a clean
  Baroque Flute load.
- The African Horn to Baroque Flute automation transition passes all commands;
  `timeLayer1` and Spy 1 both report an absolute sum of `3529.3626` rather than
  the stale transition value near `1489.7`.
- The production-size app-side canvas capture shows the Baroque time node and
  Spy 1 with matching mesh content. The preferred OS crop completed the fixture
  but was unavailable at the wrapper capture step.
- The standalone Debug target builds successfully.
- The complete CTest matrix passes all 1,049 discovered tests in 202.58 seconds.
