# Cycle V2 Causal Update Graph

## Status

Complete.

## Problem

Cycle V2 has invalidation policy objects, but its production editor path does
not execute work through them. `NodeUpdateGraph` is currently exercised by
tests without governing `GraphPresentationModel`, editor-local rasterization,
or graph preview execution. Consequently, the application can satisfy policy
tests while a single pointer edit still rasterizes, serializes, publishes,
compiles, or traverses more than once.

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

## Implementation Sequence

1. Add edit/gesture identity and the bounded audit trace around the current
   paths to establish failing multiplicity evidence.
2. Introduce atomic transient edit sessions and remove graph serialization from
   movement callbacks.
3. Turn `NodeUpdateGraph` into the production causal planner over the compiled
   dependency index, or replace it and delete the test-only object.
4. Split `GraphPresentationModel` compilation, configuration preparation,
   incremental traversal, and probe publication into independently executable
   products.
5. Wire Envelope, Trimesh, and flat curve editors through the same edit
   lifecycle and product scheduler.
6. Add the central application setting and Spy rail toggle, then implement both
   policies.
7. Run the refactor, style, semantic verification, standalone build, and native
   smoke gates before declaring the TDD complete.

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
