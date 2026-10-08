# Cycle V2 Immediate Graph Edits and Asynchronous Publication

## Status

Proposed, 2026-10-07. Design only; implement on a separate branch. No runtime
behavior changes are part of this document commit.

## Problem and baseline

The graph mutation for deleting a cable is incremental, but
`NodeCanvasAuthoring::graphEditResult` calls `refreshPresentation()` before
returning to the canvas. `GraphPresentationModel::refresh` then waits for a
preview worker, compiles changed topology, constructs a runtime trace, and
calculates previews on the message thread. Its `refreshAsync` overload
explicitly calls `refresh` when compilation is required. Scheduling an ordinary
UI callback around that call would postpone the same block to another frame.

In a standalone Debug automation run, opening `with-spies.cyclegraph`, resetting
canvas performance counters, deleting edge 10, and inspecting performance gave:

| Measured phase | Time |
| --- | ---: |
| Graph edit handler | 202.652 ms |
| Synchronous presentation refresh | 202.168 ms |
| Graph evaluation for preview data, within refresh | 103.363 ms |
| Preview extraction, within refresh | 21.096 ms |

This is one run, not a percentile or an allocation profile. It establishes
that the presentation refresh dominates this example. The deleted cable and
its anchored Spy are present in the committed graph before refresh completes,
but the message thread cannot paint that change until the call returns.

## Goal and scope

For a committed semantic graph edit, show the changed graph on the next UI
frame and calculate dependent products without blocking the message thread.
The first complete slice covers cable deletion, connection, undo, and redo;
it must use one shared lifecycle rather than a cable-specific bypass. Node
addition and removal should use the same path before the TDD is complete.

The edit itself, validation, undo capture, document revision, selection repair,
and cheap structural invalidation remain synchronous and deterministic. Graph
compilation, runtime trace construction, preview graph evaluation/extraction,
and durable audio-plan preparation are derived work. The current graph may be
visible while those products are pending. Preserve the existing active-audition
policy for switching audio plans; never publish a plan for a different document
revision. A failed compile publishes diagnostics for the edited revision and
never publishes an invalid plan. The handling of the previously sounding plan
must be explicit and tested under the existing active-audition policy.

Initial graph opening, offline export, and explicit capture APIs may remain
synchronous where their callers require a completed result. This TDD targets
interactive semantic edits.

## Authoritative implementations and ownership

| Responsibility | Existing authority | Required extension |
| --- | --- | --- |
| Meaning, validation, undo, and durable revision of an edit | `GraphCommandDispatcher`, domain editors, `GraphDocument` | Emit one consolidated `GraphChangeSet`; do not run UI or DSP policy in commands. |
| Canvas geometry, selection, and repaint | `NodeCanvas` and its existing presentation/scene code | Read the committed graph immediately; invalidate only the affected visible regions and avoid stale derived facts. |
| Causal product selection | `PresentationUpdateRequestBuilder` and `NodeUpdateGraph` | Plan topology work against the new compiled plan, without copying their dependency rules. |
| Job ordering and stale-result rejection | `PresentationRefreshScheduler` and `MessageThreadWorker` | Queue topology compilation and preview work, supersede without waiting during ordinary edits, and publish only the newest valid revision. |
| Compiled plan, previews, facts, and audio-plan revision | `GraphPresentationModel` | Publish one coherent derived snapshot for a committed graph revision. |
| Audio engine handoff | Existing `NodeWorkspace`/audio-engine publication | Swap a prepared valid plan at the existing safe boundary. |

The stable end state is one command publication followed by one presentation
request. UI callers supply semantic edits and repaint facts; they do not choose
which runtime products to build. No separate edit executor, duplicate compiler,
preview renderer, graph traversal, or undo system is introduced. If a small
coordinator is needed to connect these existing owners, it only translates the
committed revision and `GraphChangeSet` into a presentation request.

## Lifecycle contract

1. On the message thread, the dispatcher validates and applies a semantic delta,
   records undo, and publishes a durable document revision plus its consolidated
   change set. Failed or unchanged commands schedule nothing.
2. The canvas updates selection and structural scene state from the committed
   graph and requests a bounded repaint before derived work is complete. The
   immediate paint must not rebuild unrelated node previews or wait for a full
   canvas image. Removed edges, pans, and Spies disappear immediately; pending
   previews may retain last-good content only when they cannot be mistaken for
   the new topology.
3. The presentation owner takes a stable, immutable view of that exact graph
   revision and queues required products. The message thread does not deep-copy
   unrelated nodes, mesh models, or audio samples to make this view, and it does
   not serialize the graph. Workers never borrow the mutable `GraphDocument`
   graph or a mutable editor mesh.
4. For topology changes, the worker builds the new plan first, then performs
   runtime/preview work using that plan and the same graph revision. The
   compiler and preview executor retain their existing domain behavior.
   Non-topology edits continue through the existing causal invalidation path.
5. Publication on the message thread checks document revision and scheduler
   generation, discards stale or cancelled results, and installs one coherent
   plan/trace/preview/facts snapshot. A valid audio plan is handed to the
   existing audio publication boundary. Completion repaints affected derived
   views; it does not reapply the edit or add another undo entry.

The revision check must also cover two quick edits, undo before completion,
redo, graph load, editor close, and destruction. A newer edit may supersede a
running job without calling `cancelAndWait` on the message thread. Waiting is
reserved for shutdown or another proven lifetime boundary. If a worker cannot
stop promptly, its result is still rejected, and its work must not hold up the
next input event.

## Snapshot and thread-safety gate

The current `refreshAsync(NodeGraph graph, ...)` overload copies the graph when
called with a durable lvalue. `snapshotNodeEdits` supports a narrow transient
overlay but relies on a stable base and does not make the mutable committed
graph safe to borrow. Before moving topology compilation to the worker, define
an immutable committed-revision representation that can be retained while later
edits proceed. Reuse immutable node models and shared audio sample storage.
Capture or retain only data affected by the edit; do not move a full graph copy
or serialization into the message-thread request path. If the current concrete
`NodeGraph` compiler API prevents a narrow read view, extract a shared graph
read boundary used by the existing compiler and runtime instead of duplicating
their algorithms in an adapter.

Audit mutable state in `GraphCompiler`, `GraphPresentationModel`,
`PresentationPreviewRenderer`, `NodeUpdateGraph`, and the scheduler. Compilation
and preview jobs for different revisions must not race on shared caches,
executors, or gesture state. Keep message-thread ownership of publication and
UI objects. The implementation branch must record the exact snapshot storage,
thread confinement, lifetime, and deletion of any replaced copy path before
accepting production code.

## Implementation slices and deletion targets

1. **Measure and secure snapshot ownership.** Add separate timing/operation
   counts for command mutation, snapshot capture, compilation, runtime trace,
   preview evaluation/extraction, queue delay, and publication. Prove a worker
   can hold revision N while the UI edits revision N+1 without reading mutable
   storage or copying unrelated graph/audio data.
2. **Queue topology products.** Extend the existing scheduler/model to compile
   and render topology revisions on its worker. Build causal requests against
   the new plan. Remove the compilation-required synchronous fallback from the
   interactive `refreshAsync` path, and remove ordinary-edit use of
   `cancelAndWait`.
3. **Unify caller publication.** Replace the unconditional synchronous
   `NodeCanvasAuthoring::refreshPresentation()` after semantic edits with one
   shared committed-edit request. Keep immediate canvas repaint and selection
   repair. Route deletion, connection, addition/removal, undo, and redo through
   it; remove any duplicate refresh performed by UI wrappers.
4. **Publish safely.** Reject stale results, publish compile diagnostics for
   failed revisions, update audio-plan revision only with the accepted valid
   plan, and keep structural hit testing correct while derived facts are
   pending. Remove old callback paths only after all interactive callers have
   migrated.

Do not mark this TDD implemented after the first successful cable deletion.
The snapshot gate, all interactive topology callers, audio handoff, negative
boundaries, and deletion targets are completion criteria.

## Verification and performance proof

- A focused Cycle V2 fixture deletes a cable with an inline pan and Spies,
  immediately asserts committed topology/selection, and checks that the first
  repaint occurs before derived completion. Repeat for connection and node
  addition/removal, including a compile failure.
- Use a controllable worker barrier in tests: queue edit A, apply edit B, then
  release A. Assert that A cannot restore removed geometry, Spy previews,
  compile diagnostics, or an audio plan. Repeat with undo and redo while A is
  pending, graph load, and canvas destruction.
- Assert one undo step per semantic command and unchanged graph/preview/audio
  semantics after the latest worker result publishes. Include active audition
  and failed compilation at the audio handoff boundary.
- With the `with-spies.cyclegraph` deletion fixture, target a message-thread
  edit handler below one 16 ms frame at p95 across repeated runs, with the
  removed cable visible by the next paint. Measure edit-to-first-visible-paint
  latency and the paint's work separately; a fast handler followed by a 50 ms
  full-canvas paint does not satisfy the goal. Report worker end-to-end latency
  separately; it may exceed a frame. Do not treat a queued UI-thread callback
  as success.
- Scale unrelated nodes, mesh vertices, and audio samples while keeping the
  deleted delta constant. Operation counters must show no unrelated graph,
  model, audio-sample, or serialization copies in the UI phase and no
  whole-graph preparation there. Record baseline and after sizes, decision
  sites, dependency direction, copy counts, and timing by phase.
- Run the architecture audit, changed-file size review, style/diff checks,
  targeted semantic tests, standalone build, and focused agent fixture.

If a stable read view cannot be provided without copying mature compiler or
runtime behavior, document the blocked extraction here and seek architectural
direction before shipping a temporary duplicate path.
