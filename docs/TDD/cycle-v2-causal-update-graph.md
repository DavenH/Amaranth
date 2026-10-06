# Cycle 2 Point Dragging, Previews and Undo

## Status and starting point

Planned; no new implementation or performance measurements completed.
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
