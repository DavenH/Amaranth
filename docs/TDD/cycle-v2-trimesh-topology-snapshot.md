# Cycle V2 Trimesh Topology Snapshot

## Status

Implemented.

Implemented in July 2026. `mesh.topology` now adapts the authoritative Mesh
JSON contract across the editor, graph serializer, DSP configuration, realtime
fallback, and preview paths. Sparse indexed persistence and its selected-vertex
fallback were deleted. Live gestures retain Mesh identity, refresh locally,
and publish one complete snapshot at the semantic boundary.

## Problem

Cycle V2 persists Trimesh edits as a set of indexed parameters such as
`mesh.vertex.4.amp`. This records some field values but not vertex ownership,
cube membership, guide assignments, or topology. Add/delete therefore works
only in the live widget and cannot round-trip the authored mesh exactly.

`TrimeshNodeModel` also contains a selected-vertex override fallback. Cycle V2
is a new graph format, so retaining parallel sparse and fallback persistence
paths would calcify ambiguity rather than provide useful compatibility.

## Existing Authoritative Boundary

`Mesh::writeJSON()` and `Mesh::readJSON()` already encode vertices, cube vertex
IDs, guide channels, and guide gains. Cycle V2 must adapt that mature format;
it must not invent another topology schema or reconstruct cubes from sparse
field overrides.

## Design

Introduce `TrimeshMeshState` as the narrow Cycle V2 adapter around the mature
Mesh JSON contract:

- serialize a complete Mesh to one canonical graph parameter,
  `mesh.topology`;
- validate and apply the snapshot atomically to a temporary Mesh before
  replacing live model state;
- retain the last accepted snapshot so ordinary node synchronization does not
  rebuild unchanged topology;
- persist selection separately as editor intent, while mesh identity and cube
  membership live exclusively in the topology snapshot.

The editor command service publishes a snapshot at semantic edit boundaries.
Live drags may update the retained widget/model directly, but graph persistence
must be one conflict-checked compound edit rather than six indexed parameter
writes per vertex. Add, move, curve, guide, collision-approved topology, and
delete operations all use this same publication boundary.

DSP, preview, and expanded-panel models consume the same typed snapshot. No
high-level canvas or hosting class parses Mesh JSON.

## Deletion Targets

- Delete indexed `mesh.vertex.<index>.<field>` graph persistence.
- Delete `TrimeshMeshEditState` once callers consume `TrimeshMeshState`.
- Delete the selected-vertex override fallback and its parameter IDs.
- Remove tests that bless sparse indexed persistence; replace them with exact
  topology contracts.

## Semantic Tests

- A mesh with authored vertex values, cube membership, guide channels, and
  guide gains serializes and restores exactly through `TrimeshMeshState`.
- Invalid or incomplete snapshots do not partially mutate the active mesh.
- Add, move, curve, and delete followed by graph save/reload preserve the exact
  vertex and cube counts and the surviving cube-to-vertex mapping.
- One semantic edit advances graph history once and remains undoable.
- Audio DSP, preview traversal, compact preview, and expanded editor observe
  the same restored topology.
- The native macOS smoke saves and reloads its authored Trimesh graph and
  asserts exact topology rather than only selected field values.

## Performance And Realtime Constraints

- JSON parsing and Mesh allocation remain off the audio thread.
- Unchanged node synchronization performs an O(1) snapshot comparison and no
  mesh rebuild.
- Audio processors consume prepared immutable/owned mesh state and allocate
  nothing while processing.
- A drag publishes at its semantic boundary; it does not serialize the entire
  topology for every delivered mouse-move event.

## Completion Criteria

- `mesh.topology` is the sole durable Trimesh mesh representation in Cycle V2.
- Full topology survives graph serialization and authoring save/reload.
- Sparse indexed and selected-vertex fallback production paths are deleted.
- Focused model/DSP tests, full Cycle V2 tests, native authoring smoke, style
  review, and OS capture pass.
- The implementation is committed and this TDD is removed from
  `docs/TDD/refactors.md`.
