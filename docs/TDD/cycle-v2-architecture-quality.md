# Cycle V2 Architecture Quality Review

## Status

In progress, 2026-09-18. Graph edit ownership and the Trimesh dependency
boundaries have completed extraction slices. The UI validation, `NodeGraph`
ownership, UI coordination, and runtime execution criteria remain open.

## Context and baseline

The merged causal-update work extracted `PresentationRefreshScheduler`,
`PresentationPreviewRenderer`, `PresentationUpdateRequestBuilder`, and more
gesture ownership. `GraphPresentationModel.cpp` is now 542 lines. The causal
update TDD remains **in progress**: caller-side refresh choices and the broad
editor refresh API still exist. Finish those deletion targets in
[`cycle-v2-causal-update-graph.md`](cycle-v2-causal-update-graph.md), rather than
implementing a second policy here.

Current physical line counts (`wc -l`, 2026-09-18):

| Source | Lines | Main concern |
| --- | ---: | --- |
| `UI/NodeCanvas.cpp` | 2,572 | Canvas, gestures, document, editor host, automation, and refresh coordination |
| `App/CycleV2Automation.cpp` | 2,012 | Protocol, routing, UI actions, inspection, and assertions |
| `Graph/GraphCompiler.cpp` | 1,559 | Size trigger; inspect cohesion before changing it |
| `Graph/GraphSerializer.cpp` | 1,217 | Size trigger; inspect cohesion before changing it |
| `Runtime/GraphAudioExecutor.cpp` | 1,207 | Processing modes, preparation, cache, diagnostics, and voice execution |
| `Runtime/SpectralOscillatorFrameRenderer.cpp` | 828 | Source operations, transforms, combining, and output |
| `Graph/NodeGraph.h` | 479 | Topology, resources, probes, guides, indexes, and overlays in one aggregate |

`python3 scripts/cycle_v2_architecture_audit.py` reports 22 size-triggered
files among 430 Cycle V2 C++ sources and headers at this baseline. The table
lists reviewed concerns plus the largest unreviewed triggers; it is not a
finding against every flagged file.

File size is evidence to inspect, not a mandate to create one file per method.
The compiler and serializer are listed as triggers, not yet diagnosed as failed
abstractions. Count public roles, duplicated decisions, and dependency direction
before proposing their extraction.

## Ownership and boundaries

- `GraphCommandDispatcher` is the authoritative semantic mutation and durable
  gesture boundary; `GraphEditor` provides lower-level graph edits. Commands
  must return consolidated `GraphChangeSet` facts so wrappers do not infer
  invalidation policy independently. Keep UI callers outside direct graph
  mutation. `GraphTransaction` currently has test callers only; decide whether
  to delete it or make it the production transaction primitive before further
  extension.
- `NodeUpdateGraph` owns causal dependency planning. The shared presentation
  session and scheduler own gesture and publication lifecycle. Domain editors
  supply semantic deltas and local product inputs. See the causal-update TDD
  for its remaining migration and deletion work.
- Existing Cycle 1 DSP, Trimesh rasterization, and node-domain edit rules remain
  authoritative. Extract ownership around them; do not copy their behavior.

## Refactor slices

### 1. Narrow graph mutation and graph aggregate ownership

`GraphCommandDispatcher.cpp` annotates success and assigns topology, layout,
and probe change flags per wrapper. `GraphTransaction.cpp` separately clones a
candidate graph and maintains another change accumulator; it has no production
caller. `NodeGraph.h` holds topology, Guide curves/heatmaps/assignments, probes,
audio resources/bindings, their indexes, and transient overlay views.

Design a small semantic result contract per command family, then centralize
change-set merge and publication in the dispatcher. Separate domain stores
behind `NodeGraph` only where doing so removes coupled invariants or copies;
preserve its stable read API while migrating. Remove the unused transaction
abstraction unless it becomes the single production owner. Replace UI
speculative `GraphEditor` calls with a read-only connection/splice validation
service that reuses the same domain rules.

Completion evidence: one production transaction owner; no UI `GraphEditor`
calls; no second change-flag accumulator; operation-count tests show unrelated
resources are not copied for local gestures; the graph's public mutation
surface and index ownership are materially narrower.

First slice: removed the test-only `GraphTransaction` implementation and
retargeted its two semantic tests to the production dispatcher compound edit.
Moved topology, probe, and Guide change facts from dispatcher wrappers into
`GraphEditor` results. `GraphCurveStateCommands` now retains the Guide model
result rather than overwriting its change flags. `GraphCommandDispatcher.cpp`
fell from 588 to 484 lines; `GraphEditor.cpp` rose from 968 to 1,035 lines.
This is a policy ownership improvement, not completion of this slice: the
UI speculative validation still calls `GraphEditor` directly, and `NodeGraph`
still owns all collections and indexes. Focused
topology, Guide, probe, and compound transaction tests pass. The broad `[graph]`
run has eight serializer/preset-fixture failures outside the changed command
paths; output is in `/private/tmp/cycle-v2-graph-tests.log`.

Second slice: moved the eleven Guide resource and attachment edit operations,
including their result facts, unchanged into `Nodes/Guide/GuideGraphEditor`.
Production callers and direct tests use that domain service; the old
`GraphEditor` Guide surface and implementation were deleted. `GraphEditor.cpp`
fell from 1,035 to 766 lines and its header from 153 to 119 lines; the new
Guide implementation is 279 lines. Focused Guide assignment, resource, and
heatmap tests pass (103 assertions across eight cases). The Guide service
continues to use the graph's authoritative resource and assignment methods.

Third slice: moved seven node parameter, model, editor-state, and audio-resource
edit operations into `GraphNodeStateEditor`, preserving their existing
validation and `GraphEditResult` behavior. The dispatcher and direct tests use
the new service; the old methods were deleted. `GraphEditor.cpp` fell from 766
to 345 lines, its header from 119 to 88 lines, and the new implementation is
430 lines. Focused parameter, resource, Trimesh reconciliation, and transaction
tests pass (73 assertions across seven cases). The architecture audit reports
21 size triggers among 432 Cycle V2 C++ files. This is composition of the
editors, not a change to the graph aggregate or UI preview path.

UI connection/splice preview remains open. Today the UI copies `NodeGraph`
and invokes `GraphEditor`, while `GraphEditor::connect` copies it again for
whole-graph validation. A facade around that path would hide the dependency
without removing the movement-time graph copies. Extract a read-only graph
validation view for proposed edge deltas, shared by preview and commit, before
replacing these callers; retain `GraphValidator`'s existing domain and scope
rules rather than adding a second approximation.

The read view must cover more than `GraphValidator::validateEdge`:
`GraphDomainResolver` propagates domains and channel layouts across edges,
`GraphAudioScopeAnalyzer` derives processing scope, and `GraphValidator`
checks global reachability, operation inputs, and voice assignments. These
currently consume `NodeGraph`'s complete node/edge vectors. A preview adapter
that merely presents a modified vector still scans unrelated graph data on
each movement. Cache the unchanged baseline at gesture start and make the
shared rule units query the affected edge neighborhood and its downstream
dependencies. The completion test must scale unrelated nodes, edges, and
audio resources while holding the proposed edge delta constant, then assert
unchanged copy, lookup, and validation-work counters. Connection and splice
commit must use the same rules with a durable graph mutation only at commit.

Contract prerequisite: moved `PortAddress`, `GraphEditResult`,
`GraphChangeSet`, and related edit data types from `GraphEditor.h` into
`GraphEditTypes.h`. Fourteen production headers now import that contract
without importing the mutating editor class; the four production `.cpp` files
that still construct `GraphEditor` include it explicitly. `GraphEditor.h`
fell from 88 to 31 lines. Connection and hit-routing tests plus dispatcher
transaction tests pass (51 assertions across four cases). This removes a
header dependency; the three UI preview calls and their graph copies remain
and still require the shared read view above.

Read-view extraction, first internal slice: `GraphEdgeView` borrows the graph's
existing edge vector and overlays only removed indices and added edges. It
translates `edge count/index` reads; it owns no graph nodes, resources, or
validation decisions. `GraphDomainResolver` remains authoritative for domain
and channel propagation and now accepts this view alongside its existing
`NodeGraph` entry point. The stable end state is for preview and commit to
pass the same proposed-edge view into domain, scope, and validation rules,
then retire clone-based candidate validation. The overlay must never become
a second domain resolver. A parity test compares a proposed replacement with
the equivalent committed graph while checking that the source graph is intact.
`GraphAudioScopeAnalyzer` and `GraphValidator` now consume the same view,
including graph-level scope, reachability, operation, and voice-assignment
checks. `GraphEditor::connect` validates the proposal through that view and
applies its edge replacement only after acceptance, removing its whole-graph
candidate copy. Proposed-vs-committed domain, scope, and validation parity
tests plus a scaled connection copy-count test pass; the focused domain,
audio-scope, connection, splice, and canvas tests pass (209 assertions across
25 cases). A following slice moved splice's two candidate connections through
the same edge-construction method and proposed-edge validation order, then
applies the two accepted edges in place. Splice no longer clones the graph;
focused splice and hit-routing tests pass (46 assertions across five cases),
including a scaled unrelated-graph/audio copy-count test. The validator still
scans complete graph vectors, and the three UI preview callers still copy
`NodeGraph`; no movement path may use this view until those costs are removed
or cached.

The architecture audit reports 21 size triggers among 438 Cycle V2 C++ files
after these graph-view slices. `GraphEditor.cpp` is 410 lines after the splice
change, below the review threshold; size reduction is secondary to the
removed candidate copies and shared validation rules.

Bundle commit no longer performs a separate clone-based preflight. It now
attempts routes through `GraphCommandDispatcher` inside one compound edit and
cancels on the first rejection. A test rejects the second route after the first
succeeds and verifies that edges, revision, and undo history remain unchanged.
The dispatcher still captures a full `NodeGraph` for the compound edit, so this
does not complete the affected-state undo requirement or the movement preview
work. Give edge topology an invertible delta before claiming that boundary is
resolved.

The preview index must retain the authoritative rule owners. Cache the durable
graph's node/port addresses, input/output adjacency, resolved edge domains and
layouts, neutral audio-scope components, singleton/context facts, reachable
global regions, and baseline validation issues. Apply each proposed edge delta
to those indexes and recompute only its dependency closure. Connection and
splice acceptance must use `GraphValidator::validateEdge` and the same domain,
scope, operation-input, and reachability rules as commit, including the current
strict-repair allowance for pre-existing issues. Edge removal may split a
neutral component or global path, so the affected closure is wider than the
new edges alone. Invalidate the context on durable graph changes and on node
parameter changes that alter ports, domains, or processing scope. Compare the
indexed proposal with committed full validation on both valid and invalid
graphs; scale disconnected graph data and assert unchanged validation visits.

As a prerequisite, `GraphValidator::validateEdge` now uses the indexed
`NodeGraph::findNode` instead of its own per-edge linear node search. Focused
edge-grammar, proposed-edge, and neutral-scope tests pass (20 assertions in
three cases). This removes one repeated lookup policy but does not bound the
remaining graph-wide validation passes.

The commit path's strict-repair comparison now belongs to `GraphValidator` as
`acceptsProposedIssues`; connection and both splice proposal steps call it.
Preview can reuse this exact issue-identity policy once its incremental facts
are ready. A focused contract test covers empty proposals, strict issue
reduction, unchanged issue counts, and a changed edge address; four connection
and splice tests retain 24 assertions. This centralizes acceptance policy but
does not remove movement-time graph copies or validation scans.

### 2. Reduce UI coordination surfaces

`NodeCanvas` inherits component, OpenGL, timer, editor presentation/resources,
expanded-editor delegate, and invalidation roles. It also owns document,
commands, presentation, automation, rendering, dock, and per-domain gestures.
Move remaining domain gesture state and editor lifecycle behind their existing
collaborators, and narrow the host interfaces to the actions each client uses.
`CycleV2Automation::runCommand` is a single conditional router over unrelated
command families. Split protocol/transport, command registry, assertions, and
UI-facing domain handlers while retaining one external automation schema.

Completion evidence: a new gesture or automation command no longer requires
adding state and dispatch branches to both central classes; production sizes
fall for the original files; existing focused UI automation fixtures retain
their observable behavior. The causal TDD owns removal of refresh host calls.

### 3. Separate runtime execution policies

`GraphAudioExecutor::processInternal` takes diagnostics, observer, dirty-node,
incremental-result, pass, and traversal controls together. Extract preparation
and cache ownership, execution-mode orchestration, and diagnostic capture so
the realtime path has a narrow prepared contract. Preserve processor identity,
buffer ownership, and no-allocation audio-thread behavior. The existing
realtime-storage refactor note remains a separate prerequisite where relevant.

`SpectralOscillatorFrameRenderer` combines source rendering, FFT/IFFT,
operation execution, and final output in one frame method. Compare its
`supports`/`prepare` graph-role policy with
`ChainedOscillatorRecipeRenderer`; move only truly shared eligibility and
planning into one region planner. Keep distinct renderer behavior distinct.

Completion evidence: execution modes have explicit inputs rather than
nullable policy arguments; no duplicated region eligibility decisions;
operation-count and audio-parity tests preserve preparation, rendering,
capture, and realtime allocation contracts. Original orchestration files
shrink as behavior moves to cohesive owners.

### 4. Correct node-domain dependency direction

`TrimeshNodeModel::renderGrid` constructs blockwise/gridwise DSP processors and
applies `TrimeshRenderProfile`. Move this presentation work into a Trimesh
service that reads model state and delegates to the existing DSP. `GraphValidator`
imports `TrimeshGuideAttachmentTarget` from `Nodes/Trimesh/Editor`, although
that helper now handles Envelope targets. Put the shared target contract in
the Guide domain and keep family-specific selection lookup with each editor.

Completion evidence: model code has no presentation/DSP construction;
graph validation has no editor include; Trimesh and Envelope guide attachment
semantics and render products remain covered by focused tests.

First boundary slice: moved shared Guide target validity, including the field
limit, into `Nodes/Guide/GuideAttachmentTarget`. `GraphValidator` now depends
on that domain contract instead of a Trimesh editor header. The Trimesh editor
keeps vertex-to-cube selection lookup and calls the shared validity rule for
Envelope selections; the duplicate validity method was deleted. Focused
Trimesh assignment/selection/validation tests pass (40 assertions across five
cases), as does the authored Envelope Guide playback test (17 assertions).
Second boundary slice: moved `TrimeshNodeModel::renderGrid` and its presentation
result type into `TrimeshGridRenderService`, with the panel data source calling
that service. It uses the same blockwise/gridwise DSP and render profile logic
without copying the algorithm. `TrimeshNodeModel.cpp` fell from 549 to 459
lines; the new service is 100 lines. The model no longer imports rendering or
DSP headers. Seven focused Trimesh grid, panel, and spectral presentation
tests pass (1,463 assertions). This completes the two dependency boundaries
named in this slice; the broader architecture TDD remains in progress.

## Measurement and exit criteria

For each slice, record `wc -l` for touched production files, the number of
callers that decide its policy, public methods/roles removed, dependencies
removed, and the semantic tests or operation counters used. A smaller source
file alone is insufficient: the old path and duplicate decisions must be
deleted. Re-run the repository's architecture review triggers after each slice
and update this TDD until every slice is implemented or explicitly superseded.
