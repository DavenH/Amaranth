# Cycle V2 Trimesh Link Buttons

Status: Implemented 2026-09-01

## Objective

Restore the three Trimesh Link toggles as durable, visible, undoable editor
controls. Pointer and keyboard/automation activation must toggle the intended
axis, survive editor rebind, and remain governed by the graph command boundary.

## Authoritative Implementations

- Cycle 1's `MorphPanel` stores `LinkYellow`, `LinkRed`, and `LinkBlue` as
  durable morph-editing settings and passes them to the mature Interactor link
  behavior unchanged.
- `Interactor::getVerticesToMove` is authoritative for the one-, two-, and
  three-axis linked-vertex sets. It reads the three established `Settings`
  slots and must not be copied into Cycle V2.
- Cycle V2's established design is already expressed by
  `TrimeshWidget`, `ConcreteNodeEditors`, and `NodeEditorCommandService`: the
  durable fields are `link.yellow`, `link.red`, and `link.blue`, with defaults
  `true`, `false`, and `false` respectively.
- `TrimeshControlsComponent` owns pointer routing, while
  `GraphCommandDispatcher` and `GraphDocument` own semantic mutation and undo.

The current failure is a schema omission. Every consumer and command uses the
three parameter IDs, but the Trilinear Mesh `NodeDefinition` does not declare
them. `GraphEditor::setNodeParameter` therefore returns `UnknownParameter` and
the editor never receives a changed node to display.

## Design

Declare the three boolean parameters on the canonical Trilinear Mesh node with
`ParameterImpact::Presentation` only. They affect mesh-editing interaction and
editor presentation; they do not directly change DSP configuration, preview
products, graph topology, or the mesh model.

Canonical normalization adds the defaults to older saved graphs. Existing
Cycle V2 consumers continue reading the same IDs and defaults. No adapter,
model migration, dynamic-parameter exception, or duplicate link state is
introduced outside the existing mature-panel compatibility boundary.

`TrimeshPanelEnvironment` is the existing compatibility boundary around the
mature panel cores. During node sync it translates the three durable booleans
into that bridge instance's `Settings` slots. The values and linking algorithm
are reused unchanged; only ownership changes from Cycle 1's document-wide
Morph panel to Cycle V2's per-node editor. This narrow translation is the
stable end state because each Cycle V2 Trimesh widget intentionally owns an
isolated mature-panel environment.

## Test-First Contract

1. Canonical Trilinear Mesh creation and normalization contain all three link
   parameters with the established defaults.
2. The real `TrimeshControlsComponent` link hit region dispatches its axis to
   the editor delegate.
3. A command-service sequence toggles off-to-on and on-to-off, rebinds a hosted
   editor after each edit, observes the selected visual state, then undoes and
   observes the restored state.
4. The bridge forwards link parameters to `Interactor::getVerticesToMove`,
   producing 2, 4, and 8 moving vertices for one, two, and three enabled axes.
5. The existing production pointer-target replay clicks an expanded Link
   button and asserts the durable graph parameter.

## Negative Boundaries

- Do not allow arbitrary dynamic Trimesh parameters to bypass the schema.
- Do not store a second copy in `TrimeshNodeModelState`, local component state,
  or the OpenGL panel bridge.
- Do not mutate `NodeGraph`, publish document changes, or manage undo from the
  component/editor layer.
- Do not mark Link changes as DSP or Preview impacts; resulting mesh edits
  already publish through their authoritative model path.
- Do not alter mature Interactor linking behavior in this repair.

## Completion Criteria

- All three buttons toggle through pointer targets and semantic commands.
- The selected state survives rebind and undo restores both graph and visible
  state.
- Old nodes normalize to the established defaults without losing other data.
- Focused unit and production automation pass, followed by Standalone Debug,
  the Cycle V2 suite, `git diff --check`, style review, and diff review.

## Implementation Evidence

- The Trilinear Mesh definition now declares the three persisted presentation
  booleans, and canonical normalization and shipped graphs carry the established
  `true`, `false`, `false` defaults.
- The existing panel-environment boundary forwards those values into the mature
  `Interactor` settings. Focused tests observe 2, 4, and 8 moving vertices as
  one, two, and three axes are linked.
- Pointer targets and Space/Return activation use the same semantic delegate.
  A hosted-editor sequence proves off-to-on, on-to-off, rebind, and undo with
  the visible selected state following the durable graph value.
- `cycle-v2-agent-pointer-target-replay.json` passes against the production
  Standalone build; its report is
  `/private/tmp/cycle-v2-trimesh-link-report.json`.
- All focused tests pass. The full Cycle V2 suite passes 10,665 of 10,666
  assertions; the sole failure is the pre-existing edge-hover help regression
  in `TestNodeCanvasHitRouter.cpp:66`.
