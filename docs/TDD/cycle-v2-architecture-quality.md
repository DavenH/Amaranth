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
At this stage the dispatcher still captured a full `NodeGraph` for the compound
edit, so bundle rollback alone did not complete the affected-state undo
requirement or the movement preview work.

Edge topology now has an invertible affected-input delta. Connection, edge
deletion, and splice commands capture indexed before/after edge state only for
the destination inputs they can change. Compound modulation bundle connect and
delete therefore avoid the dispatcher's full-graph fallback while preserving
exact edge order through undo and redo. Tests scale the graph with 64 unrelated
nodes and assert zero `NodeGraph` copies for bundle connect/delete; direct
connection replacement and splice tests also assert zero copies and exact
undo/redo topology (91 assertions across nine focused cases). This completes
the affected-state undo requirement for edge commands. Node add/remove and
other aggregate edits may still use the explicit snapshot fallback. The three
UI movement-preview copies and graph-wide validation scans remain open.

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

Global audio boundary, reachability, neutral-scope conflict, and voice-terminal
rules now live in `GraphAudioScopeValidator`. `GraphValidator` delegates to
that cohesive rule unit for bulk and proposed-edge validation, while retaining
edge grammar, operation-input, Guide, and voice-context policy. The original
implementation fell from 700 to 524 lines; the extracted implementation is
204 lines. All 17 focused audio-scope cases pass (183 assertions), including
proposed removal, neutral partition conflicts, and global reachability. This
separates the policy that will need affected-region caching; its current
implementation still scans the complete graph.

Operation-input consistency and multi-Voice-Context assignment now live in
`GraphTopologyValidator`. It consumes the authoritative proposed-edge view and
resolved domains, so bulk validation and commit proposals retain one policy.
`GraphValidator.cpp` fell again from 524 to 394 lines; the extracted topology
validator is 160 lines. Eight operation/domain cases and the active-context
compiler rejection pass (21 assertions across nine cases). This isolates the
second graph-wide policy needed by the preview context; it still scans all
nodes and edges until affected-node indexes are introduced.

Guide assignment and heatmap-resource integrity now live in
`GraphGuideValidator`. Edge proposals cannot change these facts, so the future
preview context can retain their baseline issues without invoking Guide-domain
checks. `GraphValidator.cpp` fell from 394 to 367 lines; the extracted rule
unit is 47 lines. Focused Guide assignment, topology reconciliation, and
heatmap history tests pass (115 assertions across nine cases).

Per-edge attachment, Envelope, domain, channel-layout, pitch, and processing-
scope grammar now lives in `GraphEdgeValidator`. Bulk validation, committed
edge queries, and proposed-edge validation call this same rule unit, leaving
`GraphValidator` to resolve graph-wide facts and coordinate the cohesive
validators. `GraphValidator.cpp` fell from 367 to 92 lines; the extracted edge
validator is 287 lines. Focused edge grammar, edge-query, proposal, connection,
and splice tests pass (91 assertions across 11 cases). This is the reusable
rule unit needed by an incremental preview context; movement-time graph copies
and graph-wide domain, scope, and topology analysis remain open.

Validation codes and issue values now live in `GraphValidationTypes.h`.
`GraphEditTypes` and the four extracted validator implementations no longer
import the `GraphValidator` orchestration facade merely to exchange results.
The mutating `GraphEditor` now declares its actual facade dependency in its
implementation. Seven focused edge, proposal, connection, and splice cases
pass (82 assertions). This leaves the rule units composable for the indexed
preview context without reversing their dependency toward its coordinator.

Connection proposal and validation now live in the read-only
`GraphConnectionValidator`. It owns port orientation and lookup, edge metadata,
destination replacement, full rule evaluation, and strict-repair acceptance.
`GraphEditor` applies the accepted edge, while splice reuses the same proposal
construction and destination lookup. `GraphEditor.cpp` fell from 410 to 306
lines and its header from 31 to 30 lines; the new service is 115 lines. Eight
focused connection, splice, proposal, and copy-count cases pass (50
assertions). The three UI preview callers remain on their existing clone-based
path until an indexed gesture context can call this boundary without graph-wide
movement work.

Splice search and its two-stage proposal validation now live in the read-only
`GraphSpliceValidator`. It composes `GraphConnectionValidator` and the shared
graph validators, then returns the two accepted edges for `GraphEditor` to
apply. `GraphEditor.cpp` fell again from 306 to 239 lines; the splice validator
is 97 lines. Five focused connection, splice, and copy-count cases pass (36
assertions). Commit mutation and validation policy are now separate for both
connection forms; indexed preview evaluation remains the next prerequisite
before replacing the UI clone paths.

`InteractionComplexityDiagnostics` now measures validation node visits,
validation edge visits, and domain-transfer work. The domain resolver, audio
scope analyzer, edge validator, topology validator, and audio-scope validator
record work at their authoritative traversal boundaries. These counters do
not change validation behavior; they provide the scale assertions required for
the indexed context and prevent a graph-copy removal from concealing complete
graph scans. Six focused proposal, connection, splice, and copy-count cases
pass (46 assertions).

`GraphEdgeIndex` now owns destination-input and per-node incoming/outgoing edge
indexes for a stable edge view. Connection validation uses it for replacement
lookup, and splice validation builds one index for both proposal stages instead
of rescanning the complete edge vector for each input and output candidate. A
scaled test grows unrelated nodes, edges, and audio data while asserting that
indexed input and adjacency queries perform zero validation edge visits, graph
copies, or audio-sample copies (12 assertions across two scales). The index is
the first retained component of the gesture validation context; resolved
domains, audio scope, and affected-closure invalidation remain open.

`GraphValidationContext` now binds one durable graph revision to its borrowed
edge view, edge index, resolved domains and channel layouts, audio-scope
analysis, and baseline validation issues. `GraphValidator` accepts those
precomputed facts, so the context uses the authoritative orchestration without
repeating domain or scope analysis. The context rejects a changed graph
revision, and cached fact/index reads record zero validation visits or domain
transfers. Context parity, invalidation, proposed-edge parity, and scaled index
tests pass (31 assertions across three cases). Proposal evaluation still needs
affected-closure updates before this context can enter live UI movement paths.

`GraphEdgeIndexOverlay` now projects a proposed `GraphEdgeView` over the stable
baseline index. It translates retained edge indices, filters the small removed
set, and indexes only added edges while borrowing all unchanged adjacency.
Input, incoming, and outgoing queries match a fully rebuilt proposed-edge
index at both zero and 128 unrelated edges, with zero validation edge visits
after overlay construction (20 assertions across two scales). This supplies
the local adjacency needed by incremental domain and scope worklists without
copying or rescanning the base graph.

`GraphDomainResolver` now consumes `GraphEdgeIndex` for input and node
adjacency and uses `NodeGraph::findNode` for node lookup. Its private node map
and incoming/outgoing edge tables were deleted, leaving one indexing policy
for full and future incremental resolution. The resolver fell from 377 to 339
lines. Nine focused propagation, invalid-cycle, proposed-edge, operation, and
channel-layout cases pass (31 assertions). The next domain slice can seed the
existing worklist from `GraphEdgeIndexOverlay` without reproducing transfer
rules or adjacency construction.

Seeded domain recomputation now initializes retained proposed edges from the
durable `GraphValidationContext` resolution, expands the local downstream
dependency closure from destinations changed by `GraphEdgeIndexOverlay`, and
runs the existing domain and channel-layout transfer functions only over that
closure. Removed and added edges share one structural seed calculation;
propagation retains the authoritative resolver rules and overlay adjacency.
`GraphDomainResolver.cpp` grew from 339 to 426 lines,
`GraphEdgeIndex.cpp` from 131 to 165 lines, and `GraphEdgeView.h` from 101 to
104 lines. The new code removes no caller yet because incremental audio-scope
and validation facts remain prerequisites for the live preview path. Full and
seeded replacement/removal parity tests pass, and a scale test holds the
affected branch constant while adding 128 disconnected branches: domain
transfers remain unchanged with zero validation node visits, validation edge
visits, or graph copies. The focused domain set passes 29 assertions across
six cases and the complete complexity set passes 605 assertions across 31
cases. The broader `[graph]` run retains six pre-existing serializer/preset
fixture failures; its output is in
`/private/tmp/cycle-v2-graph-seeded-domain.log`. Incremental audio-scope
analysis is the next indexed fact slice.

Seeded audio-scope analysis now starts from signal-edge endpoints changed by
`GraphEdgeIndexOverlay`, walks only the affected domain-neutral components,
and replaces their inherited scope and conflict facts. Full and incremental
analysis share the authoritative capability, explicit-scope, and component
resolution rules; deterministic conflict ordering makes their results directly
comparable. `GraphAudioScope.cpp` grew from 155 to 306 lines and its header
from 39 to 45 lines. The source remains one cohesive policy owner: full graph
component discovery, proposed-view component discovery, and their shared scope
classification. A replacement/removal parity test and a scaled test pass; the
scaled case adds 128 disconnected branches while holding validation node and
edge visits constant. The focused audio-scope set passes 144 assertions across
14 cases, and the complete complexity set passes 623 assertions across 32
cases. Result materialization still copies the baseline scope map and conflict
list, so a persistent or layered fact representation remains a prerequisite
before claiming total proposal cost independent of graph size. Incremental
affected-closure validation is the next indexed fact slice.

Affected-closure proposal validation now reuses the durable issues in
`GraphValidationContext`, consumes the affected edge and node sets produced by
the seeded domain and audio-scope analyses, and revalidates only changed or
fact-dependent edges and affected operation nodes. Validation issues now carry
a stable `subjectId` for node-owned policies, so an operation issue can be
invalidated without parsing its message or discarding unrelated baseline
issues. Full and proposal topology validation share one indexed operation-node
rule. `GraphValidator.cpp` grew from 96 to 226 lines,
`GraphTopologyValidator.cpp` from 113 to 201 lines, and
`GraphValidationContext.cpp` from 23 to 47 lines; each remains below the size
review thresholds and retains one policy level. A replacement test proves that
a repaired mixed-domain operation removes its baseline issue with full-validator
parity. A scale test adds 128 disconnected nodes while validation node visits,
edge visits, and domain transfers remain constant. The complete complexity set
passes 643 assertions across 33 cases. The broader `[graph]` run passes 183 of
189 cases and retains the same six serializer/preset fixture failures; output
is in `/private/tmp/cycle-v2-graph-affected-validation.log`. Explicit audio
graphs and Voice Context
assignment edits deliberately retain the full validation fallback until the
context owns indexed reachability, terminal-output, and assignment facts.
Baseline scope-map and issue-vector materialization also remain graph-sized.
Those retained global policy facts are the next validation slice before live UI
callers adopt the context.

Voice Context assignment policy now has one read-only owner,
`GraphVoiceContextAssignments`. It identifies providers, accepting nodes, and
explicit assignments once; `GraphCompiler` translates those facts into
single-context implicit edges, while `GraphTopologyValidator` translates them
into missing-assignment and multiple-active-context issues. The duplicated node
predicate and graph/edge scans were deleted. `GraphCompiler.cpp` fell from
1,563 to 1,522 lines and `GraphTopologyValidator.cpp` from 201 to 185 lines;
the new focused implementation is 82 lines with a 36-line interface.
`GraphValidationContext` retains the analysis, and a proposed context edge now
updates assignment facts and issues without taking the former full validation
fallback. Three compiler integration cases pass 20 assertions, and three
validation-context cases pass 17 assertions. Incremental explicit-audio
reachability and terminal-output facts remain the final global validation
fallback before caller adoption.

#### Explicit-audio proposal validation design

The authoritative behavior remains `GraphAudioScopeValidator`: singleton audio
boundaries, neutral-scope conflicts, directed reachability from Global Input,
reverse reachability to Output, and ambiguous linked-stereo voice terminals.
The retained validation context will add one audio-validation fact set with the
boundary IDs, forward- and reverse-reachable node sets, and each voice node's
count of linked-stereo outputs that bypass Voice Output.

For a proposed edge view, forward reachability can change only at each changed
edge destination, at nodes whose resolved scope changed, and downstream from
those seeds. Reverse reachability can change only at each changed edge source,
at scope-changed nodes, and upstream from those seeds. Recompute each affected
directed closure over `GraphEdgeIndexOverlay`; initialize it from the retained
reachability of unchanged boundary predecessors or successors, then propagate
inside the closure. This preserves alternate paths after removal and handles a
cycle by recomputing every affected member reachable within the proposed view.
Do not restart either traversal from Global Input or Output.

Voice-terminal status can change for changed-edge sources, scope-changed nodes,
and sources of edges entering a scope-changed node. Recompute linked-stereo
terminal counts only for those nodes through overlay outgoing adjacency, update
the retained total, and emit the single ambiguity issue from that total.
Neutral-scope conflict issues use the affected node IDs already returned by
`GraphAudioScopeAnalysis`. Singleton boundary issues are unchanged because an
edge proposal cannot add or remove nodes.

Proof requires full-validator parity for edge addition, replacement, removal,
alternate-path retention, a directed cycle, a neutral node changing scope, and
voice-terminal consumption. Operation counters must remain unchanged when 128
disconnected global and voice branches are added while the edited closure stays
fixed. Once these facts replace the explicit-audio fallback, connection and
splice gesture contexts may adopt proposal validation and delete the two UI
`NodeGraph` candidate copies.

The first explicit-audio slice extracts `GraphAudioValidationFacts` as the one
owner of boundary discovery, indexed forward/reverse reachability, per-node
voice-terminal counts, and issue materialization. `GraphAudioScopeValidator`
is now a 30-line adapter instead of a 219-line mixed analysis/validation file;
the focused fact implementation is 197 lines with a 46-line interface. The
authoritative behavior is unchanged: all 14 audio-scope cases pass 144
assertions, proposed/committed validation parity passes eight assertions, and
the complete complexity set passes 643 assertions across 33 cases. The next
slice will add the proposed-view constructor described above and retain these
facts in `GraphValidationContext`.

Incremental explicit-audio facts now copy the retained boundary, membership,
reachability, and terminal-count facts, then replace only the forward and
reverse affected closures and changed terminal sources through
`GraphEdgeIndexOverlay`. `GraphValidationContext` retains the baseline facts,
and proposal validation rematerializes audio-policy issues from the proposed
facts; the former full-validation fallback and its duplicate explicit-audio
predicate were deleted. Full-validator parity covers removal with and without
an alternate path, a directed cycle, neutral-node scope migration, and voice
terminal consumption. A scale test adds 128 disconnected branches while
validation visits and graph/audio copies remain unchanged. The focused
validation-context set passes 22 assertions across four cases, and the complete
complexity set passes 655 assertions across 34 cases. During that proof, the
larger affected worklist exposed a borrowed vector element that could be
invalidated by worklist growth; `GraphAudioScope` now copies the current node
ID before appending successors. `GraphAudioValidationFacts.cpp` grew from 197
to 400 lines and its interface from 46 to 53 lines. This is one cohesive fact
owner for full construction, proposed closure replacement, and issue
materialization; it remains below the source review threshold. The explicit
audio graph now has no graph-wide proposal fallback. Live connection and
splice callers can adopt the retained context and delete their UI candidate
copies next.

Live connection, modulation-bundle, and splice previews now retain one
`GraphValidationContext` for the gesture and call the same read-only connection
and splice validators used by commit. The three UI `NodeGraph` candidate copies
and all UI imports of `GraphEditor` were deleted. Node drags may change graph
revision through bounds updates while leaving topology and validation facts
intact, so the context exposes an explicit layout-only proposal path; other
revision changes retain the existing invalidation behavior. Scale tests add
128 disconnected nodes and 16,384 unrelated audio samples to connection,
bundle, and splice previews. Movement-time node visits, edge visits, and domain
transfers remain unchanged, with zero graph and audio-sample copies. The
gesture complexity set passes 389 assertions across 12 cases. Production file
sizes after adoption are 110 lines for `GraphConnectionValidator.cpp`, 122 for
`GraphSpliceValidator.cpp`, 78 for `GraphValidationContext.cpp`, 237 for
`ModulationCableBundle.cpp`, 373 for `NodeCanvasInteraction.cpp`, and 238 for
`NodeCanvasHitRouter.cpp`. `NodeCanvas.cpp` grew from 2,594 to 2,603 lines only
to capture and pass the gesture context; its UI coordination extraction plan
remains slice 2. This completes shared preview/commit rule adoption and the UI
copy deletion target. Narrowing the graph aggregate remains open in this slice.

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

Automation registry slice: command names and compatibility aliases now map to
one typed `CycleV2AutomationCommand` registry instead of being interleaved with
handler invocation in a 46-branch string chain. `runCommand` performs typed
dispatch, while protocol aliases such as `connect`, `openMeshPopup`, and
`removeGuideCurve` have one owner. The registry contract passes five focused
assertions. `CycleV2Automation.cpp` fell from 2,030 to 1,995 lines; the registry
is 70 lines with a 61-line interface. Transport, assertions, pointer input, and
domain handlers still share the original class, so their extraction remains
open.

Assertion slice: JSON result construction, property reads, path traversal, path
flattening, and comparison semantics now live in
`CycleV2AutomationProtocol`; `CycleV2AutomationAssertions` composes that
protocol with snapshot and parameter-reader callbacks. The state, node
parameter, and assertion-path handlers were deleted from the transport class.
`CycleV2Automation.cpp` fell from 1,995 to 1,742 lines. The assertion service is
110 lines with a 30-line interface, and the shared protocol is 176 lines with a
41-line interface. Focused assertion behavior passes three checks. Pointer
input, protocol transport, and UI-facing domain handler extraction remain
open.

Input slice: keyboard translation, pointer targeting, JUCE event construction,
cursor reporting, and performance-keyboard automation now live in
`CycleV2AutomationInput`. The input service receives four semantic expanded
editor actions as callbacks, so it translates protocol input without owning
the corresponding graph edits. Shared rectangle and cursor encoding moved to
`CycleV2AutomationProtocol`; the duplicate helpers were deleted from the
orchestrator. `CycleV2Automation.cpp` fell from 1,742 to 1,293 lines; the input
implementation is 443 lines and its interface is 37 lines. The Cycle V2 app
and test targets build, and a live pointer fixture successfully dispatched its
double-click and wheel commands. Its two state assertions remain stale because
their fixed canvas coordinate no longer expands `waveMesh`; the command results
and final snapshot show that input dispatch completed. Protocol transport,
UI-facing domain handlers, and `NodeCanvas` gesture/editor ownership remain
open.

Workspace-command slice: graph edits, Guide edits, node parameters, expanded
editor controls, and their protocol validation now live in
`CycleV2AutomationWorkspaceCommands`. It receives the workspace plus snapshot
and path callbacks and delegates semantic edits to the workspace's existing
automation boundary. The 18 old handler implementations were deleted from the
orchestrator; palette invocation and pointer semantic targets call the same
service. `CycleV2Automation.cpp` fell from 1,293 to 1,036 lines; the new
implementation is 287 lines with a 46-line interface. The app and test targets
build, and a live fixture successfully inspected and opened `waveMesh` through
the service. Session transport and `NodeCanvas` gesture/editor ownership remain
open.

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

Execution-mode contract slice: `GraphAudioExecutor::processInternal` now takes
one explicit variant selecting complete diagnostics, incremental diagnostics,
or realtime execution. Incremental dirty state, cancellation, and result
capture travel together; realtime pass, observer, and operation counters travel
together. The former twelve-argument mixture of nullable policy controls was
deleted. The implementation remains allocation-free for realtime calls: mode
inspection uses the caller's stack value and the cancellation callback remains
borrowed. Complete rendering passes 18 assertions, incremental rendering passes
25 assertions across three cases, realtime ownership-pass coverage passes three
assertions, and the realtime-tagged set passes 144 assertions across 14 cases.
`GraphAudioExecutor.cpp` is 1,215 lines and its header is 308 lines after the
contract change; preparation/cache extraction and the shared oscillator region
planner remain open.

Oscillator planning slice: `OscillatorRegionPlanView` now owns structural
region validation, step membership, port lookup, and the decision that an input
originates inside a region. Both chained and shared-spectral renderers use that
view while retaining their distinct strategy and supported-role rules. The
duplicated membership vectors and input traversal helpers were deleted.
`SpectralOscillatorFrameRenderer.cpp` fell from 828 to 799 lines and
`ChainedOscillatorRecipeRenderer.cpp` from 377 to 349 lines; the shared view is
48 lines with a 29-line interface. Its boundary test passes seven assertions,
and the spectral unresolved-control fallback passes six assertions. The broad
oscillator set still contains the existing missing preset fixture failures.
Preparation and processor-cache ownership remain the final runtime extraction
target.

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
