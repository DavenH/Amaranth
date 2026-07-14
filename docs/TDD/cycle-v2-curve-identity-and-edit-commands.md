# Cycle 2 Curve Identity And Edit Commands

## Status

Implemented (2026-07-14).

Depends on `cycle-v2-curve-node-models-and-editors.md`.

## Problem

Flat curve publication currently recovers vertex identity by comparing edited
coordinates and curve values. Moving a vertex changes those values and can
assign a new identity, which loses selection and prevents semantic model
commands. Envelope selection is represented by cube index, so topology edits
can redirect selection to a different cube.

The domain models therefore serialize identities, but the live editing path
does not preserve them reliably.

## Goals

- Give every editable flat vertex and Envelope cube a stable model identity.
- Preserve identity and selection through movement, curve changes, insertion,
  deletion, ordering, and publication.
- Express editor mutations as validated model operations.
- Keep existing `Mesh`, `EnvelopeMesh`, panels, and interactors as the curve
  implementation rather than reimplementing curve behavior.
- Reject invalid edits atomically without partially mutating the live mesh.

## Non-Goals

- Replace the mature mesh interactors or rasterizers.
- Change curve interpolation, Envelope topology semantics, or visuals.
- Define graph undo or cross-node publication; that belongs to
  `cycle-v2-curve-state-publication.md`.

## Target Design

Introduce domain identities that survive independently of storage position:

```cpp
using CurveVertexId = uint64_t;
using EnvelopeCubeId = uint64_t;

class FlatCurveModel {
public:
    CurveEditResult moveVertex(CurveVertexId, CurvePoint);
    CurveEditResult setCurve(CurveVertexId, float);
    CurveEditResult insertVertex(CurvePoint);
    CurveEditResult removeVertex(CurveVertexId);
    CurveEditResult selectVertex(std::optional<CurveVertexId>);
};
```

Envelope exposes equivalent cube/topology operations using
`EnvelopeCubeId`. Selection is never represented by array index outside the
mesh adapter.

A narrow adapter maps stable model identities to the mature mesh objects used
by the panel. The mapping must be updated by explicit insert/delete/reorder
events. It must not infer identity from floating-point equality after editing.
If the existing interactor cannot expose those events, add the smallest
adapter callback at that boundary rather than duplicating its algorithms.

## Validation And Revisions

- Each accepted semantic edit advances the model revision once.
- Selection-only changes do not advance DSP configuration revision unless
  selection is intentionally persisted as document state.
- Failed edits leave model data, mesh data, selection, and revision unchanged.
- Duplicate or missing identities are invalid serialized state.
- Loading a snapshot reconstructs the identity-to-mesh mapping deterministically.

## Migration Plan

### Slice 1: Identity Types And Tests

- Introduce typed vertex and cube identities.
- Replace Envelope cube-index selection in the domain model.
- Add movement, insertion, deletion, and reorder identity tests.

### Slice 2: Mesh Identity Adapters

- Add flat and Envelope identity adapters around existing mesh objects.
- Preserve selection when interactors rebuild or reorder mesh storage.
- Remove floating-point identity recovery from `adoptEditedVertices`.

### Slice 3: Semantic Editing

- Route panel edit notifications through model operations.
- Publish one model revision per committed gesture.
- Remove remaining whole-mesh reconciliation from normal editing.

## Verification

- Moving a selected flat vertex preserves its ID and selection.
- Editing only curve/sharpness preserves identity.
- Inserting before a selected vertex does not change the selected identity.
- Deleting another vertex does not redirect selection.
- Deleting the selected vertex clears selection explicitly.
- Envelope cube insertion, deletion, and reorder preserve unrelated IDs.
- Invalid edits are atomic.
- Snapshot round trips preserve all IDs and the selected identity.
- Existing curve interaction and OS-captured OpenGL visuals remain unchanged.

## Completion Criteria

- No editing path derives identity from coordinates or container index.
- Flat and Envelope selections use stable typed identities.
- Normal editor gestures call semantic model operations.
- Model and mesh cannot disagree after a committed edit.

## Implementation Notes

- `CurveVertexId` and `EnvelopeCubeId` are stable document identities.
  Flat snapshots retain their vertex IDs; Envelope snapshot version 2 stores a
  validated ID for every cube and selection by cube ID.
- `FlatCurveModel` provides atomic move, curve, insert, remove, and selection
  operations. Rejected edits leave vertices, selection, mesh, and revision
  unchanged, while accepted content edits advance the revision once.
- Semantic edits retain the mature mesh objects: move and reshape mutate the
  identified `Vertex` directly, insertion adds one `Vertex`, and deletion
  removes one `Vertex`. Whole-mesh reconstruction is limited to document
  replacement, snapshot loading, and rollback after an invalid external edit.
- Flat model and mesh storage retain document order; spatial consumers sort a
  temporary view only where x-order is required. Direct ID-to-entry and
  ID-to-`Vertex` maps make single-point move, reshape, and selection expected
  O(1), while IDs come from monotonic counters rather than container scans.
- The active `currentVertex` takes precedence when committing selection after
  a drag, so a retained multi-selection cannot substitute a different vertex
  identity. Envelope topology reconciliation uses pointer hash sets and maps
  and is expected O(n), rather than repeatedly scanning cubes in O(n²).
- The live panel adapter maps identities to the mature interactor's stable
  `Vertex*` and `VertCube*` objects. Gesture commits classify retained, new,
  removed, and reordered objects by pointer identity, never coordinates or
  container index.
- Selection-only synchronization updates stable selection without advancing
  the content revision. Moving and reshaping retain identity; insertion gets a
  new monotonic identity; deletion clears only a deleted selection.
- Flat and Envelope panel gestures synchronize the model before publishing the
  typed snapshot. Whole-mesh floating-point identity recovery was removed.

Verification on macOS:

- Focused curve-model suite: 156 assertions in 17 test cases.
- Full `CycleV2_tests`: 2,463 assertions in 238 test cases.
- `cycle-v2-agent-effect2d-interaction.json` exercises direct synthetic event
  forwarding only. It is adapter-level coverage and is not evidence that JUCE
  or macOS delivers working product interactions.
- OS-driven verification uses a persistent CycleV2 session plus `cliclick`.
  It confirmed automatic Waveshaper hover resolution, retained vertex identity
  through a drag from `(0.125, 0.125)` to approximately `(0.272, 0.272)`, and
  publication to the node snapshot. It also confirmed Envelope vertex dragging,
  publication, and stable selected-vertex parameters after moving the pointer
  elsewhere.
- The OS-driven run exposed and fixed two host-boundary defects that the
  synthetic fixture bypassed: duplicate event delivery through both JUCE
  listener registration and manual forwarding, and render-thread snapshot
  loading that rebuilt the editor mesh between pointer events.
- `scripts/test_cycle_v2_native_edit_smoke.py` is the native acceptance smoke.
  It uses AppKit/JUCE delivery for add, move, curve reshape toward the vertex,
  delete, and re-add, then exercises the corresponding Trimesh editing path,
  collision rejection, vertex controls, and morph controls in the same app
  session. Per-edit settling is capped at 80 ms.
