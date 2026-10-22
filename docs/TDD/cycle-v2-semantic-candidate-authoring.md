# Cycle V2 Semantic Candidate Authoring

## Status

Complete. Roadmap PR 4; follows synthesis discovery and reuses performance suites.

## Product contract

`applyNodeEdits` applies one bounded list of edits to existing nodes. Each entry
names a node, zero or more stored parameters, and optionally a replacement model
with its expected current model revision. The request supplies the expected
document revision. One successful changed batch produces one publication and undo
entry; a no-op produces neither. Any invalid operation or final graph validation
failure leaves graph serialization, dirty state, document revision, history/redo,
connections and guide assignments unchanged, including collection ordering.

`inspectNodeControls` already returns the current parameters and model without an
editor; add the document revision to that response. Baseline creation, candidate
saving and discard use existing `saveGraph`/`openGraph`. No candidate document,
session ID, alternate snapshot manager or open IPC transaction is introduced.
Node creation, connection editing and guide-resource authoring retain their
existing commands; this batch covers node-owned flat curves, Envelopes and Trimesh.

## Authoritative owners and permitted translation

- `GraphCommandDispatcher` owns transactions, undo and consolidated publication.
  Add a typed `NodeAuthoringEdits` request and dispatcher batch member implemented in
  `GraphCandidateEdits.cpp`; app/UI code never mutates `NodeGraph`.
- `GraphNodeStateEditor`, `NodeDefinitionRegistry` and registered `NodeModelCodec`
  own parameter normalization, model schema/version/revision and domain edits.
  The typed batch carries model payloads to the registered codec itself, so direct
  C++ callers cannot bypass model validation performed by an IPC adapter.
- Existing persistence readers remain unchanged and permissive where migration
  requires it. Add explicit authoring decoding to canonical model codecs. Share
  finite/type/identity/topology checks for mesh data in a node-domain helper;
  retain mature `Mesh` reconstruction rather than copying interpolation,
  rasterization or mesh interaction algorithms. Full replacement validates safe
  structure; collision eligibility remains the mature gesture owner’s policy.
  Unknown serialized fields remain tolerated by persistence and authoring validators.
- `GraphDelta` owns restoration of ordered implicit removals. Current compound
  cancellation uses `finish(graph, {})`, losing removed connections/assignments.
  Record affected routing commands in order, including input replacement and
  cable deletion, and reverse that order for inverse replay. Capture implicit
  Envelope logarithmic state and removal positions at their canonical owners.
  Fix this owner and its inverse deltas; do not mask the gap with UI overlays,
  parameter-only restrictions or a second transaction implementation.
- `NodeCanvas`/`GraphPresentationModel` settle existing worker reads before the
  synchronous batch and refresh once after commit. The existing document listener
  schedules runtime preparation. No per-edit widget or pointer work occurs.
- The automation adapter validates request representation and translates errors;
  schema/domain policy remains in the owners above. Capability advertisement uses
  the registered codecs' actual authoring support and canonical command registry.

## Request, bounds and lifecycle

The command takes `expectedDocumentRevision` and `edits`. Each edit has `nodeId`,
optional `parameters: [{id, value}]` using stored strings, and optional `model`
plus `expectedModelRevision`. Model payloads use existing codec envelopes, with
new revisions strictly above their current revision for changed content. Duplicate
node/parameter addresses, missing edits and active compound/transient sessions
are rejected. Limits are 64 node edits, 1024 total parameter edits, 16384 model
vertices/curve points and 4096 mesh cubes. Coordinate validation preserves valid
factory wraparound/overshoot while rejecting nonfinite/float-overflow values. Limits and supported operations are
discoverable. Decode/validate the complete request before mutation; validate the
resulting graph before commit. Return changed state, the new document revision
and failing operation/domain errors where relevant.

Numeric authoring parameter strings must be finite, complete numeric values;
integer parameters must be integral. Preserve legacy acceptance/normalization by
adding an explicit authoring-validation method rather than tightening migration
readers. Strict model decoding checks required numeric fields, bounded coordinates,
unique identities and cube references before permissive reconstruction. Direct
typed requests pass through this same strict codec path.

All work runs on the message thread, outside audio callbacks. Decode work scales
with edited model payloads; batch/delta work scales with touched values and
required implicit side effects, followed by one final graph validation. Whole
document serialization is used only for explicit inspection/save/proof, never
as transaction or equality machinery. Existing live-gesture paths retain their
complexity. No compatibility facade is needed. Deletion targets are incomplete
implicit-removal restoration and any duplicated ordering/restoration mechanics
replaced by the canonical delta changes.

## Files and review envelope

- `Graph/GraphEditTypes.h`, `GraphCommandDispatcher.h`, new
  `Graph/GraphCandidateEdits.cpp`: typed requests and orchestration, 150–220 lines.
- `Graph/GraphDelta.{h,cpp}` and dispatcher cancellation: ordered side-effect
  restoration, 80–150 lines; preserve existing delta and snapshot history forms.
- `Graph/NodeDefinition.{h,cpp}`, relevant model codecs and new
  `Nodes/ModelAuthoringValidation.{h,cpp}`: explicit authoring-only validation,
  approximately 200–300 lines; persistence behavior unchanged.
- New `App/Automation/NodeAuthoringAutomation.{h,cpp}`, command/capability adapters,
  workspace/canvas forwarding and inspector revision: approximately 180 lines.
- `tests/TestGraphCandidateEdits.cpp`, transaction/model compatibility tests,
  `scripts/create_cycle_v2_candidates.py`, focused Python contracts and docs.

Review touched large files for responsibility/size and run the architecture audit.
Keep dispatcher batch implementation separate from its existing coordinator;
reuse the code-map semantic-edit feature route without adding source-map entries.

## Acceptance and verification

- Batch parameter/model edits use codecs and domain commands without an editor.
  Capabilities advertise only installed operations and authoring-supported codecs.
- Malformed, nonfinite, wrong-schema/version, duplicate-identity, bad-topology,
  stale-revision, duplicate/oversized and invalid-final-graph requests fail with
  no partial durable edit. Direct typed requests receive identical checks.
- Failure after valid edits restores ordered connections removed by Envelope
  purpose and assignments removed by shrinking a guided mesh. Assert serialized
  byte identity, document revision/dirty/history/redo and zero publications.
- Success publishes once, then one undo/redo restores all edits and implicit side
  effects. No-ops and failed requests preserve redo history.
- Existing canonical model defaults and factory/legacy graphs still load. Verify
  strict replacement acceptance of valid existing geometry and tolerance of unknown
  fields, while permissive persistence behavior remains intact.
- A host script uses the shared exact-instance coordinator/session client to save
  one baseline, create two distinct validated candidates with flat-curve/Trimesh
  payloads and parameter changes, save/reload them, discard through `openGraph`,
  and prove invalid-candidate atomicity. It consumes inspected model payloads,
  never patches a serialized document. Existing PR 5 rendering supplies audio
  proof: candidates differ from each other, remain nonzero and repeatable.
- Build exact app/test targets with `--parallel 10`, run focused C++/Python tests,
  code-map/architecture/style/diff checks and sequential real-app demonstration.
  Record incidental runtime incidents in the existing ledgers.

## Evidence

Based on merged master `fc58e73a`; the temporary integration ancestry was removed
before building. Exact app and test targets built with `--parallel 10`.

- Focused C++: 35 cases / 9078 assertions pass across candidate authoring, strict
  model/default/factory compatibility, compound transactions, cables, groups and
  Envelope membership equality. `.context/semantic-tests-final.log`.
- Host contracts: 22 tests pass for candidate authoring and existing performance
  suites. Request bodies preserve inspected payload fields and source objects;
  rejection stops before candidate saving/rendering.
- Real app: `.context/semantic-candidates-final-2/results.json` passes baseline,
  one-publication candidate edits, undo/redo byte identity, save/reload and
  malformed-batch document/history isolation. Both presets were saved through
  `saveGraph`; no document JSON was patched.
- Audio: 16 captures (four cases, two repetitions, two candidates) through PR 5.
  Every repeated pair has identical PCM, every case differs between candidates,
  and RMS spans 0.0064–0.0246. Individual WAVs and suite manifests are in the
  `warm-renders` / `lean-renders` artifact subdirectories.
- App/test logs contain no JUCE assertions, crash indications or leak reports.
  The incidental existing Envelope equality defect and canonical fix are in
  `docs/TDD/ui-bugs.md`.
- Code-map check passes at 100 entries; architecture audit retains the existing
  24 threshold triggers with no new crossing. Changed-file responsibility and
  cohesion review is recorded below; `git diff --check` passes.

Deletion criteria are complete: cancellation no longer drops implicit changes;
ordered routing fragments replace unordered input/cable replay. The batch reuses
canonical parameter, model, validation, undo, publication and document lifecycle
owners, and rendering continues through PR 5's existing path.

## Responsibility and size review

The batch implementation is separate from the existing dispatcher. `NodeCanvas`
(+21 lines) settles worker reads and applies the existing presentation result;
`NodeWorkspace` forwards the typed request. Their existing orchestration roles and
collaborators are unchanged. `NodeCanvasAutomationInspector` adds document facts;
`CycleV2Automation` adds dispatch arms; `NodeDefinition` owns explicit parameter
validation; curve codecs delegate to their small shared authoring validator.
`NodeGraph.h` retains model/storage declarations with one indexed assignment
record (+7 lines). No new source entry is added to the 100-entry code map. The canonical
`EnvelopeMesh::equals` membership comparison now sorts logical cube indices; this
small shared-owner fix prevents false equal-revision conflicts and retains the
existing geometry equality owner. Its regression independently decodes factory
geometry and checks changed loop/sustain membership remains unequal.

Ordered routing fragments replace the previous unordered input/cable replay;
each command copies only affected edge/probe/node state. No compound-start scan
or snapshot is introduced, and live model/parameter updates add no routing work.
The new Python demo builds only inspected domain payloads; PR 5 remains the only
suite orchestration and offline render path.

The host demonstration deliberately uses `old/sat-bass.cyclegraph`, whose existing
`waveshaper`, `timeLayer1` and `voice` nodes support the authored payload examples.
It demonstrates candidate authoring and verification, not generic preset design
or arbitrary topology insertion.
