# Envelope interaction feedback and surface orientation

Status: Implemented and verified on 2026-10-24. Follow-up to Trimesh interaction
publication on 2026-10-23.

Follow-up: Envelope morph persistence regression fixed and verified on
2026-10-24, including controller reconciliation and settled positions.

## Contract

Envelope hover and Shift-drag box selection update local highlights and selection
without graph edits, model serialization, curve rasterization or morph-surface
preparation. Painting an unchanged panel must consume prepared products rather
than reapply controls or run geometry preparation. Work per pointer event must
remain independent of unrelated graph/audio state.

Reduced morph-plane dragging displays its current local cursor throughout the
gesture. Held coordinates must survive repaint/rebinding; release uses the
existing single undoable transaction and cancellation restores the controls.
Cube gestures start from the displayed effective coordinates. An unchanged
controller preview must not replace manually edited coordinates during a gesture,
after release, or when reopening the editor. A changed controller resumes preview
only on its changed axis; undo, redo and document replacement reconcile through
the existing model/presentation lifecycle. Reuse Trimesh's morph reconciliation
policy rather than introducing another competing editor state machine.
Closing a held morph editor restores the preceding authored/manual positions
without publishing. A controller change during that gesture retains ownership,
including an A→B→A sequence; general mesh-gesture cancellation is unchanged.
Trimesh's reduced diagram must expose a real pointer region: the 2D plane edits
Yellow/Red through the existing pair gesture, and the 1D Yellow line retains
primary navigation's zero-edit semantics, including release. Static has no
diagram target. Shared morph-space geometry owns the input bounds/mapping for
both mesh families.

The Envelope surface displays envelope time horizontally, matching the curve
below, and the selected primary morph coordinate vertically. Vertex rails remain
visible in the same coordinate system, including release time beyond 1. The
surface samples the curve's full fixed time bounds; each zoom host retains its
own viewport. Annotate the primary view axis with its colour: the Trimesh
surface's horizontal edge and the Envelope surface's vertical
edge. All primary choices and both compact and normal layouts must agree.

## Ownership and deletion targets

Reuse mature Interactor selection/highlight behavior, CurvePanelHost preparation
and render lifecycle, TrimeshPanel3D scalar rendering/overlays and existing zoom
owners. EnvelopeMorphSurfacePreview adapts domain columns and geometry; the
shared morph controls own projection/cursor presentation. Existing dispatcher
services own durable edits and undo.

Remove unconditional preparation and layout work from unchanged paints, stale
authored-coordinate cursor reads, and the Envelope surface's missing rail feed.
Do not introduce another interaction state machine, rasterizer, document owner
or per-paint mesh scan to transpose the display.

## Verification

- [x] Preserve before-fix actual-app pointer checkpoints.
- [x] Test real hosted hover/box-selection events and prepare/publication counts,
      including unrelated graph/resource growth.
- [x] Test local reduced-plane cursor feedback, commit and undo.
- [x] Test transposed surface data/rails and primary-axis annotation for all axes.
- [x] Build app/tests, run focused and related regressions, then native pointer
      checks and normal/compact captures sequentially.
- [x] Review production ownership/style/size, architecture audit, code-map and
      whitespace checks; commit the complete slice without user preset edits.

## Completion evidence

The final actual-app report keeps rasterizer refreshes at 4, slice evaluations
at 5, expensive panel preparations at 0 and layouts at 2 across hover and two
held box updates plus release. Surface readbacks remain at 2; curve readbacks
increase only for changed overlays. The box grows from 595 × 227 to 871 × 316
pixels, clears on release and retains 12 vertices. Graph/document/model
revisions, causal edit IDs and audio-plan copies stay fixed. The surface retains
seven rails, nonzero authored values and release time beyond 1.

The matching standalone-debug workload records 12 OpenGL frames averaging
2.75 ms (maximum 8.26 ms), compared with 49.55/66.83 ms before the fix; JUCE
paint averages 2.11 ms. These are measurements, not machine-independent test
thresholds. The saved report is regenerated after adding actual selection-box
bounds; a retained action tag alone cannot establish a visible native drag.

Focused tests pass 13 cases / 2,671 assertions; related controller, Guide-lock,
zoom, reload, preview and gesture tests pass 25 cases / 1,596 assertions. The
shared Envelope rasterizer's three existing default-contract cases pass. Tests
include unrelated graph/audio growth, native-pixel conversion, pan/zoom cache
invalidation, typed prepared-mesh lifetime, numeric full-time surface sampling,
both reduced pointer modes, all cube faces and primary-axis choices. Real
controller presentation revisions and surface rebuild identities participate
in the reused snapshot-cache key. Compact rendering invalidates the expanded
image so its selection layout is restored once on return.

Native `envelope-feedback` passes actual hover, continuous Shift selection,
two held plane updates in both families, release/Undo, every primary-axis bar
and normal/compact captures. Envelope plane trace tokens record local curve
feedback without graph/model publication; selection and Trimesh staged movement
also preserve their causal IDs. Modifier events remain in one native input
sequence and assertions wait for actual box geometry. Both the surface-zoom
fixture and the existing Trimesh publication/controller fixture pass on the
exact final app. Reports, logs and captures are under
`.context/envelope-interaction-regression/`.

## Baseline evidence

The morph follow-up is reproduced with the actual app and native input. With
live Red/Blue previews at 0.261682/0.251969 and saved controls at 0.5/0.5,
pressing the cube jumps to the saved controls. Two held updates reach 0.66/0.427;
release saves those values once but returns the displayed cursor to the old
preview. Reopening the editor preserves the wrong display. The reduced plane
likewise saves 0.7/0.2 but displays its old controller coordinates after release.
The new settled-position checker rejects the before-fix report at the cube
press; the native cube checker also rejects the held cursor/panel disagreement.
Evidence is retained as `morph-full-baseline-report.json`,
`morph-baseline-rejection.log` and `morph-native-baseline.log` in the artifact
directory above.

The instrumented actual-app fixture keeps document revision 2, graph/model
revisions and edit IDs unchanged, but its five hover/selection checkpoints
increase rasterizer refreshes 9 → 20, slice evaluations 18 → 40, panel
preparations 5 → 16 and layouts 4 → 15. The regression checker rejects the first
hover because its rasterizer refresh count rises from 9 to 11. A separate copy
diagnostic protects selection release, which previously republished a copied
mesh into the editor model even without a durable graph edit.

The same 1400 × 900 default-graph workload, after resetCanvasPerformance in
standalone-debug, recorded 12 OpenGL frames averaging 49.55 ms (max 66.83 ms)
and JUCE paints averaging 2.30 ms (max 3.86 ms). There were zero preview requests.
These describe this host/build and establish neither a 350 ms reproduction nor
a universal frame-rate target. Reports and baseline rejection are under
`.context/envelope-interaction-regression/`.

## Morph persistence follow-up verification

The final actual-app morph report contains 18 checkpoints: cube press, two held
updates, release/reopen, a second face, undo/redo, controller takeover, reduced
plane movement and document reset. Press now retains 0.261682/0.251969; the cube
finishes at 0.421683/0.178635 and retains those effective/displayed coordinates
after release and reopening. The plane retains 0.7/0.2. Held updates keep
document/graph/model revisions and audio-plan copy counts fixed; each release
publishes once and stores matching model/control coordinates.

Focused tests pass 14 cases / 3,171 assertions, including the new 500-assertion
live-preview regression for both dimensionalities. It exercises repeated
preview injection, held/settled rebinding, close rollback after a prior manual
commit, changed controllers and A→B→A ordering, parameter-only authored sync
with the same model pointer, and undo. Related controller/Guide/zoom/reload and
keyboard coverage passes 25 cases / 1,596 assertions. The earlier Trimesh
actual-app publication/controller fixture also passes after extracting its
reconciliation policy.

Native `cube-faces envelope-feedback` passes all three cube faces in both
families, held cursor/panel agreement, unchanged held graph/model/audio counters,
settled positions after release, reduced-plane release persistence, hover and
Shift-selection, plus every primary-axis annotation at both window sizes.
Captures are retained under `morph-native/` and the app is relaunched with the
pre-test graph snapshot restored.

The selection fixture still performs no additional preparation: rasterizer
refreshes remain 24, slice evaluations 45, panel preparations 0, layouts 6 and
surface readbacks 19 throughout hover, two held box updates and release. These
are cumulative counters on the reused app instance; unchanged deltas are the
contract. Reports and logs use the `morph-` prefix in the artifact directory.

The shared `EditorMorphPosition` replaces Trimesh's former reconciliation
algorithm and adds a small per-axis checkpoint for cancellation. Controller
changes advance per-axis generations so rollback cannot revive stale manual
ownership. Envelope adapters retain authored candidates during a held rebind;
explicit editor updates change only the selected face's axes. Both local
overrides are composed before the existing single slice-preparation path runs.
Parameter-only dimension increases refresh the slice once; unchanged syncs
retain the preparation guards.

Production size/responsibility review: EnvelopeEditorComponent grows
1,152 → 1,199 lines for existing control/gesture composition; its adapter grows
146 → 188 for existing morph presentation ownership. CurvePanelController grows
848 → 866 for forwarding/lifecycle and the dimension boundary; the expanded
curve base grows 446 → 452 by extracting its existing control cancellation.
TrimeshPanelBridge shrinks 492 → 464; shared reconciliation is 63 implementation
and 32 header lines. Widget forwarding adds 12 implementation lines. No owner
grows by 200 lines, no domain algorithm is duplicated, and held pointer work
does not copy graphs/meshes or serialize models. Architecture, code-map,
whitespace and production-style reviews pass.

## Prepared surface mesh lifetime

The surface owns one affected-mesh snapshot for its cached scalar grid and
mature vertex rails. Source topology replacement during a held gesture leaves
both products intact; the next eligible preparation replaces them together.
The copy occurs only at that preparation boundary and is instrumented. The
existing panel render lock protects rail parents while the snapshot changes.

The first snapshot used plain `Mesh` for an `EnvelopeMesh` source. The mature
copy path dispatches virtual XML writers/readers, so the Envelope container was
rejected by the plain reader at `Mesh.cpp:186`, producing empty rails and values.
The prepared owner now preserves the source's Envelope/plain mesh type and
delegates copying to the same mature `deepCopy` path. Existing plain-mesh test
inputs remain supported; no geometry or serialization algorithm is duplicated.

## Implementation and responsibility review

Envelope mutations prepare their local slice at the existing edit/control
boundaries. Unchanged controls, logarithmic settings and overlay paints do not
prepare it again. Selection remains panel state until an actual mesh edit
publishes. The host's layout key includes size, scale and zoom, preserving wheel
grid updates while skipping unchanged layout.

Expanded images reuse the existing render-cache algorithm. Model/content,
preview-presentation and surface-rebuild revisions, view bounds and dirty-state
epochs invalidate the appropriate snapshot. Hover invalidates its own overlay
without invalidating compact content or the untouched surface. Reset/context
release discard cached images; validity and diagnostic counters are atomic.
Native JUCE PixelARGB readback replaces per-pixel Colour construction with row
copies, vertical flip and opaque alpha, preserving the old visible-content test.

The transposed grid is prepared once at the existing surface boundary. Mature
Panel3D rail paths and Guide tables share the Envelope visualization seed;
the scalar sampler accepts an explicit time range while its default Cycle 1
sampling contract is unchanged. The local morph controls supply both painted
cursor positions and automation readback throughout held plane gestures.
Trimesh's reduced diagrams now register the same real child pointer target as
its cube. The line routes to existing primary navigation and the plane to the
existing pair gesture. Shared layout input bounds and normalized coordinates
replace the Envelope-only calculation and Trimesh's hardcoded 3D hit policy.

| Production owner | Before → after lines | Responsibility / stable collaborators |
| --- | ---: | --- |
| CurvePanelController.cpp | 829 → 848 | Existing family composition; adapters own mutations, host owns rendering/cache. |
| CurvePanelInfrastructure.cpp | 555 → 626 | Panel hosting, layout and image cache; existing cache/JUCE own mechanics. |
| EnvelopeCurvePanel.cpp | 1133 → 1158 | Mature domain interaction/curve preparation; no new transaction or graph policy. |
| EnvelopeEditorComponent.cpp | 1124 → 1152 | Control composition and ephemeral feedback; shared morph layout owns projection. |
| EnvelopeMorphSurfacePreview.cpp | 161 → 280 | Prepared domain grid/mesh and host adapter; shared sampler/Panel3D own algorithms. |
| TrimeshWidget.cpp | 974 → 981 | Existing view composition and hit-region export; forwards read-only surface facts. |
| TrimeshControlsComponent.cpp | 759 → 803 | Existing child pointer routing; shared layout and gesture service retain geometry/edit policy. |
| MorphSpaceLayout.cpp | 163 → 176 | Shared projection and reduced line/plane input geometry for both families. |
| TrimeshPanel3D.cpp | 228 → 270 | Shared surface presentation, primary colour and edge geometry. |
| Panel3D.cpp | 1118 → 1135 | Mature rail/Guide rendering with one coordinate-orientation hook. |
| NodeCanvasAutomationInspector.cpp | 1312 → 1321 | Read-only target facade translating editor-owned primary-axis rectangles. |

Large files retain their existing cohesive responsibilities and policy owners;
the inspector adds only target translation, so no new per-family layout or edit
policy needs extraction. No production file grows by 200 lines. No mesh, graph,
serialization or audio work is added to hover/selection movement. The surface's
affected-mesh copy is limited to preparation, with render → grid lock ordering
shared by preparation/drawing. There is no second undo, document or rasterizer
authority.
