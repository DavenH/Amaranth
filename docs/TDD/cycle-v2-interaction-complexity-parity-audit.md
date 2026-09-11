# Cycle V2 Interaction Complexity Parity Audit

## Status

Audit complete; remediation required. P0 findings are open (2026-09-11).

## Trigger

A Trimesh movement publication caused `syncGuideContext()` to prepare and
deep-copy the complete mesh back into the live panel during the same native
gesture. Besides turning a local vertex movement into whole-mesh work, the
replacement invalidated the mature interactor's retained pointers and caused a
native crash. The immediate lifetime defect is fixed, but the same cost-class
regression exists in other Cycle 2 interaction paths.

This audit treats movement-update asymptotic parity as semantic correctness.
Faster hardware, 30 Hz throttling, background execution, and small factory
presets do not make an O(n) replacement acceptable when Cycle 1 applies an
O(1) or O(k) delta. One gesture-boundary undo capture is a temporary allowance,
not permission to copy unrelated state or the intended final undo design.

## Cost Vocabulary

- `G`: total graph nodes, edges, Guide resources and assignments.
- `A`: total samples in graph-owned audio resources.
- `V`, `C`: vertices and cubes in the edited domain model.
- `P`: parameters on the edited node.
- `K`: objects intentionally changed by the gesture, normally one vertex,
  node, or scalar and occasionally the linked selection.
- `R`: samples or pixels in the local render product that visibly must change.

`K` and `R` are permitted costs. Work that grows with unrelated `G`, `A`, `V`,
`C`, or editor-state size is not.

## Authoritative Cycle 1 Contract

- `Interactor::doDragVertex()` and `moveSelectedVerts()` update the retained
  selected frame. Their domain cost is proportional to the selected/linked
  vertices and the affected raster range, not the complete document.
- `VertexTransformUndoer` captures the affected vertex frames rather than a
  complete application graph.
- Curve point movement delegates to the incremental rasterizer, whose explicit
  contract is the adjacent affected curves and waveform ranges. In-order point
  movement performs no full sort, serialization, or rebuild.
- Hover state, morph positions, node/component bounds, and ordinary scalar
  controls are retained interaction state. Changing one does not clone an
  unrelated document or resource collection.

Cycle 2 may add a semantic command boundary, immutable runtime publication,
and asynchronous derived products. Those are ownership translations, not
permission to change local interaction complexity.

## Audited Continuous Paths

| Cycle 2 path | Mature delta | Current synchronous work | Verdict |
|---|---:|---|---|
| Canvas node move/resize | `O(K)` | Full graph copy at compound begin; each resize performs linear node lookup. Multi-node translation scans all nodes and searches the selected-id vector. | P0 update / P1 boundary |
| Compact Pan and Output Gain | `O(1)` | Full graph copy at gesture begin, `O(G + P)` node/parameter lookup per update, full graph undo copy at commit. | P0 update / P1 boundary |
| Generic and paired parameter sliders | `O(1)` or `O(2)` | Full graph copy at begin, `O(G + P)` command lookup per update, full graph undo copy at commit. | P0 update / P1 boundary |
| Trimesh morph slider | `O(1) + O(R)` | Same graph-copy and lookup costs as generic sliders; local slice work is legitimate `R`. | P0 update / P1 boundary |
| Trimesh point/curve movement | `O(K) + O(R)` | Local mature movement is retained, but every throttled publication calls `TrimeshNodeModelState::copyOf()`, which deep-copies all `V + C`; selection publication also stringifies editor JSON. | P0 |
| Trimesh selected-vertex/Guide-gain rail | `O(1) + O(R)` | Every delivered value deep-copies all `V + C` into a new immutable model before graph publication. | P0 |
| Waveshaper/IR/Guide flat-curve movement | `O(K) + O(R)` | `FlatCurvePanelAdapter::registerMeshEdit()` serializes all `V` merely to detect change; publication synchronizes and copies all `V`. | P0 |
| Envelope point, curve, marker, morph-plane and vertex rail | `O(K) + O(R)` | Change detection compares the full mesh and deep-copies it; publication synchronizes and copies the complete Envelope model, including mesh state. | P0 |
| Guide curve publication | `O(K) + O(R)` | Inherits flat-curve full serialization/copy, scans assignments to collect consumers, and may allocate and prepare the 512-sample heatmap path during publication. | P0/P1 |
| Hover, viewport pan/zoom, dock resize, performance keyboard | Local retained state | No graph/model publication was found in the delivered movement path. Hit testing or painting may scale with visible items by definition. | Pass, retain guards |

## Systemic Findings

### P1: gesture transactions copy unrelated graph and audio data

`GraphCommandDispatcher::beginTransientEdit()` constructs `TransientEdit {
document.graph() }`. `beginCompoundEdit()` also copies `document.graph()`, and
transient commit copies it again for undo. `NodeGraph` owns nodes, edges,
assignments, and `AudioSampleResource::samples` vectors by value, so starting an
ordinary scalar gesture is `O(G + A)` and allocates in proportion to unrelated
content.

Temporary allowance: one snapshot may be captured at pointer-down or commit for
undo while the delta mechanism is being introduced. It must be narrowed to the
affected node/domain state and must not include unrelated graph topology or
audio payloads.

Required end state: a gesture owns a bounded semantic delta over an immutable
durable base. Applying the delta performs edit/redo; applying its inverse
performs undo. Audio payload storage is shared immutable ownership and is never
copied by a graph metadata edit.

### P0: immutable publication is being constructed inside movement callbacks

Trimesh, flat-curve, and Envelope editors create complete immutable model
snapshots during movement. Throttling reduces frequency but does not change
complexity. Moving the copy to a worker would reduce message-thread latency but
would still violate the operation contract.

Required end state: local mature models remain gesture-owned. Movement emits a
typed delta containing stable domain identity and changed fields. Durable model
storage applies that delta with structural sharing; derived runtime preparation
may consume the resulting immutable revision asynchronously.

### P0: full serialization and equality are used as change detection

`FlatCurvePanelAdapter::registerMeshEdit()` serializes the complete curve on
each edit notification. `NodeGraph::replaceNodeEditorState()` converts both
states to JSON strings for equality. Envelope change detection walks and then
copies the complete mesh.

Required end state: the owner that performs the mutation returns a typed change
result and advances a content revision only for an accepted delta. Equality and
serialization remain validation/persistence operations, never gesture probes.

### P1: ostensibly addressed graph operations still scan collections

`NodeGraph::findNode()` and `findNodeForEditing()` are linear. Parameter changes
then linearly scan `Node::parameters`. `translateNodes()` scans all nodes and
does a linear membership test against selected IDs. Guide publication scans
assignments to derive consumers even though relationship indexes already
exist.

Required end state: stable ID indexes provide average `O(1)` node and parameter
resolution, and multi-object edits iterate the changed IDs. Existing Guide
indexes, rather than fresh assignment scans, drive invalidation.

### P1: gesture-boundary undo captures are not delta-sized

Cycle 1 vertex undo records affected vertex frames. Cycle 2 records a complete
pre-change `NodeGraph`, including unrelated resource data. This is less severe
than copying during every movement, and one gesture-boundary capture is an
acceptable migration step, but its current scope is still wrong.

Required end state: undo records an invertible semantic delta. Redo reapplies
its forward form and undo applies its inverse. Single-object commit cost is
independent of unrelated graph and audio-resource size.

## Negative Boundaries

- No aggregate state copy in a movement callback.
- During migration, at most one undo snapshot may occur per gesture boundary;
  it contains only the smallest affected domain state and no unrelated graph or
  audio payload. The snapshot is a deletion target for invertible delta undo.
- No complete Mesh/domain-model copy, serialization, or JSON conversion in a
  delivered movement callback.
- No synchronous graph compilation, DSP preparation, Guide preparation,
  preview traversal, or unrelated editor rebind in local gesture feedback.
- No test may use a small fixed graph or mesh as evidence of constant work.
- No timer, debounce, cache, worker, or faster equality function may be used to
  conceal an operation whose input scope is wrong.

## Enforceable Audit Surface

Add fixed-cost diagnostic counters at the ownership boundaries, not ad-hoc
timers at call sites:

- graph root/full-container copies and copied audio-sample count;
- full Mesh/domain-model copies and vertices/cubes copied;
- model/editor serialization and JSON equality conversions;
- full node/parameter/assignment scans;
- local slice, full surface, preview traversal, DSP preparation and editor
  rebind counts.

Every continuous interaction test must execute at least two accepted updates,
a no-op/constrained update, commit, and undo. Repeat the same gesture after
adding unrelated nodes, edges, audio samples, vertices, and Guide assignments.
Forbidden counts remain zero and permitted counts depend only on `K` and `R`.

## Remediation Sequence

1. Add operation counters and scaling fixtures so every red row fails for the
   correct reason before production ownership changes.
2. Replace value-copied graph transactions with a typed edit overlay. Share
   immutable audio resources and store an invertible delta whose forward and
   inverse forms implement redo and undo.
3. Add average `O(1)` node/parameter indexes and changed-ID iteration without
   exposing mutable graph access to UI code.
4. Introduce domain edit sessions for Trimesh, Envelope, and flat curves. Keep
   mature local interaction unchanged; replace per-update snapshots with typed
   deltas and structurally shared immutable model revisions.
5. Move Guide consumer resolution to existing indexes and separate visible
   local heatmap feedback from durable/runtime preparation.
6. Run the counter matrix, semantic gesture suites, native gestures, full
   Cycle 2 tests, and standalone Debug build. Remove the old full-copy and
   serialization-based change-detection paths rather than retaining fallbacks.

## Completion Criteria

- Every audited row has an explicit passing operation-count test at small and
  scaled unrelated input sizes.
- Scalar and single-object interactions have the same asymptotic phase costs as
  Cycle 1; local render cost is isolated and named.
- No movement update copies or serializes a full graph or domain model.
- No non-topological gesture copies audio samples or the full graph for undo;
  undo and redo apply inverse/forward semantic deltas.
- The production diff contains no compatibility adapter that duplicates mature
  interaction, rasterization, topology, or DSP behavior.
- The causal-update TDD consumes the same typed deltas and counters rather than
  maintaining a second interaction publication mechanism.
- Focused and full tests, native macOS acceptance, style review, hot-loop review,
  and `git diff --check` pass; remediation is committed in coherent slices.
