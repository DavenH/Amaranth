# Cycle V2 Trimesh Guide Gain Controls

Status: Implemented

## Objective

Restore Cycle 1's per-vertex-property guide-gain affordance in the Cycle V2
Trimesh editor. At production width, place the six vertex parameters in two
top-row columns, then pair morph and spectral-range controls with the cube in
the lower row.
Remove the meaningless Spectral layer divider text and align the Range/Width
label with its slider rail. Make a second click on an open Guide selector
dismiss its menu.

## Authoritative Implementation

- `VertCube::guideCurveGains` is the durable gain model and
  `VertCube::guideCurveAbsGain` is the mature decibel mapping already consumed
  by guide rasterization and DSP. No second gain model or DSP mapping is added.
- Cycle 1 `VertexPropertiesPanel` establishes the 0..1 knob range, unity at
  0.5, and -30..+30 dB display meaning.
- `TrimeshSidePanelRenderer` remains the sole source of visible and interactive
  control geometry. `TrimeshWidget::expandedControlHitRegions` delegates to it.
- `NodeEditorCommandService` and `GraphCommandDispatcher` retain transient
  publication, commit, refresh, and undo ownership.

## Geometry And Interaction Contract

- The production Trimesh side panel uses a full-width vertex-parameter row
  followed by a lower controls row. Vertex parameters are ordered
  time/red/blue in the left column and phase/amp/curve in the right column.
- Morph/range controls occupy the lower-left cell and the cube occupies the
  lower-right cell, keeping the related morph affordances on one row.
- The upper 3D surface and control area share the editor width evenly so the
  additional Guide/gain controls remain legible at production size.
- The compact stacked fallback is used only when two columns cannot retain
  usable controls; it is not used at the production editor width.
- Phase, amp, and curve rows contain, in order, label, parameter rail, Guide
  assignment target, and guide-gain knob. Time, red, and blue use the reclaimed
  row width for their parameter rails and do not expose deformer controls.
  Knobs have a visible rotary indicator and a larger row-height hit target.
- Guide gain uses a relative vertical drag with at least 120 px of virtual
  travel across the 0..1 range. Arrow keys provide 0.01 steps. The knob is
  visibly disabled when that field has no guide attachment.
- The Range/Width label centre matches the range rail centre. The section rule
  above it has no text.
- The active Guide selector owns one popup identity. Clicking that same
  selector again dismisses the popup; switching selectors dismisses the old
  popup before opening the new one.

## Semantic Contract

- A gain gesture captures one durable base revision, publishes at least two
  transient model snapshots, commits once, refreshes guide-dependent output,
  and undoes as one transaction. The widget acknowledges the committed model
  identity so undo cannot leave a stale locally mutated gain visible.
- Editing the selected vertex's guide gain applies the mature relative gain
  delta to every owning cube targeted by that vertex-field attachment, while
  preserving any pre-existing differences until a value clips at a limit.
- The vertex panel does not conflate the internally stored time-dimension
  component-curve attachment with a deformer on the Time property.
- Serialization, guide preparation, blockwise/gridwise DSP, and decibel mapping
  remain unchanged.
- Transient mesh publication during a point/curve drag must not resynchronize
  the panel bridge from its own published snapshot or clear the active
  interaction pointers. Rebinding resumes when the gesture completes.

## Negative Boundaries

- Do not introduce a node parameter or parallel guide-gain store.
- Do not reimplement guide deformation or gain conversion in Cycle V2.
- Do not make unattached gain knobs editable.
- Do not add separate paint, hit-test, and automation layout calculations.
- Do not change Envelope controls; they do not expose Trimesh Guide assignment.

## Completion Criteria

- Reference and compact geometry tests cover column selection, non-overlap,
  gain placement, minimum rail travel, and Range/Width alignment.
- A complete gain gesture test covers two updates, commit, downstream render
  effect, and undo.
- The native Trimesh edit sequence completes curve reshaping and repeated
  point dragging across intermediate publications.
- Focused automation exposes and exercises the gain target with a selected
  attached vertex.
- Production-size before/after captures demonstrate the intended hierarchy.
- Standalone Debug and focused tests pass; style, production diff, hot-loop,
  and `git diff --check` reviews are complete.

## Implementation Evidence

- Geometry coverage passes with 46 assertions across seven focused tests; the
  broader Trimesh node set passes 1,905 assertions across 53 tests.
- Editor command coverage passes 89 assertions across seven Trimesh editor
  tests, including two transient gain updates, commit, prepared guide state,
  undo, and same-selector popup dismissal.
- `cycle-v2-agent-trimesh-guide-gain.json` completes all 20 commands against a
  selected Phase attachment, including drag, undo, reopen, toggle dismissal,
  and screenshot capture.
- `test_cycle_v2_native_edit_smoke.py trimesh-point-drag` passes a real native
  point drag across intermediate mesh publication.
- Standalone Debug builds successfully. The final production capture is
  `/private/tmp/cycle-v2-trimesh-guide-gain.png`.
