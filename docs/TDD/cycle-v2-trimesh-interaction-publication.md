# Trimesh interaction preview and publication

Status: Implemented and verified on 2026-10-23. Regression reported after the
dimensionality/live-authoring merges; repair starts from `origin/master`
(`da296f07`) on `DavenH/trimesh-interaction-regression`.

## Contract

- Every eligible morph rail follows actual pointer input, including Red and Blue
  with live controller preview configured. Manual editor navigation takes
  precedence over an unchanged controller preview; a subsequent controller
  change may move that axis again.
- A primary-axis morph rail selects the local slice. Pointer-down, multiple
  movement events and release never change edit/document/model revisions,
  publish graph changes, copy/serialize graph state or create an undo entry.
  The prepared surface remains unchanged while its selected slice moves.
- Cube/plane dragging updates local control feedback. The committed graph and
  editing view remain unchanged throughout movement. Release commits the final
  coordinate pair together in one undoable transaction; cancellation
  restores the initial controls without publication.
- Ordinary secondary morph rails preview locally and retain their existing
  persisted fallback semantics with one commit on release. Unchanged release
  must not reconstruct an already-prepared local surface.

## Ownership and deletion targets

The mature Cycle 1 CubeDisplay and shared MorphSpaceLayout own projection and
hit/drag geometry. TrimeshControlsComponent translates real child pointer
events; the expanded editor/widget/bridge own ephemeral control and slice
presentation. GraphCommandDispatcher and NodeEditorCommandService retain durable
command, transaction and undo ownership. GraphPresentationModel remains the
source for actual live controller preview.

Remove per-movement graph parameter commands for these editor gestures and the
unconditional priority of live preview over a manual editor position. Do not
disable live controllers globally, copy interpolation algorithms, add a second
document owner or infer performance from elapsed time alone.

## Verification and completion

- [x] Capture the failure from the real expanded-editor pointer path.
- [x] Hosted integration tests cover all three eligible rails, primary-axis
  zero-publication semantics, multi-update cube movement, commit and undo.
- [x] Actual-app integration covers live controller preview interleaved with
  manual movement and records revision/copy/preparation counts.
- [x] Movement work stays independent of unrelated graph/resource size; primary
  navigation and held cube movement do not rebuild the full surface.
- [x] Build exact app/tests, run focused regressions and related interaction
  tests, inspect native input and screenshots, review style/ownership/size,
  run architecture/whitespace/code-map checks and commit the completed slice.

## Implementation and proof

The live-authoring preview overlay gave mapped controller coordinates priority
over local editor updates, making Red/Blue look stuck. Meanwhile the expanded
editor sent every slider/cube movement through graph parameter commands. Deferring
durable publication alone did not prevent edit IDs, transient graph work and
downstream scheduling during movement.

The bridge now reconciles three axis-sized local overrides with authored and
controller positions. Repeated painting/rebinding keeps manual navigation;
changed controller coordinates resume preview only for their axes. Effective
positions also select the key-scale slice pitch. Primary service navigation
bypasses dispatcher gestures entirely. Secondary rails stage local slice updates;
cube gestures retain prepared geometry and update only control presentation.
The existing transaction owner receives final coordinates at release.

Closing a held secondary rail restores its initial effective coordinate without
publishing; closing primary navigation retains its selected slice. Document
reset clears local overrides through the existing NodePreviewResources lifecycle,
so loading the same node IDs cannot retain navigation from the preceding graph.
Removed expanded-editor live-update delegate methods, unused pair-update service
API and redundant pair state. No graph, rasterization or interpolation algorithm
was added or duplicated.

`baseline-report.json` shows Red staying at .898 and Blue at 0 while each two-move
drag advances three edit IDs. Yellow release advances the document revision and
edit ID too. The actual-app regression rejects this saved baseline at Red's
unchanged displayed value. The repaired fixture checks all three rails, cube
movement/release/undo, subsequent CC1 preview and same-ID document reload. Primary
movement/release and held cube movement leave document/graph revisions, causal
edit IDs and audio-plan copies unchanged. Full-surface rebuild count stays fixed
for primary navigation and held cube movement; secondary release retains the
already prepared surface. A real CC change schedules preview events while
leaving the graph/document intact and preserving unrelated navigation.

The final focused selection passes 23 cases / 1,935 assertions: all primary-axis
choices, empty/128-node unrelated-state variants, live controller precedence,
selection, cancellation, reload, cube faces in both families, undo, invalidation,
persisted fallback editing and controller/audio behavior. Movement asserts zero
graph/mesh copies, serialization, audio-buffer copies and validation visits.
Fixtures that require authored geometry now seed it explicitly instead of
assuming a new Static/empty factory node contains vertices.

Native macOS `trimesh-morph-drag cube-faces` passes: two held movements on every
rail, unchanged edit/document state while held, no primary release edit, all
three cube faces in both editors, final coordinates and Undo. Native cube checks
start from effective displayed coordinates instead of assuming authored fallback
values equal controller preview. The older primary-gesture and selection fixtures
now follow navigation semantics and retain valid collision-constrained geometry.
Evidence, reports, logs and captures are in
`.context/trimesh-interaction-regression/`. App and test targets build with
`--parallel 10`; architecture, style/production review, code-map and whitespace
checks pass.

## Responsibility and size review

| Production owner | Before → after lines | Responsibility / collaborators |
| --- | ---: | --- |
| TrimeshExpandedEditorComponent.cpp | 681 → 735 | Local gesture staging/cancellation and delegate translation; controls own pointer mapping. |
| TrimeshWidget.cpp | 957 → 974 | View composition and readback; bridge owns reconciliation, grid service owns sampling. |
| TrimeshPanelBridge.cpp | 464 → 492 | Effective morph/panel lifecycle; model and invalidation policy retain slice/surface decisions. |
| ConcreteNodeEditors.cpp | 506 → 499 | Host adapter and automation facts; removed obsolete live edit forwarding. |
| NodeEditorCommandService.cpp | 876 → 879 | Durable transaction routing; dispatcher owns mutations/undo, widget owns navigation. |
| NodeEditorHost.h | 369 → 368 | Host/delegate contracts; removed unused pair update API/state. |
| NodePreviewResources.cpp | 261 → 264 | Existing per-document preview reset; forwards the Trimesh presentation reset. |

The widget, command service and host exceed review thresholds but retain their
existing cohesive boundaries and stable collaborators. No file grows by 200
production lines or reaches a new extraction-plan threshold. Local position
reconciliation is a fixed three-axis operation; graph-size traversal occurs only
at existing document/commit boundaries. The existing shared projection and
render-product ownership remain authoritative.
