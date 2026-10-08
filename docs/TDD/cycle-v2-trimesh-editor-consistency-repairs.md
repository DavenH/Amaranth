# Cycle V2 Trimesh Editor Consistency Repairs

Status: In progress (2026-10-07)

## Reported behavior

- A vertex created by right click appears at the pointer briefly, then moves to a distant corner.
- Selecting a vertex makes its property controls alternate among different values.
- The Curve property slider can show zero for one corner of a VertCube while the component guide curve remains visible because its other seven corners have nonzero Curve values.
- The 3D guide rails and the grid beneath them can disagree when a Scratch envelope is assigned.

## Authoritative behavior and ownership

- Shared `Interactor2D` and `Interactor3D` own vertex creation, selection, movement, and collision semantics. The Cycle V2 bridge translates interaction events into one graph command gesture; it does not implement another mesh interaction algorithm.
- `TrimeshNodeModelState` is the durable topology. `TrimeshPanelBridge` owns the live panel mesh and selection. `NodeEditorCommandService` owns publication, commit, and undo. A panel gesture retains live mesh identity until its final publication.
- `VertCube` owns the eight corner vertices and mature `WaveformBakePolicy::bakeGuideCurve` multiplies the component guide by the interpolated Curve value and the cube's Time guide gain. Cycle V2's Curve property currently shows and edits one selected corner. The cube displayed beside the controls is the correct local edit target for a cube-wide Curve average. The panel model and edit core should use that same cube identity.
- `TrimeshGuidePreparation` prepares the mesh and guide provider for both the 3D rail rasterizer and the grid DSP. A displayed rail and corresponding grid column must use the same prepared guide data, domain, morph position, and visualization seed.

## Interaction and render contract

- Creation at a 2D panel point preserves the authored phase and amplitude through pointer-up, graph publication, guide preparation, and editor repaint. Its selected vertex refers to that authored topology.
- Selection changes only once per completed click. Hover and rebinds do not replace the selected vertex or temporarily show values for a different vertex. Controls repaint from the same selected topology snapshot.
- The Curve slider displays the arithmetic mean of the selected cube's eight corner Curve values. An edit changes those corners and reaches the requested average, including exact zero and one. The mature component-guide bake then has zero guide contribution if all eight Curve values are zero. Undo restores each prior value.
- Guide-assigned 3D rails match grid columns at their corresponding morph position, including Scratch envelope assignments. Preserve intentional per-column variation only where the rail depicts the same variation.

## Complexity and boundaries

- Pointer down and click commit may visit the edited vertex and its owners, plus the local render product. Selection and movement updates must not clone an unrelated mesh, serialize editor state, or rebuild unrelated graph products.
- Reuse mature interaction and rasterization behavior. Do not copy guide evaluation, Scratch envelope semantics, or mesh traversal into the bridge.
- Keep the renderer's seed policy at one authoritative boundary; the UI must not independently approximate DSP output.

## Proof and completion

- Reproduce each reported behavior with a focused native or in-app fixture. Record the selected vertices, authored coordinates, Curve property values, and rail/grid samples before and after publication where applicable.
- Add focused semantic tests for an eight-corner Curve average, editing to zero and one, rendering the mature component guide, and undo; use event-sequence and operation-count tests for click selection and creation.
- Verify the guide rail/grid agreement on a graph with a Scratch envelope, using the actual prepared guide provider and a representative column.
- Run focused tests, a Standalone Debug build, the architecture audit, style and diff review, and `git diff --check`; commit each complete repair slice.

## Investigation notes

- The user clarified that the Curve case concerns one VertCube's eight corners, not a multi-vertex box selection. The Curve value is also the component-guide multiplier in the screenshot's assigned Time guide; guide gain is a separate control.
- The 3D panel rasterizer uses one visualization seed; `TrimeshGridwiseDsp` adds a column-specific seed offset. This is a candidate explanation for rail/grid divergence and needs direct sample proof.
- The 2D creation and selection callbacks can both occur on pointer-down. Publication ordering and the subsequent prepared-guide mesh replacement need an end-to-end reproduction before changing the bridge.
- Initial pointer automation inherited the host's Shift and Command modifiers and performed box selection. Cycle V2 automation now starts each pointer event with no modifiers and adds only those requested by the fixture.
- A true right click on `organ-3.cyclegraph` `magnitudeLayer3` at normalized panel position `(0.8, 0.7)` created a new cube whose displayed intercept moved from authored phase `0.8014` to `0.7465` after commit. `Interactor::addNewCubeForMultipleIntercepts` used a prepared/rendered neighbor intercept to offset raw copied vertices. The resulting cube did not necessarily render at the click phase. The shared interactor now measures its own resulting intercept and translates its eight Phase values by the residual. The in-app fixture `cycle-v2-agent-trimesh-guided-vertex-insertion.json` verifies a final phase within `0.79–0.81`.
- This repair leaves interpolation, wrapping, collision validation, and mesh ownership in the mature shared interactor. `Interactor.cpp` is already 2,548 lines before this eight-line correction; its responsibility remains interaction and cube construction, while `CycleV2AutomationInput.cpp` only translates fixture input. The correction does one fixed-size reduction and visits eight corners. Four focused Trimesh CTest cases pass. The broad 1,277-case suite was stopped after 59 passing cases because the focused interaction and editor checks plus the live fixture cover this slice.
- `esurience-3.cyclegraph` is the reported graph; `magnitudeLayer2` is the pictured node. It has a local `scratchEnvelope1` attachment and three cubes. A direct in-app right click at normalized 2D position `(0.4, 0.3)` produced selected phase `0.3961`, amplitude `0.7025`, and a new intercept at `0.3961` after publication, so this position does not reproduce the corner jump. The report is `/private/tmp/esurience-3-trimesh-add-report.json`.
- The same graph has guide noise set to zero. Column-specific noise seeds alone cannot explain its rail/grid difference. Expanded grid rendering calls `TrimeshGridRenderService` and the 3D rail calls shared `Panel3D::drawInterceptLines`; the graph Scratch attachment is consumed by runtime morph resolution and is not an input to either of those editor render paths. The precise discrepant render product still needs measurement.

## Curve average repair (complete slice)

- The eight-corner edit lives in `TrimeshVertexEditCore`; `TrimeshNodeModel` selects the same first owner cube as the existing cube preview. The widget exposes the selected parameters to both the renderer and automation. `NodeEditorCommandService` retains the eight starting values for one gesture and publishes one durable edit on commit.
- The edit shifts all eight values by a common offset with clipping, solving for the requested mean. Endpoints force every corner exactly to zero or one. This preserves relative corner differences at intermediate values where possible. The live update visits only eight corners and does not copy the mesh or graph. Undo restores each original corner.
- The focused model and command tests pass, including two movement updates, commit, and undo with unchanged graph and mesh copy counters. The `organ-3.cyclegraph` `magnitudeLayer3` automation fixture confirms the displayed average, eight-corner zero result, and undo in the running app. The mature component-guide bake was inspected; its Curve multiplier is zero when all eight corners are zero. A direct rendered-guide assertion remains part of the overall completion criteria.
- Review: `TrimeshNodeModel` owns cube selection and values; `TrimeshVertexEditCore` owns the shift algorithm; `NodeEditorCommandService` owns the graph transaction. `TrimeshWidget` and `ConcreteNodeEditors` only translate the selected state. The command service and widget were already above the 800-line review threshold and grew by approximately 20 and 5 lines; they remain orchestration and presentation respectively. No second Curve-edit policy was added in either layer.
