# Cycle 2 Point Dragging, Previews and Undo

## Status and starting point

Implemented for the scoped Waveshaper point drag on 2026-10-06. The wider mesh
storage refactor remains excluded.
Rewritten on 2026-10-05 after the user rejected the larger mesh-storage rewrite.
Start from master at `97bf9107` (PR #158), on `cycle2/point-drag-refactor`.
The old implementation remains on `cycle2/refactors-001`; its design notes remain
in Git history. Do not cherry-pick its storage changes as the starting design.
The four recovered fixes are described in `cycle-v2-salvaged-editing-fixes.md`.

## Goal

Make one existing Waveshaper point drag update its curve and downstream preview
correctly, with one undo action, while preserving Cycle 1 interaction behavior.
First establish where Cycle 2 does unnecessary copying or repeated work. Change
only the parts required by this interaction. Undo already exists.

## Scope

One selected Waveshaper point: press, at least two movements, release, undo,
redo and cancellation. Verify both Live and On Release preview modes.
Keep point identity, selection, constraints, curve evaluation and saved results.

Excluded: adding/deleting points, pencil drawing, multi-point gestures, Envelope
markers/loops, Trimesh topology, Guide assignments, file-format changes and a
replacement mesh-storage system. Other editor families are follow-up decisions.

## Existing implementations to reuse

| Responsibility | Current implementation |
| --- | --- |
| Point interaction and constraints | `lib/src/Inter/Interactor.cpp` and existing curve-panel interaction code |
| Cycle 1 undo capture | `lib/src/Inter/VertexTransformUndo.cpp` |
| Applying/restoring vertex values | `lib/src/Inter/UndoableActions.cpp` |
| Cycle 2 mesh-to-model conversion | `cycle-v2/src/Nodes/Curve/Panel/FlatCurvePanelAdapter.cpp` |
| Editor publication and gesture events | `cycle-v2/src/Nodes/Curve/Editor/CurveExpandedEditorComponent.cpp` |
| Semantic commands and graph undo | `NodeEditorCommandService`, `GraphCommandDispatcher`, `GraphDocument` |
| Preview timing and background work | Existing presentation session, refresh policy and scheduler |

Cycle 1 currently captures all mesh vertex values in `VertexTransformUndo::start`
and captures/compares them again at commit. Do not describe that undo path as
constant-cost. Its movement cost must be traced separately, including required
local rasterization; no measurement has yet been recorded here.

Cycle 2 currently marks transient curve changes during the gesture and calls
`publishModelState` at commit. `FlatCurvePanelAdapter::modelPublication` synchronizes
from the mesh and copies the typed model. This proves a whole-curve publication
cost, not that it occurs on every movement. Trace preview work before changing it.

## Work plan

1. **Measure the existing sequence.** Trace the real hosted Waveshaper drag in
   both versions. Record pointer-down, each movement, release, undo and redo costs
   separately. Count copied vertices/models/graphs, serialization, lookups,
   preparation, rebuilds and preview requests/publications. Identify each call
   site; distinguish required local curve rendering from avoidable state copying.
2. **Choose the smallest correction.** Write down the measured defect, the
   authoritative implementation reused, ownership and any exact type translation.
   Prefer existing commands and refresh services. If shared editing code must
   change, extract only the needed behavior and use it from both callers. Do not
   copy mature interaction algorithms or introduce parallel undo machinery.
3. **Implement and remove the replaced path.** Route semantic edits through the
   command service/dispatcher. Remove the duplicate copy or scheduling decision
   that the measurement identified. If no unnecessary work is found, keep storage
   unchanged and record that result; performance work needs evidence.
4. **Verify the complete interaction.** Compare observable results with Cycle 1,
   inspect the production diff, check style, run focused tests and app automation,
   and commit the complete change. Stop this plan after this one interaction works.

## Ownership and safety requirements

- The existing interactor decides movement and constraints; the UI reports events.
- The dispatcher owns transient graph edits, commit/cancel and the single undo
  action. Capture the durable starting revision once; movement revisions cannot
  replace that concurrency check. Consolidate all changed fields in one result.
- The presentation services own refresh timing and stale-result rejection. Workers
  must read stable data and never the editor's mutable mesh. Record how the chosen
  implementation retains that data until worker completion and prevents stale
  results from replacing newer previews. Do not assume this requires new storage.
- Undo/redo restores the actual changed values and existing identities. Keep one
  user-visible undo step. Any initial capture must stay within the affected domain;
  record its cost honestly rather than copying unrelated graph or audio content.
- Movement must not clone the whole graph/mesh/model, serialize state for equality,
  prepare durable resources or rebuild unrelated presentation. Required local
  rendering may scale with the affected curve; identify and count it separately.
- Preserve Cycle 1 behavior and do not increase the asymptotic cost of any phase.

## Evidence and completion

Use the existing hovered-point drag regression in `TestNodeEditorHost.cpp` as a
starting check, plus a focused app fixture exercising the actual hosted events.
Require two movements, an observable downstream change, release, one undo, redo,
cancel, stable selection, and saving/reloading the final curve through existing
formats. In Live mode, the downstream preview must reflect movement before release;
in On Release mode, it updates after release. Cancel restores the original state
without adding an undo step. Worker ordering must not publish an older final result.

Compare operation counts with the edited point held constant while unrelated graph
nodes/resources and unedited points grow. Separate local rendering counts from
editing/publication counts. Record baseline/after costs by phase, affected files and
sizes, policy owners, dependencies and actual deleted paths here in a compact table.

Completion requires semantic parity, safe worker reads, one correct undo action,
measured removal of the identified unnecessary work, no duplicated mature behavior,
architecture audit and applicable style checks. Tests must not bless approximations.
Do not label the broader mesh refactor complete from this result. If solving this
one interaction requires a storage rewrite or another excluded feature, document
the concrete blocker and reassess scope before adding that work.

## 2026-10-05 implementation investigation

The hosted point-drag and movement-notification tests pass (30 assertions in
two cases). They do not exercise downstream Live feedback. The source trace
shows the following costs and missing data path; runtime operation counts for
the complete hosted sequence remain to be collected.

| Phase | Cycle 1 | Cycle 2 hosted Waveshaper |
| --- | --- | --- |
| Press | `VertexTransformUndo::start` copies every mesh vertex; the hosted Cycle 2 panel suspends that undo path. | `beginCurveTransaction` opens a graph edit; the durable model remains unchanged. |
| Movement | `Interactor::mouseDrag` changes the selected mesh point and refreshes the local rasterizer. | The same interactor changes the panel mesh. `publishCurrentState` sends a fingerprint; `NodeCanvas::recordNodeEditorMovement` requests a refresh against a graph whose curve model still has the old point. |
| Release | `VertexTransformUndo::commitIfPending` copies and compares every mesh vertex. | `FlatCurvePanelAdapter::modelPublication` synchronizes every vertex from the mesh, then `CurveNodeModelState::copyOf` copies the complete curve model once. The dispatcher commits one graph edit. |
| Undo/redo | The Cycle 1 action restores the captured vertex values. | The graph delta restores the prior model; downstream refresh follows document publication. |

The existing worker path accepts a stable `NodeGraph` and prepares the
Waveshaper from its immutable `CurveNodeModelState`. It has no input for the
panel's mutable mesh or a single-point delta. Publishing a complete model on
each movement would violate this plan's movement cost and copy rules; handing
the mutable mesh to a worker would violate its lifetime rule. Passing the
current editing overlay to `GraphPresentationModel::refreshAsync` copies its
changed nodes but leaves a non-owning pointer to the mutable durable graph.
The existing presentation gesture session can make an owned changed-node
snapshot, but first copies the whole graph to own its stable base. The current
`NodeEditorCommandService` curve transaction opens a dispatcher edit directly,
so that gesture session is not used for these movements anyway.

The user authorized the stable point-delta preview and worker snapshot path.
The completed path keeps the mutable panel mesh on the UI thread. Each movement
publishes one immutable point value and identity in a curve model state that
shares its unchanged flat curve. `FlatCurvePreparation` substitutes that value
while producing the required local transfer. Release still converts the panel
mesh and publishes one complete model through the existing dispatcher undo path.

For Live feedback, `GraphCommandDispatcher` snapshots only edited overlay nodes
and borrows the durable graph as its unchanged base. The command service waits
for pending preview work before committing or cancelling the transient edit.
Canvas document load, undo and redo also settle preview work before replacing
or changing that base.
The scheduler's generation check rejects an older result after a newer request.
On Release movements use the panel's local rasterization and defer downstream
graph refresh until commit. The former On Release movement refresh changed the
probe prematurely and copied the graph; that scheduling path was removed for
this curve gesture.

| Phase | Before | After |
| --- | --- | --- |
| Press | Cycle 1 captures all selected mesh vertex values; Cycle 2 starts a transient edit. | Same transient edit; no graph or mesh copy added. |
| Each movement | Panel mesh changed, downstream Live read old model; asynchronous refresh copied the whole editing graph. | One selected point value, shared curve base, edited-node snapshot; zero graph, mesh and model copies or serialization in scaled operation-count tests. Required curve preparation still traverses the local curve. |
| Release | One complete curve model publication and graph commit. | Same single complete model publication and one undo entry; pending workers settle first. |
| Undo/redo/cancel | Graph delta restores committed state; cancel had no hosted Escape path. | Delta undo/redo and save/reload retain point identity; Escape cancels the hosted gesture, restores the panel and creates no undo entry. |

The operation-count test holds the edited point constant with 0 and 128 unrelated
nodes and a 16,384-sample unrelated resource. In both sizes, two movement
publications plus their worker graph snapshots record zero graph copies, mesh
copies, model serializations, audio-sample copies, node scans and validation
visits; parameter lookup remains two for the two movements at either size.
The hosted panel test also
records zero mesh and graph copies during two movements. The Live app fixture
observed downstream probe absolute sum change from 9150.68 to 5279.29 before
release; On Release retained 9150.68 until release, then changed to 5279.29.
The Live and On Release fixtures verified undo and saved-graph reload. The unit
sequence verifies redo and Escape cancellation. A third Live fixture releases
immediately after two movements, then verifies the final probe and undo after
the worker settles.

Policy owners: the legacy interactor owns point movement and constraints;
`CurveNodeModelState` owns the point preview value; `FlatCurvePreparation` owns
curve evaluation; `GraphCommandDispatcher` owns transient edits and undo;
`GraphPresentationModel` and its scheduler own refresh generations and worker
completion. `NodeCanvas` routes movement according to the existing refresh
policy. No interaction or DSP algorithm was copied into that orchestration.

The touched size triggers are `NodeCanvas.cpp` 2807 lines, `CurveNodeModels.cpp`
859 lines, `NodeCanvasAuthoring.cpp` 959 lines and `NodeEditorCommandService.cpp`
826 lines. Their added code is
limited to orchestration, typed preview state and curve transaction cleanup,
respectively. The old full-graph movement refresh and premature On Release
probe refresh were removed for this gesture. The broader class-size extraction
plans remain in `docs/TDD/refactors.md`; this change did not add a new switchboard.

Validation: `standalone-debug` and `tests` CycleV2 targets built with
`--parallel 10`; three focused Catch2 cases passed with 220 assertions and
`ctest --test-dir build/tests -R` passed 3/3; all
three hosted app fixtures passed after the On Release correction. The architecture
audit and `git diff --check` passed. CLion's bundled `clang-tidy` was found
outside `PATH` afterward, but its LLVM 23 parser fails on the Apple/JUCE
toolchain (`AudioProcessor` constructor diagnostic), so no reliable tidy
result is available. The modified curve preparation loop has no scalar
standard math.
