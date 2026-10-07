# Cycle 2 Point Dragging, Previews and Undo

## Status and starting point

Functional interaction implemented for the scoped Waveshaper point drag on
2026-10-06. The movement complexity criterion remains in progress: the Live
presentation path still copies and scans unrelated graph state. The wider mesh
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
result was available at that time. The modified curve preparation loop has no
scalar standard math.

Follow-up: Homebrew `llvm@21` provides `clang-tidy` on `PATH`. A focused run on
`CurveNodeModels.cpp` with the `tests` compilation database exits successfully;
it reports existing style diagnostics but no parser errors. The disabled
`readability-isolate-declaration` check is absent from the enabled-check list.

## 2026-10-06 Live movement complexity follow-up

A source trace after the functional commit found work that the earlier
`InteractionComplexityDiagnostics` counters do not observe. These are pending
complexity defects, not new interaction requirements:

| Live movement site | Cost tied to unrelated data | Required correction |
| --- | --- | --- |
| `GraphPresentationModel::refreshAsync`: `GraphPresentationSnapshot next = current` | Copies the execution plan, runtime trace, node previews and probe sample arrays per movement. | Share immutable plan and unchanged preview products; publish changed products without copying the whole snapshot. |
| `GraphPresentationModel::refreshConfigurations` → `GraphCompiler::refreshVoiceContexts` → editing `NodeGraph::getNodes` | Rebuilds all voice contexts and materializes a full vector of graph nodes for one Waveshaper point. | Keep unchanged voice contexts; update only the edited node's configuration. |
| `GraphAudioExecutor::prepareExecutionInternal` | Clears and prepares processors and step contexts for every compiled step. | Reuse unchanged prepared steps and update affected configuration only. |
| `NodeUpdateGraph::executeDeferredPublication` and `PresentationPreviewRenderer` | Reset plan-sized slots/masks and iterate all steps before processing affected products. | Use sparse affected-step work or generation-stamped state; retain required downstream traversal. |
| `GraphPreviewExecutor::renderPreview` and probe extraction | Build plan-sized workspaces and revisit unrelated previews/probes. | Preserve unchanged preview products and visit only affected steps/probes. |

`NodeGraph::snapshotNodeEdits` itself copies only changed overlay nodes; the
full-node materialization occurs later through `getNodes()`. The existing
`graphCopies == 0` assertion counts `NodeGraph` copy operations, not this
materialization or `GraphPresentationSnapshot` copies. The corrected proof must
scale unrelated nodes, unedited curve points and probes, and count copied
snapshot bytes, node materializations, configuration preparations, plan-step
visits and local curve rendering separately. A Live movement can scale with its
affected downstream render product, but not with disconnected graph content.

## 2026-10-06 corrective implementation design

The first full-presentation sequence test uses two point values with 17 graph
nodes, then adds 128 disconnected `Add` nodes while keeping the edited point
and downstream probes fixed. Before correction, the measured movement counts
are:

| Two Live movements | 17 nodes | 145 nodes |
| --- | ---: | ---: |
| Presentation snapshot copies | 2 | 2 |
| Overlay nodes materialized | 17 | 145 |
| Preview processor preparation step visits | 34 | 290 |
| Causal planning slot visits | 68 | 580 |
| Preview render step visits | 17 | 145 |

The authoritative graph renderer remains `GraphAudioExecutor` plus
`GraphPreviewExecutor`; the fix must reuse their processors and preview
semantics. `GraphCommandDispatcher` retains the existing edited-node worker
snapshot. The presentation layer should carry an immutable compiled plan and
unchanged preview products across movements, prepare only changed
configurations, and publish changed preview products back into the current
presentation on the message thread. Workers must own their changed
configuration and result while borrowing the stable plan until cancellation or
completion. The scheduler remains the sole stale-result gate. Local curve
rasterization remains with `FlatCurvePreparation`.

The implementation order is: remove full voice-context/node materialization
for a point-only change; give processor preparation a stable plan and changed
configuration path; make causal planning and preview rendering sparse; then
remove the full presentation snapshot copy and merge changed products at
publication. Remove the old full-copy path for this gesture rather than
retaining an alternate renderer. The operation-count test above must reach
zero unrelated-copy and unrelated-step visits at both scales, while both Live
and On Release app fixtures, rapid commit, undo/redo and save/reload continue
to pass. Baseline touched sizes: `GraphPresentationModel.cpp` 574 lines,
`GraphAudioExecutor.cpp` 1162, `NodeUpdateGraph.cpp` 518,
`GraphPreviewExecutor.cpp` 602, `PresentationPreviewRenderer.cpp` 224,
`GraphCompiler.cpp` 1535 and `NodeGraphEditing.cpp` 414. Growth in these
already-large files should be offset by moving responsibility to focused
modules, not another orchestration switchboard.

### Planner and presentation-index slice

Point-only curve publications now carry an explicit edit scope. The
presentation configuration refresh looks up the changed step by ID and keeps
unchanged voice contexts; it no longer calls editing `getNodes()` for this
gesture. `NodeUpdateGraph` clears previously touched slots and observations
only, traverses downstream nodes into a sparse list, and visits those slots
in plan order. `GraphPresentationFacts` shares stable lookup indexes between
same-structure snapshots, rebuilding probe indexes only when probe IDs may
have changed. These are local changes to the existing owners; the old full
slot reset and unconditional index rebuild paths are deleted.

| Two Live movements | 17 nodes | 145 nodes |
| --- | ---: | ---: |
| Overlay nodes materialized, after | 0 | 0 |
| Causal planning slot visits, after | 8 | 8 |
| Presentation index rebuild visits, after | 0 | 0 |
| Presentation snapshot copies, still open | 2 | 2 |
| Preview preparation step visits, still open | 34 | 290 |
| Preview render step visits, still open | 17 | 145 |

The test for this slice checks the first three properties while measuring the
remaining costs. Completion still requires the stable preview-owner and
sparse runtime/extraction work described above. In particular, the current
snapshot value copy makes every per-movement worker job own all unchanged
plan, trace, preview and probe arrays. Moving only the causal planner's work
does not satisfy the interaction complexity criterion.

Architecture review for this slice: `NodeUpdateGraph` alone decides causal
invalidation, observed-node filtering and publication; its clients supply
invalidation facts. `GraphPresentationModel` owns configuration refresh, while
`GraphPresentationFacts` owns presentation lookup indexes. No UI caller gained
those decisions. `NodeUpdateGraph.cpp` moved from 516 to 530 lines,
`GraphPresentationModel.cpp` from 570 to 589, and
`GraphPresentationFacts.cpp` from 114 to 140. Their collaborators remain the
dependency index, graph compiler/configuration factory, and presentation
snapshot respectively. The full movement-slot reset and full index rebuild
decision sites are removed. The still-open snapshot and preview-execution
sites remain single-owner runtime work, so moving them into UI glue would be
the wrong extraction.

The next slice moves observation membership into the compiled plan. The
compiler already owns probe addresses and the dependency index; it can prepare
the observed and upstream-reachable bitsets when probes or topology change.
Causal requests then share that immutable index, while direct callers that
supply ad hoc observations keep the existing local mask path. This removes a
per-movement scan and copy of all probe roots without changing which causal
products are eligible.

The compiler's probe-address and observation work now lives in
`CompiledProbeIndexBuilder`; `GraphCompiler::refreshSignalProbes` delegates to
it. The plan owns one immutable observation index, rebuilt only on compile or
probe-address changes. A Live preview request shares this index rather than
scanning every probe. Incremental extraction uses the same source-step index
to refresh affected probe products and the default output when its source is
dirty. The expanded two-movement fixture adds 128 disconnected nodes and
128 probes. Its causal planning and probe-extraction visit counts are unchanged
from the 17-node fixture; point-preview values still change downstream. This
does not yet remove plan-sized audio/result indexing or the full snapshot copy.

The extracted probe builder preserves the compiler's existing address
resolution and default-output policy; it adds only observation membership and
reverse indexes. The remaining compiler responsibilities (topology, voice
contexts, preparation) need their own later extraction; no new responsibility
was added to the large compiler file.

After this slice, `GraphCompiler.cpp` is 1,492 lines (1,535 before), with a
91-line focused probe builder. `GraphPreviewExecutor.cpp` is 640 lines (602
before), `NodeUpdateGraph.cpp` 549 (530 before), and
`PresentationPreviewRenderer.cpp` 231 (224 before). Probe-address eligibility
is decided by the compiler-owned observation index and consumed by the causal
planner and existing preview extractor. The old per-request probe-root scan
and full incremental probe replacement for stable addresses are deleted.
