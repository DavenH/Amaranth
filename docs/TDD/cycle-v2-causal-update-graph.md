# Cycle V2 Causal Update Graph

## Status

In progress (reopened 2026-09-15).

### 2026-09-17 impact-domain baseline

The native Cycle V2 Live mod-wheel fixture on `honerism-3.cyclegraph`
measured the remaining preview-publication domain. After the final drag
settled, `previewRenderCount` was 3; after release it was 4. The window
contained three preview requests, two publications, one cancelled result,
and zero synchronous refreshes. The release advanced the audio-plan copy
count from 2 to 3. These are baseline observations, not a target to suppress:
release also commits saved morph fields, so product reuse needs an effective
configuration comparison or transient morph updates. Re-run the same fixture
after the session and scheduler migration and compare these counters.

The pending-identity correction in `PresentationGestureSession` leaves this
wheel-domain telemetry unchanged: the same fixture still reports 3 to 4
renders, 2 to 3 audio-plan copies, three requests, two publications, one
cancelled result, and zero synchronous refreshes. The correction preserves
independent source identities; it does not migrate wheel scheduling.

### 2026-09-18 Live morph semantic decision

The user chose transient saved Trimesh/Envelope morph updates on each Live
mod-wheel movement. `GraphCommandDispatcher` remains the authoritative owner
of those edits and of the single durable commit/undo transaction. The gesture
must retain one durable base revision and present a worker-safe immutable
overlay/delta without copying the complete graph on movement. The final
published movement can be reused at commit only when its effective product
fingerprint includes the transient morph fields and matches the committed
configuration. Test a two-movement sequence, downstream effect, commit,
and undo before deleting the old wheel path.

The focused Live wheel fixture now measures 3 preview renders after the final
movement and still 3 after release, versus 3 to 4 before this slice. Its
audio-plan copy count still advances from 2 to 3 on commit; three requests,
two publications, one cancelled result, and zero synchronous refreshes are
unchanged. The fixture now asserts these counts and durable dirty-state
transition. The dispatcher test covers two transient movements, no-op repeat,
commit, undo, immutable movement snapshots, and zero full graph/mesh copies or
model serializations at both unrelated graph scales. This closes the Live
wheel duplicate-preview symptom but not the shared session/scheduler deletion
targets or the broader native editor proof.

The next extraction moves the wheel's active/mode/changed/base-revision and
immutable snapshot lifecycle into `PresentationGestureSession`. The session
calls the dispatcher to begin/commit/cancel a transient edit and the pure
refresh policy to determine whether a graph snapshot is needed. The wheel
control supplies only its normalized value and domain morph command; the
session does not implement morph normalization, product rendering, or worker
publication. The end state deletes the wheel gesture fields from `NodeCanvas`
and allows another continuous domain edit to use the same lifecycle methods.

The session extraction is in production for the wheel. `NodeCanvas` no longer
stores its active/mode/changed/base-revision or graph-snapshot fields; it
submits normalized wheel values and the existing morph command. Live and On
Release fixtures pass with 3/2/1 and 1/1/0 requested/published/cancelled
preview jobs respectively. The Live render count stays 3 across release and
the durable audio-plan copy still advances once. Other editor families still
own separate scheduling, so the shared-session completion criterion remains
open.

### Ordinary and paired parameter gesture migration

`GraphCommandDispatcher::setNodeParameter` remains the authoritative
normalizer and transient delta publisher. The shared session now owns the
transaction for ordinary and paired parameters, with one durable commit and
undo. On Release movement performs only the editor's local product; Live
movement snapshots only the affected overlay nodes and queues preview work
through the existing latest-only worker. `NodeCanvas` applies the pure policy
decision and submits the final committed change; `NodeEditorCommandService`
no longer selects Live/On Release refresh behavior for these gestures. The
old schedule/flush/immediate host API remains for other editor families and
must still be deleted as those families migrate.

The On Release Reverb spectrogram fixture demonstrates that some current
"local" editor products still depend on the graph preview runtime. Movement
retains the existing deferred local refresh path while suppressing probe and
durable audio publication. The shared session does not copy the graph at
pointer-down in this mode. The old local path still does graph work during
movement and does not satisfy the strict no-traversal/no-configuration-
preparation contract. Extract the mature Reverb local renderer/product input
from graph traversal before claiming that completion criterion; do not weaken
the fixture to hide it.

The focused Reverb UI fixtures now pass under both policies. Live records four
requests, two publications, one superseded-before-start job, one stale result,
and zero synchronous refreshes for the measured drag/commit window; its
preview render count remains 3 across release while the audio-plan copy
advances from 2 to 3. On Release records five requests for local editor work
and commit/undo in its wider window, with one publication and zero synchronous
refreshes. Its pre-release snapshots add CompactPreview publications while
ProbePreview and PreviewTraversal publication counts remain unchanged; those
products advance only after release. The pre-migration Reverb fixture failed
its during-drag spectrogram assertion when local work was briefly omitted,
which is why this local product remains an explicit extraction target.

### Trimesh morph gesture migration

Trimesh morph now uses the shared session for its transient edit, movement
identities, durable commit, and undo. The domain service supplies whether the
edited axis is the primary local slice axis. That local gesture does not
capture a full graph even under Live policy; it publishes durable local state
once on release. A non-primary axis retains Live preview feedback through the
session's one stable graph snapshot. The scale test covers a primary gesture
across 0 and 128 unrelated nodes and 16,384 unrelated audio samples, asserting
zero graph, mesh, and audio-resource copies, serialization, or linear scans
through commit and undo.

The native `trimesh-morph-selection` fixture cannot currently prove its undo
assertion: it expects `waveMesh.yellow = 0.317`, while the app reports `0`
immediately after loading the saved graph. This pre-gesture discrepancy is
recorded in `ui-bugs.md`. A focused morph fixture uses the observed loaded
value to check Live drag, durable parameter isolation during movement,
commit, and undo without changing the older fixture's expectation.

### Request-construction extraction boundary

`NodeUpdateGraph` and `GraphExecutionPlan` remain authoritative for product
freshness and dependency topology. A pure request builder may reuse the
existing `GraphPresentationModel::updateRequest` translation unchanged:
compute the effective authoring fingerprint, collect changed and probed roots,
and name typed product invalidations. It must receive edit identity from the
gesture session and hand its request to `NodeUpdateGraph`; it must not plan
dependency closure, execute products, or choose refresh policy. The end state
deletes `GraphPresentationModel::updateRequest` and keeps request translation
in the scheduler below the presentation facade.

The first extraction moves fingerprint and typed invalidation construction to
`PresentationUpdateRequestBuilder`; `GraphPresentationModel` still owns the
session identity call and the thin request wrapper. The focused preview and
Guide tests pass, and the native wheel fixture retains the baseline counts
above. The scheduler and wrapper deletion remain open.

The production causal planner, product identities, audit trace, incremental
preview execution, and latest-only worker publication landed in July 2026.
The prior `Complete` status was premature: probe-refresh policy and gesture
lifecycle remain distributed across UI callers, and `GraphPresentationModel`
still combines policy, scheduling, rendering, and publication. The completion
criteria requiring one production update policy and independently executable
products therefore remain unmet.

## Problem

The original implementation gap was that Cycle V2 had invalidation policy
objects without routing its production editor path through them. The first
implementation made `NodeUpdateGraph` the production causal planner, but it
did not establish a single owner for edit policy or gesture lifecycle.

`NodeUpdateGraph` is now primarily a cohesive execution kernel: it receives
explicit invalidations, calculates affected products, merges converging paths,
tracks fingerprints, and rejects stale generations. It does not choose between
`OnGestureCommit` and `LiveLatest`. That choice is reconstructed by its callers.

The 2026-09-15 mod-wheel policy correction exposed the cost of this missing
boundary. A behavior that should have selected an existing gesture policy
required 212 added production lines across the keyboard, workspace, canvas,
presentation model, and automation layers. It introduced wheel-specific
gesture state and a wheel-specific asynchronous presentation entry point.
The verification work was appropriate, but the production plumbing was not.

The 2026-09-17 Live wheel fixture makes the remaining commit failure
deterministic: after a movement preview has published, mouse-up raises the
preview render count from 3 to 4. The trace reports three requests, two
publications, and one stale/cancelled request for a down/drag/up sequence.
The On Release wheel path has a separate focused repair, but the Live commit
still needs the shared session to distinguish a current preview product from
the durable audio-configuration update. `PresentationRefreshPolicy` has begun
as a pure decision boundary; movement routing is a partial migration, not
completion of the session or scheduler deletion targets.

The next extraction moves the semantic edit gate, active stream, and pending
movement identity out of `GraphPresentationModel` into
`PresentationGestureSession`. This is deliberately an identity/lifecycle core,
not yet the complete session contract: dispatcher-owned durable base revision,
semantic delta, graph snapshot, scheduling, and commit reuse still need to
move below the UI callers. Tests cover two movements in one gesture and
independent source streams; the remaining completion criteria remain open.
The extraction also exposed that `SemanticEditGate::cancelGesture` discarded
the gesture marker without restoring its prior effective fingerprint. The
session's cancel path now restores that fingerprint so retrying an aborted
movement is accepted instead of misclassified as a no-op.

All explicit Live-versus-On-Release checks in `NodeCanvas` and
`NodeEditorCommandService` now consult `PresentationRefreshPolicy`; the canvas
preference toggle still chooses the enum value, but does not implement update
behavior. This centralizes the boolean policy choice without yet unifying the
family-specific commit callbacks or eliminating their separate schedulers.

The current policy distribution includes:

- ten explicit `ProbeRefreshMode` branches across `NodeEditorCommandService`
  and `NodeCanvas`;
- separate gesture state machines for ordinary parameters, paired parameters,
  morphs, vertex parameters, mesh edits, curves, and the preview mod wheel;
- twelve direct `refreshNodeEditorPresentation()` calls in
  `NodeEditorCommandService`;
- synchronous, asynchronous, scheduled, and value-specific refresh entry
  points exposed through `NodeCanvas` and `GraphPresentationModel`; and
- direct broad presentation refreshes from both `NodeCanvas` and
  `NodeCanvasAuthoring`.

### Live wheel semantic decision still needed

The Live wheel path currently renders CC1 against one immutable graph captured
at gesture start, then commits saved Trimesh/Envelope morph parameters only on
mouse-up. The commit can therefore change the effective graph independently
of the final CC1 value. Its second preview render is not safely removable by a
wheel-value equality check. Making the commit reuse decision truthful requires
product-level fingerprints from the actual affected runtime configuration.

There are two possible semantic contracts for the saved morph fields during a
Live wheel gesture. They can be updated transiently on each movement (one undo
transaction, with an immutable delta/overlay for worker jobs so movement never
clones the full graph), or remain commit-only while CC1 previews live (in which
case a second render at commit may be required whenever saved fields affect
the product). This choice affects audio, Spy output, undo, and the shared
session/scheduler design. Do not set `finalMovementAlreadyPublished` merely
because the numeric wheel position matches; that would bless stale products.

As of the reopening audit, `GraphPresentationModel.cpp` is 954 lines,
`NodeEditorCommandService.cpp` is 805 lines, and `NodeCanvas.cpp` is 2,349
lines. File size alone is not the defect; the defect is that all three own
parts of the same gesture-to-derived-product decision.

The current paths also conflate four different lifetimes:

- transient state visible during one drag movement;
- local derived editor products such as a 2D slice;
- offline traversal products used by compact previews and signal probes;
- durable graph state and audio configuration committed at gesture end.

This is visible in two representative failures:

- one Envelope morph-plane movement synchronously changes red and blue as two
  slider notifications, producing two intermediate publications and two local
  rasterizations;
- a Trimesh morph parameter schedules compiled presentation refresh even when
  it changes the primary view axis, although the traversal grid already sweeps
  that axis and therefore has no downstream change.

Repaint coalescing does not solve duplicated semantic computation. The update
system must make the unit and cause of work explicit and auditable.

## Goals

- Give every semantic authoring edit a stable causal identity.
- Create semantic edits only when normalized effective state changes.
- Execute each affected node product at most once for that edit.
- Merge dirty paths before execution so a converging graph cannot recalculate
  a node twice.
- Avoid repeating work on gesture commit when the final live-edit revision was
  already calculated.
- Distinguish local presentation, preview traversal, audio preparation, and
  durable publication.
- Make update decisions and execution counts inspectable in tests and agent
  automation.
- Support an application preference that updates probes either on gesture
  commit or continuously from the latest movement state.
- Preserve immediate local feedback in both modes.
- Make the refresh preference a value consumed by one policy object rather
  than a branch repeated by each editor or control.
- Give every continuous control the same presentation-gesture session instead
  of adding control-specific lifecycle and worker plumbing.
- Keep `NodeUpdateGraph` ignorant of UI preferences and keep widgets ignorant
  of graph snapshots, refresh scopes, workers, and probe publication.

## Cycle V1 Reuse Decision

Cycle v1's `Updater`, `Updater::Graph`, and `Updater::Node` in
`lib/src/Design/Updating` are the authoritative behavioral reference. Their
important guarantees are:

- a source marks a dirty path rather than directly invoking every consumer;
- parent dependencies execute before children;
- dirty paths converging on one node execute that node once;
- an update resets per-pass dirty/executed state;
- pending repeated updates have an explicit throttling and reduction policy.

The concrete class should not be installed behind a Cycle V2 adapter. It owns
a mostly fixed graph of raw `Updateable*` targets, uses source integer codes and
untyped `UpdateType`, recursively resets head graphs, and has no edit identity,
product identity, immutable transient state, or execution trace. Adapting
Cycle V2 to those assumptions would create a second topology beside the
compiled node graph and conceal the distinctions this TDD requires.

Cycle V2 will instead preserve the mature algorithmic contract while using
`GraphExecutionPlan::nodeOrder` and `GraphDependencyIndex` as its authoritative
topology. Do not independently reconstruct graph edges in the scheduler.
Parity tests must exercise diamond and multi-root graphs against the Cycle v1
exactly-once/order guarantees. The implementation should be factored as a
small general dirty-DAG execution kernel only if both clients can use it
without exposing Cycle V2 product policy to Cycle v1. Reuse is not achieved by
copying the old traversal into a new UI service.

## Identity and Revision Model

An input router distinguishes a delivered input attempt from an accepted
semantic edit. It creates one `EditId` only after domain normalization changes
effective model or DSP state. One mouse movement over the Envelope morph plane
is at most one edit containing both red and blue, not two slider edits. A
`GestureId` groups the accepted movement edits and final commit.

```cpp
struct EditIdentity {
    uint64_t editId {};
    uint64_t gestureId {};
    EditPhase phase { EditPhase::Movement };
};
```

`EditId`, graph document revision, domain model revision, and rendered-product
revision are different concepts and must not substitute for one another.

Every immutable transient node snapshot carries a content revision. The
scheduler keys execution by `(EditId, nodeId, product)`. A product cache also
records its input revision fingerprint. This provides two guarantees:

1. the same product cannot execute twice during one edit, even when reached by
   multiple dirty paths;
2. committing an already-rendered final live state records `AlreadyCurrent`
   instead of recalculating identical content under a new commit edit ID.

Attempting a second execution for the same key is an invariant violation, not
an optimization opportunity.

## Effective-Value Gate

Raw control values do not define change. Every authoring input first passes
through the same authoritative normalization used by its domain model or DSP
configuration:

```cpp
EffectiveEditResult normalize(EditAttempt attempt, const NodeState& current);
```

Normalization includes declared range constraints, integer conversion,
quantization, snapping, topology/collision constraints, and domain-specific
canonicalization. The result contains the canonical effective value and the
fields that actually changed.

If the normalized effective state equals the current state:

- do not allocate an `EditId`;
- do not advance model, graph, product, or source generations;
- do not cancel a pending downstream update for the current effective state;
- do not rasterize, serialize, prepare DSP, traverse, repaint derived content,
  create history, or publish;
- optionally record the input attempt as `NoEffectiveChange` diagnostics.

Pointer/hover presentation may still update independently when the interaction
overlay itself changes. It must not masquerade as a model or DSP edit.

Effective equality is product-specific. A semantic model edit may change a
local slice or editor field while leaving the effective downstream DSP output
unchanged. In that case the edit receives an identity and updates only its
local products; preview traversal, probes, audio configuration, and their
generations remain untouched. Trimesh primary-axis morph is the representative
case.

Each derived product therefore compares its own effective input fingerprint
after normalization. Merely changing the serialized authoring representation
does not dirty a DSP product whose canonical configuration and output are
unchanged.

Equality is defined by canonical domain representation, not a generic floating
epsilon. Integer DSP quantities compare as integers, choices as normalized
identities, snapped values as their exact snap index/value, and constrained
vertices by the accepted model coordinates. Continuous parameters compare
their canonical stored targets. Setting a smoothed parameter to its existing
target does not restart smoothing or dirty DSP preparation.

For a multidimensional edit, unchanged coordinates are omitted from the change
set. If one Envelope morph coordinate changes and the other does not, the
movement remains one atomic edit with one changed field. If neither changes,
there is no edit.

### UI and DSP quantization

The UI must display and manipulate the same effective values used by DSP.
Controls whose DSP meaning is discrete expose that discrete domain through
their parameter definition or domain control contract. For example, an impulse
response length that resolves to an integer length must visibly snap to the
same integer steps; it must not present continuously changing values that all
map to one DSP configuration.

The UI and DSP must call one shared normalization/quantization function. Do not
duplicate rounding formulas in slider code and processor code. When conversion
depends on context such as sample rate, that context is an explicit input to
the normalizer and the effective value/configuration key is what invalidation
compares.

A constrained vertex drag follows the same rule. Collision, ordering, bounds,
or topology constraints return the accepted coordinates. Repeated attempts to
move beyond a constraint produce no mesh revision and no local or downstream
update until the accepted vertex position actually changes.

## Update Products

Invalidation addresses a product rather than treating a node as one monolithic
cache:

```cpp
enum class UpdateProduct {
    LocalSlice,
    LocalSurface,
    InteractionOverlay,
    CompactPreview,
    PreviewTraversal,
    ProbePreview,
    AudioConfiguration,
    DurablePublication
};
```

The final implementation may use flags or stronger product types, but it must
retain these semantic boundaries. A local slice request must not implicitly
mean full surface, graph traversal, or audio preparation.

Domain policy translates a typed edit into dirty products. Static
`ParameterImpact` metadata remains useful for ordinary parameters, but it is
not sufficient for contextual changes such as a Trimesh morph axis. The
policy receives the node kind, changed fields, primary view axis, gesture
phase, and probe refresh preference.

Required initial policies include:

| Edit | Immediate local work | Downstream preview work | Commit work |
| --- | --- | --- | --- |
| Trimesh primary-axis morph | One 2D slice and overlay | None | Persist parameter only |
| Trimesh non-primary morph | One 2D slice; dirty surface separately | According to probe preference | Persist and publish audio configuration |
| Envelope red/blue morph | One atomic 2D slice | According to probe preference | Persist one combined state and publish audio configuration |
| Curve vertex movement | Affected local curve range | According to probe preference | Persist one model snapshot and publish audio configuration |
| Hover/selection | Interaction overlay only | None | None |
| Topology edit | Local graph presentation | Once after committed topology | Compile and publish once |

Changing a Trimesh primary-axis morph must not dirty compact grid previews,
preview traversal, probes, or audio DSP because those products sweep that
axis. Changing which axis is primary remains a distinct operation and may
invalidate traversal semantics.

## Transient Edit Session

Editors must not mutate the durable `GraphDocument` merely to display a drag.
An edit session owns the latest immutable transient snapshot:

```cpp
class NodeEditSession {
public:
    EditEvent apply(EditDelta delta);
    EditEvent commit();
    void cancel();
};
```

Local products and optional live probe execution consume the transient
snapshot. Commit writes the final combined state to `GraphDocument` once and
creates one undoable command. A committed state that is semantically identical
to the last live snapshot must reuse its derived products.

This boundary removes model serialization and graph-document mutation from
pointer movement. It also ensures an Envelope plane movement changes red and
blue atomically.

## Planning and Execution

The causal update scheduler performs four explicit phases:

1. Resolve the typed edit into source product invalidations.
2. Mark the affected downstream closure in the compiled dependency index,
   limited to products with active consumers such as visible compact previews
   and signal probes.
3. Merge causes and product flags for every target before executing anything.
4. Visit `GraphExecutionPlan::nodeOrder` once and execute each dirty product
   whose input fingerprint is stale.

Multiple changed roots in one atomic edit are marked together. A downstream
join executes after all dirty parents and only once. Work discovered after a
target has executed for the same edit is a scheduler error because it means
planning and execution were improperly interleaved.

Topology changes compile one new execution plan before derived execution.
Ordinary parameter, curve, morph, and model-snapshot edits must not compile the
topology. Audio configuration preparation and offline preview traversal are
separate products even when they share domain processing cores.

The scheduler runs authoring/preview work outside the realtime audio callback.
Immutable audio configuration publication retains the established realtime
handoff rules.

## Synchronous Local and Asynchronous Downstream Work

Applying transient editor state and producing the minimum local editor product
is synchronous with the input event. An Envelope morph-plane movement, for
example, atomically installs red and blue, renders one 2D slice, and requests
one panel repaint before downstream work is queued. The repaint itself remains
on JUCE/OpenGL's normal frame lifecycle; the updated local model and render
product do not wait for graph traversal.

Preview traversal, compact previews, and probe grids consume an immutable
snapshot asynchronously. They must never hold the message thread while
walking the processing graph. This gives local interaction priority while
allowing the Spy rail to converge on the most recent edit as quickly as the
derived work permits.

The boundary is semantic rather than merely threaded: moving a full-grid
rasterization onto a worker does not make it valid local-slice work. Local
callbacks retain their operation-count contracts, and downstream execution
retains exactly-once accounting for every edit that is allowed to execute.

## Supersession and Cancellation

Each source edit stream owns monotonically increasing product generations.
When a new movement edit changes the effective fingerprint for a product from
the same source, whether in the current gesture or a later one, it supersedes
older queued work for that product whose result has not been published. The
local product for every delivered movement remains synchronous when that
movement changes its effective state; only stale downstream work is cancelled.
A no-op input attempt or local-only edit does not advance downstream product
generations or supersede useful pending work.

Cancellation has three safe points:

1. A queued job is removed before it starts.
2. A running job observes its cancellation token between node products and at
   natural boundaries inside expensive multi-column work.
3. A non-interruptible product may finish privately, but its result is rejected
   at the publication boundary when its generation is no longer current.

No cancelled job may partially publish caches, probe tiles, presentation
revision, or audio configuration. Derived results publish atomically only
after a final generation check. A commit supersedes all pending movement work
for that gesture and receives scheduling priority, while still reusing a
completed final-movement fingerprint when available.

Supersession is scoped by source identity. An unrelated node's edit is not
cancelled merely because another editor moved. If independently queued edits
are combined into one execution pass, the trace retains every contributing
cause and the merged pass must still respect topological and exactly-once
product rules.

## Probe Refresh Preference

Add `AppSettings::ProbeEditRefreshPolicy` to the central application-settings
enumeration and initialize it in `Settings::initialiseSettings()`. Store the
integer value through a typed UI/runtime mode:

```cpp
enum class ProbeRefreshMode {
    OnGestureCommit,
    LiveLatest
};
```

The Spy rail exposes this as a compact `On Release` / `Live` toggle. It is an
application UI preference, not graph content. `OnGestureCommit` is the default
until the incremental live path satisfies this TDD's operation-count and native
interaction gates.

- `OnGestureCommit`: every movement still updates its local editor products,
  but preview traversal and probes execute once for the final committed state.
- `LiveLatest`: each semantic movement edit requests one incremental downstream
  traversal for the affected active probes. A newer movement from the same
  source cancels or invalidates stale pending work so obsolete calculations do
  not increase latency for the latest visible result.

Each superseded `EditId` remains visible in the trace and must never be reported
as completed or published. “Live” promises prompt convergence on the latest
edit, not that obsolete intermediate frames consume the full graph.

Mouse-up in live mode persists the state but does not rerun a preview whose
input fingerprint matches the final movement.

## Corrective Architecture

The policy and lifecycle correction introduces four explicit boundaries. The
names below describe responsibilities; implementation naming may vary, but the
boundaries and deletion targets are required.

### Refresh policy

A pure `PresentationRefreshPolicy` translates semantic edit context into a
refresh decision:

```cpp
struct PresentationEditContext {
    EditPhase phase { EditPhase::Movement };
    ProbeRefreshMode probeMode { ProbeRefreshMode::OnGestureCommit };
    UpdateProduct localProduct { UpdateProduct::LocalSlice };
    bool downstreamChanged {};
    bool finalMovementAlreadyPublished {};
};

enum class DownstreamRefresh {
    None,
    LatestAsync,
    CommitAsync,
    ReuseLatest
};

struct PresentationRefreshDecision {
    std::optional<UpdateProduct> localProduct;
    DownstreamRefresh downstream { DownstreamRefresh::None };
};
```

The policy contains no graph, widget, worker, or renderer references. Domain
code supplies semantic facts such as whether a Trimesh morph changes the
effective downstream product; the policy does not rediscover domain behavior
through `NodeKind` switching. Changing the application preference changes the
policy input once. Callers must not branch on `ProbeRefreshMode`.

### Presentation gesture session

One `PresentationGestureSession` owns the shared begin/update/commit/cancel
lifecycle for continuous presentation edits. It retains the source stream,
durable base revision, latest effective fingerprint, change status, final
semantic delta, and any immutable graph snapshot required by live downstream
work. It delegates durable graph mutation to `GraphCommandDispatcher` and
delegates refresh choice to `PresentationRefreshPolicy`.

Widgets and domain editors report semantic gesture events only. They do not
capture graphs, choose synchronous versus asynchronous refresh, flush workers,
or know whether probes are live. Discrete changes use the same session as a
single-update gesture rather than bypassing the policy.

### Presentation scheduler

A `PresentationRefreshScheduler` consumes refresh decisions and typed product
invalidations. It owns coalescing, immutable job input, latest-only generation,
cancellation, worker execution, and message-thread publication. It is the sole
owner of synchronous-local versus asynchronous-downstream scheduling.

`NodeUpdateGraph` remains the authoritative dirty-DAG planner and exactly-once
ledger below this scheduler. Move `ProbeRefreshMode` out of
`NodeUpdateGraph.h`; a planner must not own an application UI preference even
if it does not currently consult it.

### Presentation model facade

`GraphPresentationModel` becomes the authoritative published snapshot and a
facade over independently owned compilation/configuration, preview rendering,
and scheduling services. It must not grow a new value-specific sync/async API
for each preview control. Preview pitch, mod wheel, and future performance
controls submit typed semantic deltas through the same session and scheduler.

The stable end state is:

```text
UI control or domain editor
    -> PresentationGestureSession
    -> PresentationRefreshPolicy
    -> PresentationRefreshScheduler
    -> NodeUpdateGraph
    -> product executors and atomic snapshot publication
```

This is a direct extraction of existing behavior, not a compatibility layer.
There must be no parallel old and new refresh policy after migration.

## Corrective Deletion Targets

- Delete every `ProbeRefreshMode` branch from `NodeEditorCommandService` and
  `NodeCanvas`; only `PresentationRefreshPolicy` may interpret that mode.
- Delete the mod-wheel-specific gesture state from `NodeCanvas` and the
  mod-wheel-specific async refresh entry point from `GraphPresentationModel`.
- Replace the separate parameter, pair, morph, vertex, mesh, curve, and preview
  control scheduling state with the shared session. Domain state needed to
  produce a semantic delta remains with its domain editor.
- Remove `scheduleNodeEditorRefresh()`, `flushNodeEditorRefresh()`, and
  `refreshNodeEditorPresentation()` from `NodeEditorPresentation` after all
  callers submit typed gesture events.
- Remove direct broad refresh calls for ordinary semantic edits from
  `NodeCanvas`, `NodeCanvasAuthoring`, and editor command paths. Initial load,
  topology replacement, and explicit recovery may retain named full-refresh
  operations.
- Extract request construction and edit-gate ownership from
  `GraphPresentationModel`; delete `latestMovementIdentity` and
  `latestMovementStream` once the shared session owns those identities.
- Move preview rendering/extraction and async lifecycle mechanics out of
  `GraphPresentationModel` so its high-level methods read as snapshot
  orchestration rather than product implementation.
- Delete test fakes that encode the old schedule/flush/immediate-refresh API.
  Tests must observe policy decisions, planned products, and publication.

## Corrective Complexity Contracts

- Policy evaluation and gesture bookkeeping are O(1).
- `OnGestureCommit` movement performs no graph clone, downstream traversal,
  probe render, configuration preparation, or worker synchronization.
- `LiveLatest` captures at most one immutable graph snapshot per gesture.
  Movement updates share it and never clone the complete graph.
- The live queue is bounded per source/product and obsolete work cannot delay
  or publish over the newest accepted value.
- Commit performs no derived work when the final live movement fingerprint is
  already current.
- Adding another continuous preview control requires a semantic-delta adapter,
  not a new policy branch, gesture state machine, or async model method.

## Audit Trace

Every planned product emits diagnostic events into a bounded trace:

```cpp
struct UpdateTraceEvent {
    EditIdentity edit;
    String nodeId;
    UpdateProduct product {};
    UpdateTracePhase phase {};
    std::vector<UpdateCause> causes;
    uint64_t inputFingerprint {};
    uint64_t sequence {};
};
```

Trace phases include `Dirtied`, `Started`, `Completed`, `Published`,
`AlreadyCurrent`, `NotObserved`, `DeferredUntilCommit`, `SupersededBeforeStart`,
`CancelledDuringExecution`, `StaleResultDiscarded`, `NoEffectiveChange`, and
`InvariantViolation`.
The trace records scheduling decisions as well as completed work; otherwise an
absent update cannot be distinguished from broken event delivery.

Tests and agent automation can query:

- work count for `(EditId, nodeId, product)`;
- all causal paths merged into a target;
- topological start/completion sequence;
- why an affected product was skipped or deferred;
- compilation, serialization, slice, surface, traversal, probe, and audio
  preparation counts.

Production tracing must have bounded storage and no audio-thread involvement.
The invariant ledger is part of scheduler correctness and must not disappear
when verbose trace capture is disabled.

## Incremental Preview State

`GraphPresentationModel::refresh()` currently treats refresh as a broad
compile/audio/preview operation. Replace that entry point with named requests
handled by the scheduler. Preserve the current compiled plan across ordinary
edits and cache preview outputs by node, output port, and input fingerprint.

An incremental probe update executes only the dirty nodes needed to reach an
active probe or visible preview. Unaffected cached upstream outputs are reused.
A probe attached before a downstream branch must not cause that branch to run;
a probe after a join requires the dirty paths into that join but still executes
the join once.

Synchronous canvas commits and asynchronous editor refreshes must consume the
same planned preview products. Neither path may replace a downstream dirty set
with a full traversal. A downstream pan regression is accepted only when a
right-centre-left-centre sequence leaves the upstream audio process count and
preview payload unchanged, then edits that upstream node and proves it and its
dependents become dirty. Isolation and reactivation are one paired contract;
separate component tests are insufficient evidence.

`NodeUpdateGraph` must either become the production planner behind this
scheduler or be removed. A second test-only invalidation policy is not an
acceptable final state.

## Verification

Use deterministic counters and traces rather than timing as primary proof.

### Scheduler contracts

- A diamond graph dirtied at its root executes the join's traversal product
  exactly once for one `EditId`.
- An atomic edit changing two roots merges both causes and executes their
  shared downstream products once.
- Parents complete before a dirty child begins.
- A deliberate duplicate execution attempt produces an invariant violation
  and fails the test.
- A topology change compiles once; an ordinary parameter or model edit compiles
  zero times.

### Editor contracts

- One Envelope morph-plane movement creates one `EditId`, applies red and blue
  together, renders one local slice, serializes zero snapshots, and mutates the
  durable graph zero times.
- Repeated Envelope movements that normalize to the current red/blue pair
  create zero additional edit IDs, slices, repaints, or publications.
- A gesture of `N` Envelope movements renders `N` local slices and publishes
  one durable combined snapshot at commit when all `N` movements change
  effective state.
- A Trimesh primary-axis gesture renders its local slice per movement and
  performs zero compact-preview, traversal, probe, and audio-preparation work
  in both probe policies.
- A Trimesh non-primary gesture does not synchronously rebuild the full 3D
  surface from each message-thread callback.
- A vertex repeatedly dragged into an active constraint changes neither mesh
  revision nor any derived product until its accepted position changes.
- A discrete IR length slider visually snaps through the same normalizer used
  by DSP; raw movements within one effective length produce no edit or update,
  while crossing one step produces exactly one.

### Probe policy contracts

- With `OnGestureCommit`, `N` movements produce no downstream probe executions
  before mouse-up and exactly one per affected node product at commit.
- With `LiveLatest`, a movement requests downstream feedback, a completed
  current movement executes each affected product once, and mouse-up adds none
  when the final fingerprint is already current.
- When `N` movements arrive faster than downstream execution, the queue remains
  bounded, obsolete same-source edits are traced as superseded/cancelled, only
  the newest generation may publish, and stale completion can never overwrite
  a newer probe result.
- Unaffected branches and probes execute zero times.
- A connected chain and a diamond graph expose the expected causal trace
  through the agent automation state.

### End-to-end proof

A focused native macOS smoke fixture performs Envelope plane movement,
Trimesh primary- and non-primary-axis movement, and curve editing under both
probe policies. It asserts trace identities and operation counts after each
gesture and uses OS capture for OpenGL feedback. No assertion may claim live
visual behavior without the native event path and captured application state.

Wall-clock measurements are secondary acceptance evidence. Once semantic
counts pass, sustained movement must keep local editor interaction responsive
in a Debug build without waiting for downstream traversal.

## Corrective Implementation Sequence

The existing planner, trace, incremental preview executor, preference toggle,
and latest-only publication are retained. Correct the ownership boundaries in
small behavior-preserving slices:

1. Add characterization tests covering the current parameter, pair, morph,
   vertex, mesh, curve, and mod-wheel gesture decisions under both modes.
   Assert products and operation counts, not UI callback names.
2. Introduce the pure `PresentationRefreshPolicy` with a complete movement /
   commit / cancel truth table. Route existing callers through it before moving
   lifecycle state, then delete all caller-side mode branches.
3. Introduce `PresentationGestureSession` and migrate ordinary parameter
   gestures plus the mod wheel as the first two deliberately different clients.
   Prove two updates, final commit, downstream publication, and undo where the
   edit is durable.
4. Migrate paired parameters, curves, Trimesh morph, vertex parameter, and mesh
   gestures. Preserve each domain's authoritative normalizer and local render
   implementation; do not generalize domain algorithms into the session.
5. Extract `PresentationRefreshScheduler` from `NodeCanvas` and
   `GraphPresentationModel`. Preserve the current `NodeUpdateGraph`, worker,
   cancellation, incremental executor, and atomic-publication behavior.
6. Extract request construction and preview product execution from
   `GraphPresentationModel`, then remove broad and value-specific refresh
   escape hatches.
7. Run operation-count scale tests, focused semantic tests, the native policy
   fixtures, standalone build, refactor review, style checks, and deletion
   audit before restoring `Complete` status.

## Completion Criteria

- Every accepted semantic editor movement and commit has a traceable causal
  identity; rejected/no-op input attempts remain distinguishable diagnostics.
- Input attempts whose canonical effective state is unchanged create no edit
  and trigger no derived work.
- No `(EditId, nodeId, product)` executes more than once.
- Same-source obsolete downstream jobs are cancellable and cannot publish
  stale results.
- Identical final live and committed revisions do not repeat derived work.
- Local slice editing is independent of graph compilation and downstream
  traversal.
- The selected probe refresh policy is honored exactly and visibly.
- Cycle V2 has one production update policy and no test-only shadow policy.
- Tests prove both the Cycle v1 exactly-once invariant and the Cycle V2 typed,
  incremental extensions.
- No UI/editor caller branches on `ProbeRefreshMode`.
- Every continuous control uses the shared presentation-gesture lifecycle;
  adding a control does not add refresh scheduling or worker code.
- `GraphPresentationModel` owns published state but does not implement gesture
  policy, UI debouncing, or value-specific async control paths.
- `NodeUpdateGraph` owns causal planning and product freshness but no UI policy.
- Every corrective deletion target is absent. This TDD must remain in progress
  while any old and new policy path coexist.
