# Cycle 2 Curve Identity And Edit Commands

## Status

Proposed.

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

