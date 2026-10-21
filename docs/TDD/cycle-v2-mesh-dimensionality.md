# Unified Mesh Dimensionality

Status: Complete — Envelope commit-cached surfaces, normalized colours, zoom hosts and conditional focus verified.

## Contract

Trimesh uses Static, Yellow, Yellow/Red, and Yellow/Red/Blue morphing.
Envelope retains Static, Yellow, Red/Blue, and Yellow/Red/Blue modes. The shared
axis policy takes the two-axis plane as domain data. New nodes start Static. Reduction projects dismissed axes
at authored positions and extrudes orthogonal rails, retaining variation on
surviving axes. Expansion is initially neutral; undo restores removed content.
Envelope morph is latched at note-on, independently of playback time.

Modulation Triple configures Trimesh and Envelope default profiles. Defaults
are voiceTime/keyScale/modWheel and unmapped/keyScale/modWheel respectively.
Unmapped falls back to authored position; explicit cables take precedence.

## Ownership and reuse

Shared mesh dimensionality owns active-axis and logical-corner policy;
shared rasterization owns projection and reduced evaluation. Existing
TrilinearMeshSlicer, guide policies, and curve builders remain authoritative.
Envelope marker/padding/playback and oscillator continuity remain separate.
GraphCommandDispatcher owns atomic dimensional conversion and undo. The
compiler owns default routing and excludes inactive modulation dependencies.
Editors translate gestures to commands and compose shared morph presentation.

Mode conversion inspects the affected mesh at the mode boundary; corner deduplication
uses an ordered map. Movement scales with changed
logical points and incident cells: no graph copies, serialization, unrelated
resource preparation, or hidden-axis collision constraints. Audio allocation
is forbidden. Static geometry is prepared outside audio and never sliced at
note-on or during playback; guide updates reuse prepared geometry.

## Compatibility

Graph format 9 migrates formats 7 and 8 after earlier migration stages. Existing
Trimeshes retain 3D; Envelopes retain Red/Blue and project Yellow=0 onto full
orthogonal Yellow poles. Legacy mapping profiles preserve existing sources.
Cube persistence remains; genuine reduced topology is future shared work.

## Completion and deletion targets

- [x] Shared dimensionality, projection, semantic command, and undo.
- [x] Versioned graph and Cycle 1 converter migration.
- [x] Separate mapping profiles, unmapped fallback, eligible dependencies.
- [x] Shared absolute morph inputs and full Envelope Yellow note-on support.
- [x] Static preparation and reduced evaluation; no realtime allocations.
- [x] Dimension-aware controls, ports, logical edits, and collision behavior.
- [x] Remove forced Envelope Yellow=0, unconditional Yellow pairing, and
      duplicate input-resolution decisions from Cycle 2 production paths.
- [x] Semantic/complexity tests, app build, app-side gestures and screenshots,
      architecture/style/code-map checks and responsibility review.
- [x] Native morph/keyboard acceptance and composited OpenGL screenshots.
      Exact-PID activation avoids System Events aliasing parallel CycleV2 instances.
      Screen Recording is now available; no privacy settings were changed by the agent.

## Verification

- `CycleV2` and `CycleV2_tests` build with `--parallel 10`.
- 16 focused cases / 1,901 assertions pass, including dynamic Envelope realtime
  allocation/lock checks; 24 related runtime cases / 309 assertions also pass.
- All dimensional transitions for both families round-trip and undo/redo.
  Cancellation restores projected geometry and remapped component guides.
- Guided migration matches mature playback at nine Red/Blue positions through
  sustain and release (1e-5 tolerance); marker membership remains durable.
- Reduced evaluation matches mature interpolation at boundaries in time,
  magnitude, and phase domains. Static oscillator ordinary/chained playback
  reports zero slices; Static Envelope note-on reports zero materializations.
- Yellow note-on offset, retrigger, held-note latching, profile precedence,
  independent fallback, configuration refresh, equivalent-buffer sharing,
  inactive dependency omission and cable restoration pass.
- Two logical vertex updates compose and undo with zero graph/mesh copies or
  serialization. Incident-owner visit count stays constant after adding 100
  unrelated cells. Hidden morph-coordinate edits are rejected.
- 67 Cycle 1 converter tests pass. Five old synthetic fixtures now contain valid
  envelope geometry so they exercise projection, rather than raw malformed JSON.
- 55 app commands pass: selector pointer click, reduced rail multi-update/commit/
  undo, all modes, and normal/compact captures. Artifacts in `.context/` include
  `dimensions-final-tests.log`, `dimensions-regression-focused.log`,
  `dimensions-converter.log`, `dimensions-ui-run.json`, and screenshots
  `envelope-dimensions-{0,1,2,3,compact}.png`, `modulation-profiles*.png`.
- Code-map and whitespace checks pass. Architecture audit size triggers reviewed
  below. Initial native permission limitations were resolved during follow-up verification.
- Broader selections still expose the empty-mesh UI crash recorded in ui-bugs,
  a scratch ControlSignal regression and strict pitch-guide lane parity failure
  recorded in audio-bugs. These are not covered by a claim of full-suite success.

## Responsibility review

Baseline -> after lines for the large production files:

| File | Lines | Ownership / stable collaborators |
|---|---:|---|
| GraphCompiler.cpp | 1535 -> 1568 | Plan orchestration; shared MeshDimensionality filters eligibility; mappings remain configuration data. |
| NodeCanvasPresentation.cpp | 1511 -> 1516 | Canvas target/layout presentation; bundle visibility delegates to ModulationCableBundle. |
| GraphSerializer.cpp | 1322 -> 1328 | Persistence orchestration; geometry migration lives in MeshDimensionalityMigration. |
| EnvelopeCurvePanel.cpp | 1295 -> 1305 | Mature envelope interaction adapter; shared MorphDimensions owns logical corners. |
| GraphAudioExecutor.cpp | 1256 -> 1256 | Execution lifecycle; only profile-aware source selection changes. |
| NodeCanvasAutomationInspector.cpp | 1139 -> 1161 | Existing target serialization; adds selector discovery, no edit policy. |
| EnvelopeEditorComponent.cpp | 923 -> 1011 | Domain tools and layout; shared morph layout and renderer compose controls; surface cache is separate. |
| TrimeshWidget.cpp | 888 -> 961 | Existing model/view adapter; shared MorphControlLayout supplies stable geometry; CurvePanelHost owns compact snapshots. |
| TrimeshSidePanelRenderer.cpp | 912 -> 737 | Painting authority shared by both families; MorphControlLayout owns geometry; the reduced plane/line painter is separate. |
| CurveNodeModels.cpp | 862 -> 870 | Typed model identities/codecs; shared projection canonicalizes new Static envelopes. |
| NodeEditorCommandService.cpp | 849 -> 851 | Gesture transaction owner; forwards dimension policy into existing semantic deltas. |
| NodeDefinition.cpp | 824 -> 848 | Declarative node schemas and defaults only. |
| GraphCommandDispatcher.cpp | 809 -> 819 | Transaction orchestration; dimensional edit implementation is GraphMeshCommands.cpp. |

The existing large compiler/serializer/executor remain cohesive orchestration
boundaries. No projection, interpolation, smoothing, or marker algorithm was
copied into them. Shared library policy depends only on mesh/rasterization;
graph adapters add typed ownership and undo; editors consume that policy.
Future compiler growth should extract default-input planning with context
assignment as its collaborator, rather than adding node-family branches.
Envelope panel interaction remains the mature authority; future reduced
persistent topology must replace cube storage for **both** families together,
while retaining logical edit/undo APIs and the shared rasterization authority.

Deletion review: Cycle 2's forced Yellow=0 note-on interface, Red/Blue-only
configuration fields, unconditional Yellow pole pairing, and duplicate Envelope
plane painter are removed. Cycle 1's two-value materializer forwarding remains
as an explicit compatibility API. Component guide Time slots retain their
established meaning; no Vertex::Time rename was introduced.


## Follow-up: predictable reduction and editor continuity

Use the group label **Curve morphing**, with coloured axis ticks on each pill.
Trimesh reduction follows YRB -> YR -> Y -> Static. Preserve the current primary
axis while it remains active; fall back to Yellow only when that axis is dismissed.
Do not move surviving morph rails, gain, or vertex parameter rows as modes change.
In 2D show a plane with the cube's colour blending, selected geometry, linked
corner highlights, projected cursor lines, and axis rails. Keep the cube in 3D.
Use the established curve panel snapshot host for compact Static Trimesh previews,
removing the separate glow/trace painter. Host adapters translate lifecycle only.

Hovering an option that dismisses axes warns that the current position will be
baked and dismissed variation discarded, with ordinary Undo available. No modal
confirmation. Format 9 promotes format-8 Red/Blue 2D Trimeshes to 3D so their
existing variation is preserved; new 2D edits use Yellow/Red. Envelope migration
and playback time stay unchanged.

Follow-up acceptance: surviving controls have identical bounds in every mode;
Yellow primary and key-scale orientation stay stable through the descending sweep;
projection retains Yellow/Red variation; preview uses shared panel snapshots;
mode-specific diagram, ticks, warning hover, pointer/keyboard and undo are verified
at normal and compact widths.

Follow-up proof:

- 63 focused editor, dimensionality, mapping and runtime cases / 2,736 assertions
  pass; 67 converter cases pass. CycleV2 and tests build with `--parallel 10`.
- Both the original 55-command fixture and the 44-command refinement fixture
  pass. Surviving control bounds match across every mode; Yellow stays primary.
- Native morph drag and keyboard shortcuts pass for Trimesh and Envelope. Native
  checks caught and fixed rejected Envelope control publications caused by the
  omitted dimension setting. Keyboard tests now author their required dimensions
  explicitly and check durable values, retained dimensionality and Undo.
- Shared CurvePanelHost replaces Trimesh's duplicate 2D GL lifecycle and compact
  glow painter. Trimesh retains its gesture callbacks through a typed deletion
  hook; 3D lifecycle stays in TrimeshPanelHosts. No domain algorithm was copied.
- Shared selector options own ticks and dismissal text; JUCE BubbleMessageComponent
  owns callout presentation, with the established keyboard-help pointer polling.
  Shared axis policy owns both YR Trimesh and RB Envelope planes.
- Code-map, whitespace, architecture audit and style review pass. Large-file
  responsibilities remain as above; NodePreviewRenderer only dispatches the new
  Static preview to the existing shared snapshot host.
- Evidence: `.context/morph-refinement-{regression,converter,native-smoke}.log`,
  `morph-refinement-ui-run.json`, `dimensions-ui-regression-run.json`, and native
  screenshots `morph-refinement-native-{3,2,2-compact,compact-preview,warning}.png`.
  Warning text is visibly rendered by the JUCE callout in the native capture.
- Native point dragging passes through the shared curve host. The extended
  with-spies capture scenario has intermittent fast-drag delivery, recorded in
  ui-bugs; one complete run passed drag, Undo and captures. This does not establish
  broad native-suite timing reliability.

## Follow-up: persistent properties and orthographic reduced diagrams

Keep seven stable property slots in both editors: Yellow, Red, Blue, Phase,
Amplitude, Curve, Component. Coordinate properties outside the active dimensions are
visible and disabled, including their inapplicable guide controls. Component is
an independent, always-applicable guide selector using the existing Time-slot
component-guide semantics. Yellow does not duplicate that selector.

Use the existing shared side-panel painter and MorphControlLayout for both
families. 2D is a front-facing square, with crisp offset and extended axis rails,
blended fill, selected authored geometry and cursor projections. 1D occupies the
same diagram slot with a Yellow line, cursor and linked/selected endpoint state.
No control changes position as dimensions change. Static uses a smaller editor,
collapsing the unused lower row and placing the editable curve beside the property
stack. Preserve the header origin and inspector
control coordinates through a factory-owned layout adjustment. Canonical panel
bounds remain shared by painting, input and automation.

Ownership: domain models retain coordinate and guide data; presentation exposes
applicability without changing stored geometry or input eligibility. Hit routing
must reject disabled properties and keep Component guide gestures available.
Delete the Static-only component alias and dimension-based property erasure.
Verify property persistence, rejection, guide routing, diagrams, normal/compact
captures, app build and focused tests before completing this slice.

Follow-up completion proof:

- CycleV2 and CycleV2_tests build with `--parallel 10`; 45 focused cases / 2,632
  assertions pass. The fresh 85-command fixture covers all four modes in both
  families, including all seven property slots and their applicability.
- Native macOS input verifies descending modes, unchanged mesh/revision after a
  disabled Blue click, Yellow link toggling, Component menu access and a real
  point drag in the compact Static editor. Production-size native captures cover
  normal and compact windows. Evidence: `.context/morph-properties-native-review.log`,
  `morph-properties-native-{2,1,1-unlinked,static,static-compact}.png`,
  `morph-properties-ui-run.json`, and `morph-properties-tests.log`.
- Removed dimension-based property erasure and the Static-only Component alias.
  The shared side-panel renderer paints the seven stable slots for both families;
  disabled input is rejected before routing. The reduced preview replaces the
  old plane painter and adds the one-axis line. 3D retains its mature cube.
- Envelope's selected-corner read-through visits only eight authoritative cube
  vertices. It translates selection for the existing shared renderer; it does
  not serialize geometry or copy interpolation/selection algorithms. The new
  factory layout hook keeps Static sizing with its editor, and both runtime
  placement and automation use the coordinator's canonical bounds.
- Current slice responsibility review (baseline -> after): EnvelopeCurvePanel
  1305 -> 1324 remains the mature interaction adapter; EnvelopeEditorComponent
  1011 -> 1028 composes domain controls and input guards; TrimeshWidget
  961 -> 957 remains its model/view adapter; side-panel painter 737 -> 745
  extracts a private row painter. AutomationInspector 1161 -> 1160 delegates
  placement rather than adding layout policy. NodeEditorHost header 359 -> 360
  adds one default-identity factory hook. No DSP, transaction, or realtime
  responsibilities moved. Style, whitespace, code-map and architecture checks pass.
- A broader Trimesh filter still hits known empty-fixture failures and SIGSEGV;
  the old lower-right-cube layout assertion is also stale. Reproduction is
  recorded in `ui-bugs.md` and `.context/morph-properties-tests-broad.log`.
  Focused success is not a claim of full-suite success.

## Follow-up: slider travel is a layout constraint

Reserve 140–220 px of visible vertex slider travel after label/guide/gain costs.
The shared property-control design system's 140 px minimum takes precedence over
surface width, empty space and percentage-based column balance. Maintain the
same anchors through dimension changes and retain the smaller Static editor.
For normalized vertex values this gives at least 1.4 px per 1% adjustment;
the layout does not promise per-pixel thousandth resolution.

Ownership: PropertyControls owns the shared role metrics; MorphControlLayout
allocates the two columns from those metrics. TrimeshWidget gives that control
group its required width before assigning the adjacent surface. Painting and
routed input continue to share the side-panel renderer's row geometry. Remove
the fixed percentage split that underfunded compact controls and the unrelated
two-column threshold that could split a usable property column into tiny tracks.
No gesture mapping, domain edit, undo or audio behavior changes.

Acceptance: geometry assertions for actual travel in all four modes at normal,
compact and narrower editor widths; endpoint and intermediate native vertex edits
with Undo; production-size normal/compact screenshots; focused tests, CycleV2
build with `--parallel 10`, architecture, code-map and style review.

Completion proof:

- Shared role metrics replace the renderer's local label/guide/gain widths.
  MorphControlLayout reserves the property column and a 16 px group gap before
  allocating the morph column; TrimeshWidget reserves their combined minimum
  before the surface. The independent 480 px two-column threshold is gone.
  Static retains its compact height and controls stay anchored across all modes.
- 46 focused cases / 3,323 assertions pass, including actual hit-region travel
  in every mode at 740, 890 and 1,400 px editor widths. Envelope row travel is
  verified through the same renderer and shared layout. The 85-command UI
  fixture passes for both node families. CycleV2 and tests build with
  `--parallel 10`; architecture, code-map, style and whitespace checks pass.
- Native macOS input confirms a 140 px amplitude track at both reviewed window
  sizes with identical bounds through 3D, 2D, 1D and Static. Drags to 0, 0.25,
  0.73 and 1 change the mesh correctly; native Cmd-Z restores the original mesh
  after each edit. Normal/compact Static and 2D captures and compact Envelope
  capture reviewed at production size. Evidence: `.context/vertex-travel-tests.log`,
  `vertex-travel-ui-run.json`, `vertex-travel-native.log`, and
  `vertex-travel-{normal,compact}-{two,static-verified}.png`.
- One native attempt stopped before Undo because exact-PID foreground activation
  failed. A fresh complete run passed; the intermittent automation failure is
  recorded in `ui-bugs.md` with its separate log.
- Responsibility review: TrimeshWidget 957 -> 958 lines remains the domain
  model/view adapter and delegates control width to shared layout. Side-panel
  painter 745 -> 739 lines consumes shared geometry metrics. MorphControlLayout
  66 -> 80 lines owns allocation; PropertyControls header 219 -> 233 owns role
  tokens. No new interaction, transaction, DSP or invalidation responsibilities.
  The fixed-split and local-width deletion targets are complete.

## Follow-up: Component first and a segment glyph

Put Component curve in the first row above Yellow and the remaining vertex
properties in both families. Retain the same seven row slots, width budgets,
guide routing and mode stability. Render a dedicated vector glyph beside its
label: two bright intercept dots with a blue wave-shaped segment between them,
distinct from the existing Guide resource icon in the selector.

Use the existing cached NodeIconRenderer/resource pipeline. Glyph canvas is
32 × 20 px inside the existing row, with an 8 px label gap; SVG viewBox 48 × 30,
endpoint centres at x=6/42 and a 2.4-unit wave stroke. No new painter or cache
owner. The shared side-panel renderer owns row order and presentation; domain
adapters expose Component first without changing stored Time-slot guide data.
Delete the interposed Component slot. Verify routed selector placement,
unchanged minimum slider travel, parseable glyph, normal/compact captures and
native menu interaction before completing this slice.

Completion proof:

- Both presentation lists and the shared slot authority put Component first;
  routed Trimesh control bounds prove its selector sits above Yellow in all
  modes. The six coordinate/property sliders retain their 140–220 px travel.
- The new `componentCurve.svg` uses the existing generated resource list and
  cached NodeIconRenderer, with the shared Guide glyph kept in the menu control.
  Parse/render coverage checks both glyphs at production sizes. Normal/compact
  Trimesh and compact Envelope captures show readable dots, wave and label.
- 47 focused cases / 3,331 assertions and the updated 85-command fixture pass.
  CycleV2 and tests build with `--parallel 10`. Native clicks open both Component
  menus at the top row; screenshots visibly show the Guide options. Automation
  omits empty Yellow guide rectangles to avoid duplicate Time selector targets.
  Native target inspection proves the Envelope Component selector is unique.
- Evidence: `.context/component-curve-{tests,native}.log`,
  `component-curve-ui-run.json`, `component-curve-{normal-3d,normal-static,compact-static,compact-envelope}.png`,
  and `component-curve-{trimesh,envelope}-menu.png`. Static capture preparation
  asserts its selected mode; native acceptance covers the relocated selectors.
- Architecture/style/code-map/whitespace checks pass. EnvelopeCurvePanel
  1324 -> 1324 and TrimeshWidget 958 -> 958 remain domain presentation adapters;
  side-panel painter 739 -> 743 owns shared slot/icon presentation; automation
  inspector 1160 -> 1163 filters empty geometry only. No domain, guide-mapping,
  transaction or DSP ownership changed. The interposed slot is removed.

## Follow-up: complete inspector and mapping usability

Component now belongs at the bottom of the seven-row vertex list, superseding
the preceding first-row decision. Place output Gain/Range/Width
beneath the properties in that same inspector column in every mode. Static keeps its smaller frame.
Preserve 140–220 px vertex travel and the existing semantic gesture/undo owners.

Use shared property track/indicator painting instead of scanline slider bodies.
Keep disabled labels readable, name Guide/Gain columns, and expose exact vertex
and guide-gain values on hover/drag. Domain adapters track presentation focus;
the shared side-panel painter owns feedback. Reuse existing JUCE tooltip and
property-control facilities. Morph diagrams use a common Morph space heading,
axis letters and distinct cursor/selected endpoint marks at production size.

Modulation source rows compose NodePropertySliderRow for conditional Constant/MIDI
CC values below their selectors. Reserve compact numeric-entry and track space;
both profiles remain independent and publish through NodeEditorCommands. Delete
the zero-width residual layout and raw duplicate slider presentation. Clarify
Unmapped fallback in hover help. No domain algorithms or realtime paths change.

Verify ordering, inspector containment, minimum travel and mode anchors;
conditional controls and value/CC edits in both profiles with Undo; exact hover
and drag feedback; diagram states; normal/compact native captures and gestures;
focused tests, app build, architecture, style, code-map and whitespace checks.

Completion proof:

- Component is last in both presentation lists and the shared slot authority.
  Output Gain/Range/Width uses that inspector column, with a named Output group;
  Static gives the former footer space back to its editable curve. Coordinate
  slots stay stable and disabled, with 140–220 px of actual vertex travel.
- Shared property painting replaces the scanline bodies. Guide/Gain headers,
  readable disabled labels, exact hover/drag readouts and availability tooltips
  clarify each control. Cube, plane and rail share endpoint/selection primitives
  and a diamond cursor, with axis letters and the same Morph space heading.
- Conditional source controls use the existing NodePropertySliderRow and
  NodePropertyControlBinding transaction, keyboard and numeric-entry owners.
  Both mapping profiles retain their own values. Each profile pair reserves
  extra height only when either source needs it; default rows stay compact.
  No raw slider callbacks or zero-width residual controls remain.
- Native macOS verification passed from a fresh process at normal and compact
  widths: all morph modes, first-hover feedback, held vertex drag, output gain,
  Component menu and guide-gain adjustment with Undo, Envelope hover, Constant
  drag/Undo and MIDI CC direct numeric entry/Undo. The first guide-gain Undo
  assertion queried before UI publication; waiting for the restored state
  passed. Evidence: `.context/inspector-usability-native.log`, run `3d68c3efa87d`,
  and `inspector-usability-{compact-static,normal-three,normal-two,normal-one,vertex-drag,guide-gain-hover,envelope-hover,modulation-verified}.png`.
- A fresh native hover initially exposed the OpenGL peer's missed-enter behavior.
  PropertyHoverTracker now reconciles local pointer position using JUCE Timer
  at 30 Hz only for showing editors in the foreground process. It supplies
  presentation feedback; semantic control events and transaction ownership
  remain unchanged. Each pass scans a fixed inspector list and repaints only
  when feedback changes. Drag handlers suppress reconciliation. No mesh, graph
  serialization, audio preparation or realtime work is introduced.

- CycleV2 and tests build with `--parallel 10`. All 48 focused cases / 3,459
  assertions pass. The 85-command morph-properties fixture passes (run
  `ad780decb7c6`), including both families and mode stability. The new 32-command
  conditional-modulation fixture passes (run `40e7d65f3a4f`), checking both
  profiles, source changes, independent values and Undo. Architecture, code-map,
  style and whitespace review pass. Logs and fixture reports are under
  `.context/inspector-usability-*`. This focused proof does not claim resolution
  of the broader suite's previously recorded fixture and native-timing debt.

Responsibility and size review:

- EnvelopeCurvePanel 1324 → 1324 remains its domain editor/model adapter; only
  presentation order changes. Its existing collaborators retain geometry,
  marker and gesture policy. TrimeshWidget 958 → 958 remains the model/view
  adapter and exposes feedback only. No new domain ownership enters either.
- EnvelopeEditorComponent 1028 → 1069 and TrimeshControlsComponent 622 → 691
  retain focus, tooltip and routed-control composition. Shared painter and
  hover tracker own reused mechanics; dispatcher/delegates still own edits.
  Side-panel renderer 743 → 762 retains shared row geometry and painting.
- NodeCanvasAutomationInspector 1163 → 1206 stays a cohesive read-only export
  facade: the added functions translate bounds already owned by editors into
  pointer targets. They neither calculate layout nor introduce eligibility,
  edit or lifecycle policy. An extraction is unnecessary for these small
  geometry translations; keep future domain decisions out of this facade.
- ModulationNodeEditors 316 → 341 composes mature property controls rather than
  duplicating gesture lifecycle. Shared endpoint painters and hover tracking
  are presentation-only; all old scanline and raw conditional-slider paths
  are deleted. The UI scope has no DSP or persistent-topology changes.

## Follow-up: shared editor composition and Static partials

Use Trimesh as the layout template for both mesh families: upper-left morph
surface, upper-right morph/vertex inspector, full-width editable curve below.
Static compacts to a curve beside vertex properties. Envelope retains its
panel-wide marker/scaling/zoom toolbar along the bottom. Both families use the
same popup sizing, column geometry and full-height property rows. Preserve the
140–220 px vertex travel budget and mature envelope editing/marker policies.

Extract the existing Trimesh body rectangles into MeshEditorLayout; both editor
adapters consume that authority. Remove Envelope's fixed 310 px control band,
57% curve/surface split and reduced row-height scale. The shared morph diagram
slot may grow to 210 px (from 140), bounded by its column and three rail rows.
Selected endpoints become small solid semantic accents, without oversized rings;
the diamond cursor stays distinct. No selection or link semantics change.

Static currently returns no grid columns, starving the existing spectral partial
painter. Prepare exactly one grid-equivalent column at morph zero, reusing
TrimeshGridwiseDsp, spectral pitch mapping and guide seed policy. No drawable
surface, multi-column grid or realtime work is required. The same data-source
column API continues to serve the mature partial painter. Test magnitude/phase,
pitch-spanning and fixed-pitch cases, guide seeds and model edit refresh against
a canonical 3D mesh's zero column. Existing zero-live-slice audio checks remain.

Verify actual layout/pointer bounds in all modes at normal and compact widths,
Envelope curve edits/morph/marker controls with Undo, Static partial availability
and edit refresh, and production-size captures. Build with --parallel 10; run
focused tests, UI fixtures, architecture/code-map/style/whitespace checks.

Completion evidence:

- MeshEditorLayout owns the body rectangles previously held by TrimeshWidget.
  Envelope consumes those same controls/surface/curve rectangles, reserving its
  own bottom toolbar. Both node view modules use the same scale/aspect policy;
  the Envelope factory preserves its toolbar when compacting Static. Full-width
  lower-curve and compact Static layout are asserted on actual editor automation
  bounds at 1,400/890 px widths across all four modes, with 140–220 px tracks.
- MorphControlLayout grows the shared diagram slot from 140 to at most 210 px,
  constrained by the column and rail space. Both families share a 5 px endpoint
  backplate and 3 px selected/2 px ordinary dot, replacing the 8 px selected disc
  and 7 px ring. Cursor, face blending and linked-axis semantics are unchanged.
- Static spectral render data retains one column, so canDrawSurface remains
  false while the mature partial painter receives data through its existing
  copyColumnAtMorph API. The zero column reuses GridwiseDsp's ordinary/pitch
  sampling, column-zero guide seeds and render-profile mapping. Static time
  rendering continues to omit columns. No realtime paths or allocations change.
- 50 focused cases / 5,296 assertions pass. The new partial regression compares
  magnitude/phase against canonical 3D zero-column output with a component guide,
  for fixed pitch and pitch spanning columns, and checks guide-gain refresh.
  Existing zero-live-slice runtime checks remain in this focused selection.
  CycleV2 and tests build with --parallel 10. The 85-command mode/property
  fixture passes (run 3bffbf701826); the new 10-command spectral fixture passes
  (fe22667176d8), including Static → 3D → Undo and one-column restoration.
  Evidence: `.context/shared-mesh-layout-{build,tests}.log`,
  `shared-mesh-layout-ui-run.json`, `shared-mesh-static-partials-ui-run.json`.

Responsibility review: EnvelopeEditorComponent 1069 → 1063 remains the domain
editor composition and control-event adapter; geometry now delegates to the
shared layout and its own toolbar. TrimeshWidget 958 → 940 delegates its former
body rectangle ownership to that same class. EnvelopeMorphControls 272 → 256
owns Envelope toolbar composition and uses shared morph geometry/painting;
obsolete fixed-column geometry is gone. Side-panel renderer 762 → 763 keeps
shared painting and now consumes the shared footer metric. ConcreteNodeEditors
490 → 501 adds only Envelope's compact factory adaptation. GridRenderService
128 → 130 keeps grid/slice orchestration over the mature DSP and mapping owners.
The two new layout files contain rectangle composition only; no model, gesture,
selection, transaction, interpolation, invalidation or audio policy moved there.
Deletion targets (310 px band, 57% split, reduced property-row scale, oversized
selected ring and Static spectral zero-column return) are complete.

Native completion: run 2604269b5ef1 passed real macOS Envelope vertex movement,
marker toggle and morph multi-update drags, each with Undo. Normal (1,600 × 900)
and compact (1,100 × 800) captures cover all modes, selected corners and Static
spectral partials. Capture preparation verifies modes through semantic pointer
commands; native input separately verifies the affected editing workflows.
Evidence: `.context/shared-mesh-native.log`,
`shared-mesh-{normal,compact}-envelope-selected.png`,
`shared-mesh-{normal,compact}-envelope-static.png`,
`shared-mesh-{normal,compact}-static-partials.png` and the intermediate mode captures.
The final native captures use a height that fits the physical display; an early
1,050 px request was clamped by macOS and its normal capture was discarded.
Architecture, code-map, style and whitespace checks pass. This targeted native
proof does not claim resolution of the previously recorded broader automation
input-timing debt.

## Follow-up: Envelope surface interaction parity

Hold the last prepared morph surface throughout an Envelope gesture, while the
local curve continues updating. At gesture commit, rebuild the surface once.
Use the existing CurveExpandedEditor transaction boundary for curve, parameter
and morph gestures; do not add another transaction owner or copy the mesh during
movement. Surface rasterization remains EnvelopeMorphSurfaceRasterizer's shared
Cycle 1 authority. Convert its HalfBipolar output back to normalized [0,1] with
Buffer operations before the scalar material consumes it.

Mount the existing ZoomPanel around the Envelope curve. Replace the standalone
software-only surface painter with the mature Trimesh Panel3D/ZoomPanel hosted
through CurvePanelHost, so both surface and curve expose established zoom bars,
wheel mapping, shared-canvas rendering and cache lifecycle. Domain surface code
supplies columns and commit eligibility only. Reuse scrollbar styling from the
Trimesh host through one shared presentation authority. Delete the old image
cache/painter and inner-host-only Envelope mount.

Dimension changes clear the keyboard morph axis and hint only if that axis is
no longer active. Preserve retained-axis focus in both families.

Verify held multi-update curve/vertex/morph gestures keep the surface rebuild
count stable, update the local slice, rebuild once on release and restore on
Undo. Verify endpoint/midpoint normalized surface values, physical colour output,
actual zoom bars/pan gestures, all modes and retained/dismissed focus. Review
normal/compact native captures; run focused tests, CycleV2 build --parallel 10,
architecture/code-map/style and whitespace checks. No audio behavior changes.

Completion evidence:

- CurveExpandedEditorComponent supplies its existing transaction boundaries to
  the controller's presentation lifecycle. EnvelopeMorphSurfacePreview retains
  prepared columns while that gesture is active; release requests a repaint
  and prepares the final revision once. The local Envelope rasterizer and curve
  remain live. Primary-axis cursor traversal retains its existing prepared grid.
- The surface now composes TrimeshPanel3D, CurvePanelHost and ZoomPanel. It
  supplies the mature E3 cross-sections as normalized scalar columns; shared
  rendering/cache/resource owners replace its independent CPU image painter.
  Pointer editing is disabled for this preview; established zoom/wheel behavior
  remains available. The curve mounts its existing ZoomPanel, preserving actual
  viewport bounds for pointer automation and eight-pixel bars outside content.
- ZoomPanel's existing scrollbar update block is extracted into updateScrollbars.
  Envelope framing calls it after constraining its view, synchronizing visible
  ranges without another rasterization or zoom lifecycle. Trimesh and Envelope
  now consume the same scrollbar style authority. Existing Trimesh render bounds
  remain unchanged; Envelope adapters supply their actual inner viewport.
- HalfBipolar surface output is restored with Buffer.add(0.5).clip(0,1), retaining
  the mature rasterizer's Cycle 1 request semantics. Endpoint, midpoint and
  intermediate constant-amplitude probes pass (0, 0.2, 0.5, 0.8, 1). No scalar
  sample/pixel mapping or new DSP algorithm is introduced.
- Focus tests exercise both families: Red survives 3D → 2D and disappears in
  1D; Yellow survives re-expansion and disappears in Static. Both remembered
  focus and visible shortcut hint follow those conditions. Envelope now clears
  the inactive hint; Trimesh's established behavior passes unchanged.
- All 58 focused cases / 5,488 assertions pass, including six existing Trimesh
  viewport/trackpad cases. CycleV2 and tests build with --parallel 10. The new
  22-command surface interaction fixture passes (66f767096447): real zoom target
  geometry, full scalar range, wheel zoom and retained/dismissed focus. The
  85-command mode/property fixture also passes (9932a35e503b); report:
  `.context/envelope-surface-ui-run.json`.
- Native macOS run 3e82760a206f passed two held curve updates, vertex-parameter
  and orthogonal morph drags: local values/curve change, surface rebuild count
  stays fixed until release, and commit increments it exactly once. Undo,
  both zoom hosts, native scrollbar panning and shortcut focus transitions pass.
  Normal/compact screenshots reviewed at production size. Evidence:
  `.context/envelope-surface-{build,tests,native}.log`,
  `envelope-surface-interaction-ui-run.json`, and
  `envelope-surface-{compact-before,compact-released,compact-zoomed,compact-final,normal-final}.png`.

Responsibility review:

- EnvelopeCurvePanel 1324 → 1325 retains mature envelope interaction and grid
  policy; the added call synchronizes its existing zoom owner. Envelope editor
  1063 → 1081 composes the hosted preview and releases invalid keyboard focus;
  domain geometry, transaction and rendering policy stay with collaborators.
- CurvePanelController 769 → 809 remains the existing family-controller
  composition boundary. The Envelope adapter supplies its mesh, guides and
  commit eligibility; base controller exposes host composition and repaint
  translation. No interpolation or transaction state machine moves into it.
- EnvelopeMorphSurfacePreview 51 → 161 is now a column/data-and-host adapter,
  with CurvePanelHost owning GL context resources, snapshots and invalidation,
  TrimeshPanel3D owning scalar display, and ZoomPanel owning camera interaction.
  It does not own document mutation or undo. The old software image/cache path
  is removed, rather than retained alongside the mature host.
- CurvePanelInfrastructure 496 → 555 owns the optional zoom wrapper, role-owned
  viewport/scrollbar export and shared styling. NodeCanvasAutomationInspector
  1206 → 1230 remains a cohesive read-only facade: it translates those exported
  rectangles into targets, without calculating layout or adding domain policy.
  Keep future interaction/edit policy out of this facade.
- No audio path or realtime preparation changes. Gesture movement introduces
  no mesh/graph copies or surface preparation; surface work remains outside
  audio and occurs only at the existing gesture boundary. The old live-image
  rebuild, centred scalar feed, hidden zoom wrappers and stale focus paths are
  deleted. Architecture, code-map, style and whitespace checks pass.
