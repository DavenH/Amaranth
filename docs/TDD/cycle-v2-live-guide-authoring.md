# Direct live Guide authoring

## Contract

The user requires incremental preset edits through the running app's socket.
Stop using Python preset generators and graph regeneration for sound design.
Inspect the current state, make one reviewable semantic edit, compare the live
Spy/reference result, then retain or undo it. Reproducible captures and analysis
remain useful, but do not replace interactive authoring.

The persistent JSON-lines socket already supports this transport. A raw `nc`
connection is sufficient; no new preset generator, client framework or MCP
server is required. The existing coordinator retains instance/foreground
ownership. Lifecycle coordination is separate from preset authoring.

## First missing slice

`applyNodeEdits` supports node models, but Guide resources are not nodes.
`setGuideParameter` exposes resource controls without point geometry. Existing
mesh vertex commands depend on editor selection. First expose headless Guide
inspection, sparse point editing, creation and explicit attachment/detachment.
Further modelling changes should expose concrete missing commands as they arise.

- `inspectGuideCurve`: guide ID, document/resource/model revisions, canonical
  point IDs/coordinates, controls and actual cube-component assignments.
- `editGuideCurve`: expected document/model revisions and up to 64 `set`,
  `insert` or `remove` point operations. Address existing points by stable ID.
  An insert returns its new ID through inspection. One changed request yields
  one publication/undo; invalid or stale requests leave everything unchanged.
- `createGuideCurve`: reuse the existing default resource and return its ID.
- `assignGuideCurve` / `detachGuideCurve`: explicit mesh node and selection
  index/field, using the same vertex-to-cube rail mapping as the editor.
  Advertise this mapping; do not label a selection index as a cube index.

These commands do not open an editor or depend on selection. They do not change
unrelated nodes, connections, Guide metadata, assignments or audio resources.
Guide rail sampling keeps its current morph-axis semantics; these bindings do
not add a fourth Trimesh modulation axis.

## Ownership and size envelope

- `FlatCurveModel` owns point identity, movement, insertion, removal and curve
  sharpness. Reuse these operations, not a new interpolation implementation.
- `GraphCommandDispatcher::editGuideCurve` validates a complete typed request,
  edits a copy of only the selected small curve and publishes once through
  `publishGuideCurveState`. Existing document/delta code owns undo and runtime
  invalidation. Do not serialize or clone a graph to implement an edit.
- `GuideGraphEditor` and `MeshGuideAttachmentTarget` retain creation and rail
  attachment policy. The socket translates addresses rather than copying it.
- `App/GuideCurveAutomation` decodes JSON and exports current data; a narrow
  canvas entry settles preview workers and refreshes after changed commands.
  Workspace and automation orchestration only forward requests.

New domain and adapter files should each stay around 150–220 lines. Add only
small forwarding/registry entries to existing UI and app coordinators. No new
domain branches in runtime or DSP. Update capabilities and runbook in the slice.

## Verification and completion

Test sparse point sets, insertion/removal IDs, no-op stability, malformed and
stale requests, failed multi-edit rollback, one undo/redo, and preservation of
Guide metadata/attachments and unrelated graph state. Use the canonical curve
model and preparation path to establish an observable downstream shape change.
Verify direct socket requests against the rebuilt exact app, including a small
edit on the current reed Guide, undo, and preserved preset/reference/Spy state.
Exercise creation and attachment on an isolated fixture rather than changing
the user's graph. Build with `--parallel 10`; run focused tests, diff/code-map
checks and the architecture/size review.

Deletion targets: remove generator/reload steps from the canonical modelling
workflow. Existing generation scripts may remain historical reproducibility
artifacts, but must not drive new preset creation or refinement.

## Findings and proof

The first direct raw-socket trial changed `reedOnsetPhase` point 1 from y=.509
to .507, without opening a Guide editor or replacing any model/graph payload.
The resulting 800 ms seeded capture differs from baseline; one undo restores
the complete original WAV byte for byte. Before/after/restored files and the
request-response transcript are in `.context/sax-wave-comparison/`.

The same trial with the comparison shelf open exposed stale pinned Spies:
Guide commits changed audio but left `comparison.captureCount` unchanged.
Refresh pinned comparisons after the existing synchronous authoring refresh
and after successful audio undo/redo. Reuse `ComparisonDock`'s existing callback
and capture owner; hidden shelves still perform no capture. A focused native
fixture protects edit/undo/redo refresh counts.

Focused domain/adapter/invalid-field checks pass 550 assertions in six cases.
They exercise actual default mesh topology rather than the empty demo fixture.
The domain path performs zero graph copies or serialization with 128 unrelated
nodes. New domain/JSON adapters are 110/189 lines; the canvas entry is 20 lines.
`NodeCanvas.h` remains the existing 478-line coordinator declaration boundary:
this slice adds only forwarding and a comparison refresh helper. Large app and
workspace files gain only registry/forwarding calls; domain policy stays in the
new Guide edit owner, canonical flat-curve methods and attachment resolver.

The exact app and test targets build successfully. The native Guide fixture
passes capture counts 1→2→3→4 through edit, undo and redo; the existing Trimesh
comparison fixture also passes. Architecture/code-map/diff checks pass, with no
new size threshold crossing. Factory preset bytes are unchanged. The app is
left on the preserved working copy with both D3 references and the output Spy.

Next concrete gaps are headless sparse Trimesh and Envelope point/topology edits,
and setting preview MIDI CC values while the keyboard is occluded. Expose these
as needed, rather than returning to generator-driven presets. The failed
combined create-and-assign resource leak is recorded separately in `ui-bugs.md`;
this API uses the existing separate creation and assignment calls.
