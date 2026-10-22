# Live shape and controller authoring

Status: Implemented. Dedicated branch `DavenH/live-preset-authoring`, based on
`origin/master` at `ad966fdd` (unified dimensionality and morph editing, #196).
Persistent socket, reference pitch and direct Guide authoring are prerequisites
carried forward without the saxophone study/content history.

## Product contract

An agent can inspect the running preset, make a targeted edit through its
persistent socket, audition with live controllers, compare pinned Spies and
references, and undo. Editors may be closed or occluded. No preset generator,
document JSON patch, hidden editor, replacement graph or reload is required.
Existing factory content and the saved user session remain unchanged by tests.

Implement four coherent slices through completion:

1. Ephemeral preview controllers and raw live MIDI CC delivery.
2. Addressed Trimesh values, cube curve controls and Guide strengths.
3. Addressed Envelope values, Guide strengths and loop/sustain markers.
4. Explicit shape topology creation/insertion/deletion with identity-preserving
   Guide and marker remapping. Split/extrusion uses mature domain operations
   when exposed, rather than a second topology algorithm.

## Current base and parity map

| Behavior | Authoritative owner | Required translation |
| --- | --- | --- |
| Preview CC array and scoped rendering | `GraphPresentationModel::refreshPreviewControllerValue` | Validated CC integers and silent UI reflection. |
| Active voice MIDI delivery | `StandaloneAudioEngine` event sink, existing MIDI control/voice state | One controller event, existing channel semantics. |
| Authored mapped morph | Dispatcher and existing morph commands | Keep distinct from ephemeral audition state. |
| Logical corners and active axes | Shared `MorphDimensions`, graph `MeshDimensionality` | Supply current node family/dimensionality. |
| Trimesh numeric edits and collision rollback | `TrimeshNodeModel`, `TrimeshVertexEditCore`, mature `CollisionDetector` | Explicit addresses rather than selected widgets. |
| Envelope linked movement and seams | `EnvelopeCurvePanel`, mature Envelope interactors/rasterizer | Extract shared domain operations called by panel and socket. |
| Model publication and undo | `GraphCommandDispatcher`, `GraphDelta`, existing typed model publications | One checked batch and one publication. |
| Guide remapping | `GraphMeshCommands::setMeshDimensions`, removed/added assignment deltas | Extract existing index remapping for topology reuse. |
| Cube construction/removal | Mature `Mesh`/`VertCube` and `Interactor` topology operations | Extract domain work; leave pointer/selection/undo orchestration in interactors. |

The latest base changes the earlier proposal: Trimesh 2D uses Yellow/Red;
Envelope 2D uses Red/Blue. New meshes are Static. Inactive-corner replication
must follow `MorphDimensions`, not an assumed eight-independent-corner model.
Envelope links include Yellow, Red and Blue and are editor-session facts, not
saved `link.*` parameters. Addressed Envelope edits declare their link scope.
Its phase range is not the Guide's [0,1] range; use canonical raw field units
and bounds, including valid release extents. Inspect those units explicitly.

## Slice 1: controller state

`setPreviewController {controller, value}` accepts integer CC/value 0..127 on
the existing audition channel 1. It queues one existing MIDI controller event,
updates the canonical preview CC array, refreshes affected previews/Spies and
silently reflects wheel readback even when hidden. Playback must not later
overwrite the value from a stale wheel cache. All 128 values remain inspectable,
including controllers without a visible wheel.

Auditioning changes no document/model revision, preset bytes, undo/redo or
compiled/audio plan. It does not retrigger held voices or reset Envelope
traversal. Queue/validation failures are explicit. `sendMidi` additionally
accepts raw controller events on channels 1..16 through its existing event
queue, without claiming preview synchronization. Deliberate stored morph edits
retain the normal dispatcher/undo path; all active axes, including Envelope
Yellow, obey the shared dimensionality policy.

Expanded editors display effective preview coordinates separately from authored
coordinates. An ephemeral overlay must not become serialized mesh/morph state
when the next shape edit commits. Reuse existing preview bindings and dimension
facts; do not copy modulation-source or port-eligibility policy into widgets.
Keyboard and socket operations share the controller coordinator. Wheel views
reflect state and cannot form a second audition-state authority.

## Slice 2: Trimesh values

`inspectTrimesh {nodeId}` returns document/model revisions, dimensionality,
canonical vertex/cube addresses, stored coordinates, field ranges/units,
ownership and Guide information. Mesh ordinals are valid only at the inspected
model revision; do not call them stable identities.

`editTrimesh` takes node ID, expected document/model revisions and a bounded
typed edit list. Support explicit vertex value edits, explicit cube-average
curve edits and Guide gain edits. Do not advertise the existing `vertex.curve`
cube-average operation as an independent corner edit. Use existing logical
corner replication and collision rejection. The request does not alter current
selection or require an editor. A failed multi-edit batch restores its local
candidate and publishes nothing.

## Slice 3: Envelope values

`inspectEnvelope` exposes purpose, units, dimensions, stable cube IDs, corner
coordinates, Guide gains, loop/sustain membership and authored/effective morph.
`editEnvelope` uses document/model revision guards, stable `cubeId`, corner
ordinal and an explicit Yellow/Red/Blue link scope for value operations.
Support phase/amplitude/sharpness, Guide gains and explicit loop/sustain marker
states. Inactive corners and links follow the current shared axis policy.

Extract linked-corner movement, marker eligibility and loop-seam synchronization
from `EnvelopeCurvePanel` into an Envelope domain edit core. Both the actual
panel and socket call it. Retain mature `EnvRasterizer` marker validation and
Envelope mesh semantics. Pointer state, hover, current selection, repaint and
gesture lifecycle stay in the panel. No hidden panel is constructed by IPC.
Do not treat a morphing Envelope as a flat point path or discard its corners,
release shape, marker membership or stable cube IDs.

## Slice 4: topology and attachments

Expose bounded explicit insert/create and remove operations in the respective
Trimesh/Envelope authoring workflow through `editMeshTopology`. This initial
topology surface exposes pole insertion/removal; it does not expose split or
extrusion gestures. Any later split/extrusion must extract mature operations.
Cube construction uses the same `VertCube`
initialization and inactive-axis conventions as mature editing. Removal retains
shared vertices needed by surviving cubes and applies canonical orphan cleanup.
Topology operations report affected/new addresses so an agent can re-inspect
before its next edit. Do not implement a second selected-cycle modeller.

Guide assignment storage currently uses cube indices. A middle insertion or
deletion must not silently move an assignment to another cube. A domain topology
result supplies the old-to-new retained cube mapping; the graph layer reuses
the existing dimensionality remap and removed/added assignment delta mechanism.
An explicitly split cell inherits its Guide fields on the resulting children.
Removed-cell assignments disappear in the same transaction. Envelope topology
preserves unaffected stable cube IDs and allocates new IDs only for new cells;
marker membership/seam behavior follows the mature operation. Undo restores
geometry, identities, markers, Guide targets and assignment ordering exactly.
Ordinary model replacement is not a topology remapping substitute.

## Ownership, lifecycle and complexity

All decoding and durable edits execute on the message thread. App adapters
translate typed representation and errors only. Graph commands check revisions,
stage only the affected node model, and publish one consolidated change through
existing command/delta facilities. Failure, stale input and no-ops preserve
document/history, unrelated content and selection. No IPC transaction remains
open across requests. Point batches are bounded (initially 64 operations).

Discrete edits may capture the affected model once. Continuous UI updates retain
existing in-place/value-delta behavior; no graph/model serialization, full graph
copy or unrelated preparation is added per update. Collision checking may visit
the affected mesh under its mature cost contract. Live CC delivery remains
allocation/lock free on the audio callback; source selection and Guide sampling
reuse current runtime policy. No new synthesis engine or renderer is introduced.

Expected files and production envelopes:

- Controller coordinator, preview reflection and editor effective-coordinate
  adapters: new focused UI implementation plus small existing forwards,
  approximately 200–400 lines, with equivalent old callback policy removed.
- Typed requests/dispatcher batches and App protocol adapters: separate small
  domain files, approximately 150–250 lines per family. Avoid growing the
  existing large canvas/app coordinators beyond routing and lifecycle calls.
- Envelope edit extraction: approximately 200–350 lines moved into a focused
  core, with panel copies deleted. Shared dimension/remap/topology helpers stay
  beside their existing owners, rather than a generic kind switchboard.
- Focused C++ tests and native socket fixtures own evidence. Root coordinates
  exact app/test builds and sequential GUI checks; domain work can run in
  parallel. Review actual size/responsibility against engineering policy.

Deletion targets: widget-dependent automation for these operations; duplicated
Envelope link/marker/seam policy; inline one-off Guide index remapping replaced
by its shared owner; controller wheel caches as audition truth; generator/reload
refinement from the canonical workflow. Existing selected-widget and pointer
commands intentionally remain as UI regression interfaces. Direct authoring
does not route through them or require their widgets; both paths reuse the same
domain edit rules.

## Acceptance

- Real socket edits work with editors closed and while a different editor is
  open. No hidden widget, temporary graph file or replacement model payload is
  needed. Capabilities advertise installed commands, addresses, limits and units.
- Invalid types, nonfinite values, stale revisions, invalid corners/IDs,
  collisions and oversized/partially invalid batches publish nothing, preserve
  redo and leave bytes/selection/attachments intact. No-op batches do likewise.
- Multiple valid fields produce one publication and one undo/redo; save/reload
  retains the result and existing preset metadata/resources.
- UI and headless edits produce the same logical geometry, including Static,
  1D, both families' 2D layouts and 3D; Envelope all-axis links, valid release
  extents, loop seams and marker validation are covered.
- Insertion/deletion before a guided middle cube preserves the intended target.
  Removed cells, retained markers, new identities and exact undo are covered.
  A later split command must cover inherited Guides and marker transfer.
- Hidden-keyboard CC1/2/3/5/76 and unused CCs have exact readback; one accepted
  event updates held audio without note events/retrigger or document changes.
  Expanded preview coordinates update without leaking into a later saved edit.
- Controller changes and shape commit/undo/redo refresh the pinned shelf Spy.
  Before/after seeded audio changes for a meaningful addressed edit; undo
  restores original PCM. Preserve the user's checkpoint and factory presets.
- Scale unrelated state and assert no graph copy/serialization or unrelated
  processing increase for a fixed edit/CC delta. New live gesture paths cover
  two updates, commit, downstream behavior and undo.
- Build exact standalone/test targets with `--parallel 10`; run focused semantic
  tests, applicable native fixtures, style/diff/code-map and architecture audits.
  Record incidents in existing ledgers. Mark Implemented only after every
  principal slice and deletion target is complete.

## Implementation review and evidence — 2026-10-22

All four principal slices are implemented. Socket capabilities advertise the
installed commands, strict address/value contracts and batch limits. Topology
responses list final cube ordinals and Envelope identities, including new cells;
the caller can re-inspect geometry at the returned revision. Split/extrusion is
outside this initial command surface, as specified above.

App socket routing, transport and authoring adapters live together under
`App/Automation/`; app lifecycle, library state and the audio engine stay in
`App/`. This folder organization changes no command or ownership contract.

The production diff adds 2,167 lines and removes 312 across the four slices and
their shared owners. New focused graph files contain Trimesh batches (113),
Envelope batches (141), topology orchestration (235), Guide remapping (48), and
Envelope Guide preparation (60). App decoders/inspection are 227/218/118 lines
for Trimesh/Envelope/topology; they contain no interpolation or DSP algorithms.
Controller and shape Canvas routing live in separate 40/47-line files.

| Existing owner | Before → after | Responsibility retained |
| --- | --- | --- |
| `NodeCanvas.cpp` / `.h` | 3602/477 → 3603/481 | Routing and lifecycle; new behavior delegates to focused files. |
| `EnvelopeCurvePanel.cpp` | 1301 → 1133 | Pointer state, selection, gestures and repaint; shared domain loops removed. |
| `EnvelopeEditorComponent.cpp` | 1118 → 1124 | Editor presentation and authored/effective coordinate readback. |
| `CurveNodeModels.cpp` | 880 → 926 | Persistence, identity/revision ownership and adoption; no UI policy. |
| `CurvePanelController.cpp` | 804 → 829 | Existing panel lifecycle; effective view settings delegate to the Envelope adapter. |
| `NodeWorkspace.cpp` | 983 → 992 | Shared MIDI/preview/keyboard controller coordinator. |
| `PerformanceKeyboard.cpp` | 983 → 1001 | Wheel reflection and playback; canonical values come from the provider. |

Large-file review also covered Trimesh widget delegation and the App command
switch. They gain forwards/readback, with value/topology policy in the node and
graph owners. New family/kind branches decode typed requests, select the existing
model owner or serialize addresses. No renderer, generic mesh framework, copied
interpolator, copied collision algorithm or alternate undo mechanism was added.

Reuse/deletion proof:

- `EnvelopeEditCore` now owns the actual panel and socket link masks, marker
  eligibility and loop seams. The old panel loops were deleted.
- `VertCube::initLine` and non-owning `Mesh::detachCube` extract mature shared
  construction/removal mechanics; the existing Interactor calls them too.
  Detached objects remain available to interactive undo.
- Dimensional projection and topology call `GraphMeshGuideRemap`; its old inline
  mapping was removed. Existing indexed assignment deltas restore exact order.
- Keyboard callbacks no longer publish wheel gestures to the document. The
  canonical 128-value presentation array feeds silent wheels and playback.
  Effective editor coordinates remain separate from authored publications.
- Owned model adoption removes the extra publication clone. One discrete target
  stage is instrumented. Trimesh clones its target once; Envelope's existing
  `copyFrom` clones live/committed meshes and `synchronizeFromMesh` refreshes its
  baseline. Those existing XML-backed mesh copies are not claimed to be one raw
  deep-copy call. No batch operation or new continuous update copies a model.
- Guide preparation for these Envelope stages prepares only the target's Guide
  resources. Existing all-resource callers retain their behavior. Assignment
  iteration still uses the existing graph assignment scan.
- Empty Envelope models persist and recreate without identity reuse. Optional
  `nextCubeIdentity` preserves the high-water mark across reload; old snapshots
  derive it. Explicit modern marker arrays preserve empty sustain and authored
  sharpness through JSON/XML copies; absent/legacy markers keep their fallback.
- The runbook now starts with direct socket refinement. Whole-model import and
  historical candidate generation remain explicitly separate compatibility
  workflows; no preset generator was used for this implementation.

Verification:

- Exact `CycleV2` and `CycleV2_tests` targets build with `--parallel 10`.
- `.context/live-authoring-final-tests.log`: 53 cases, 3,810 assertions pass.
  Includes both families' dimensions, all eight Envelope link masks, actual
  panel/headless parity, partial/stale/no-op rejection, identity/selection/Guide
  remapping, empty creation/reload, marker/Guide preservation, seeded PCM change
  and exact audio undo/redo, held-voice CC and deliberate stored Yellow updates.
- Scaling unrelated nodes to 128 leaves graph copies/model JSON serializations
  at zero and the target staging count fixed. Scaling unrelated Guides to 128
  leaves the Envelope helper's prepared provider at one assigned resource.
- Native isolated fixtures pass: live shape authoring (55 commands), live Guide
  authoring (13), and actual keyboard wheel interaction (19). The shape fixture
  checks hidden keyboard, held voice, every requested CC and unchanged document,
  audio-plan and compilation revisions. Before/after CC exports are byte-identical.
  Spy captures refresh for controllers and visible shape/Guide commit/undo/redo.
- Five requests over one raw persistent `nc` connection inspect an Envelope,
  set CC76, edit a corner, undo and read CC76 back. All succeed; no generator,
  temporary graph replacement or hidden panel is involved. Evidence:
  `.context/live-authoring-persistent-socket.ndjson`.
- The original app PID 1230 remains running. Its exported graph after the work
  exactly matches `.context/live-authoring-before-branch.cyclegraph`; factory
  content has no diff.
- Whitespace, code-map and architecture audits pass. Size triggers were reviewed
  above. Five old preview fixtures now declare their intended active dimensions;
  a master fixture dereferencing an empty factory mesh now authors its mesh and
  passes its 25 gesture/copy/undo assertions.

The full aggregate suite is not a green gate: it exposed the unchanged master
cursor-initialization failure and the repaired empty-mesh fixture. A broader
304-case runtime/automation run also found missing preset fixtures, outdated
parameter counts and other historical audio expectations. The directly affected
dimension fixtures pass in the final focused run; the remaining broad mismatches
are not claimed fixed or all proven baseline. Existing bug ledgers record the
specific cursor/crash evidence and the unrelated Baroque audition sensitivity
investigation. Native app runs did not assert or crash.
