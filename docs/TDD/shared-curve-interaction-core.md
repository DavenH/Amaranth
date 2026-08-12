# Shared Curve Interaction Core TDD

Status: Implemented

## Problem

Cycle v1, Cycle v2 flat-curve editors, the Cycle v2 Envelope editor, and the
Cycle v2 Trimesh 2D editor all expose the same curve interaction:

- detect a pointer near a rendered curve;
- show the curve-reshape cursor;
- begin one reshape gesture;
- move the rendered curve with the pointer by changing curve sharpness;
- publish intermediate edits and commit or cancel the gesture.

They do not currently share that complete behavior. `Interactor2D` contains the
mature Cycle v1 interaction sequence, while the Cycle v2 flat and Envelope
panels carry local copies of most of `doReshapeCurve`. Trimesh uses the shared
implementation, but padded rasterizer curves return before initializing
`Curve::tp.ypole`. The shared implementation therefore multiplies Trimesh drag
movement by zero. Replacing that zero with a client-specific sign makes the
curve editable but can make the rendered curve move opposite the pointer.

That is a boundary mismatch, not a legitimate Trimesh interaction policy.
There must be no per-editor gesture-polarity policy.

## Authoritative Implementations

- `lib/src/Inter/Interactor2D.cpp` is authoritative for 2D curve hit testing,
  selection framing, moving-vertex resolution, sharpness mutation, listener
  notification, and interaction state.
- `lib/src/Inter/Interactor.cpp` is authoritative for mouse gesture lifecycle,
  action selection, pointer-to-panel coordinate conversion, and drag capture.
- `lib/src/Curve/Curve.cpp` and the rasterizer snapshot are authoritative for
  curve evaluation. `TransformParameters::ypole` is raster transform metadata;
  it must not become a client-specific input-gesture policy.
- JUCE component targeting is authoritative for enter, exit, move, drag
  capture, and choosing the cursor of the component under the pointer.
- Existing Cycle v2 command dispatchers remain authoritative for transient
  publication, commit, cancellation, and undo. They do not own hit testing or
  curve math.

The Cycle v2 implementations in
`Nodes/Effect2D/FlatCurvePanels.cpp`,
`Nodes/Effect2D/EnvelopeCurvePanel.cpp`, and
`Nodes/Trimesh/TrimeshInteractor2D.cpp` are clients to migrate, not separate
authorities.

## Product Contract

For every 2D curve editor:

1. A pointer within the shared curve threshold enters the shared reshape-hover
   state and the receiving panel host exposes `UpDownResizeCursor`.
2. Moving away clears that state and restores the normal panel cursor without
   polling or forcing the global mouse source.
3. Mouse-down on the hovered curve starts `ReshapingCurve` and retains JUCE
   drag capture until mouse-up, including outside the original bounds.
4. For an unclamped edit, the rendered point under the initial pointer follows
   the pointer's vertical direction. An upward drag moves that point upward; a
   downward drag moves it downward.
5. Dragging in the gesture-start direction toward the curve's controlling
   intercept increases sharpness and continues increasing after the pointer
   crosses that intercept, saturating at full sharpness. Reversing direction
   during one gesture decreases sharpness without discontinuity.
6. Every drag update mutates the domain-selected vertices once, emits one
   consolidated transient edit, and repaints from the resulting state.
7. Mouse-up commits one undoable edit. Undo restores the exact pre-gesture
   model and visible curve.

The contract is expressed in one canonical panel coordinate system. A client
must not repair an inverted result with a sign override.

## Required Diagnostic Gate

Before changing the algorithm, record one curve gesture from Cycle v1,
flat/Guide Curve, Envelope, and Trimesh with:

- current curve and intercept indices;
- `Curve::a`, `Curve::b`, and `Curve::c`, including padding flags and cube
  ownership;
- `state.currentCube` and `state.currentVertex`;
- pointer start/current positions after panel coordinate conversion;
- the selected control vertex or reduced intercept position;
- sharpness before and after one small drag;
- the rendered curve point before and after that drag.

The gate must identify why Trimesh associates the hovered rasterized segment
with the opposite or wrong control geometry. Likely candidates are curve-index
padding, `setExtraElements` choosing a vertex from the wrong side of the cube,
or a mismatch between the reduced intercept and retained vertex. Fix that
association or representation translation. Do not add a Trimesh polarity
branch.

## Diagnostic Result

The retained event traces established two independent association defects:

- Copying a prepared `Curve` into `RasterizerSnapshot` discarded its transform
  parameters and sampled transform arrays. Trimesh therefore received
  `tp.ypole == 0` and `tp.scaleY == 0` even though rasterization had prepared
  non-zero geometry.
- Choosing `CurrentCurve` from the nearest intercept associates a pointer on
  the left half of a segment with the previous control. The waveform bake
  already records the exact `Curve::waveIdx` at which each rendered curve
  starts, so the hit segment can resolve its authoritative owning curve
  directly without inferring ownership from pointer position.

The shared implementation now preserves the prepared snapshot geometry and
uses the hit waveform sample to resolve the curve owner. Flat curves retain
their vertex-backed translation because their compact rasterizer does not
publish increasing `waveIdx` boundaries. No editor-specific polarity is used.

## Target Design

`Interactor2D` owns one non-overridden reshape template:

1. validate the rasterizer snapshot and current curve;
2. resolve the controlling curve geometry in canonical panel/model space;
3. update selection framing;
4. ask the existing domain hook for the vertices affected by the edit;
5. calculate one signed sharpness delta from pointer movement and the shared
   curve/control relationship;
6. constrain and mutate the affected `Vertex::Curve` values;
7. notify selection listeners and mark the mesh changed only when a retained
   value actually changed.

Existing domain variation may remain behind existing domain hooks:

- `getVerticesToMove` may apply Envelope link topology or hidden-dimension
  selection.
- A narrow post-edit hook may synchronize Envelope loop/sustain seams.
- The Cycle v2 adapter may publish the completed shared edit through its
  command dispatcher.

Those hooks may choose affected state or translate lifecycle. They may not
calculate pointer polarity, duplicate reshape math, perform curve hit testing,
or independently decide cursor state.

Cycle v1 and all Cycle v2 curve panels call the same `Interactor2D` reshape
implementation. If the current hidden-dimension movement scale differs between
clients, determine the canonical rule during the diagnostic gate and encode it
once in the shared implementation; do not preserve an accidental difference
with a curve-editor override.

## Host And Cursor Boundary

The JUCE component receiving panel events forwards its already-local event to
the shared Interactor. `Panel::setCursor` may update that receiving component's
cursor. Parent editors and `NodeCanvas` must not inspect child cursors, call
`MouseInputSource::showMouseCursor`, reconstruct coordinates from desktop
position, poll hover, or synthesize sibling transitions.

JUCE does not replace mesh-element hit testing: the shared Interactor still
tests the rendered waveform inside the single panel component.

## Implementation Slices

1. Add the diagnostic assertions and focused native fixtures without changing
   curve behavior.
2. Correct the Trimesh curve/intercept/control association at the narrow
   rasterizer-to-Interactor boundary.
3. Move the established gesture-direction calculation and complete reshape
   sequence into `Interactor2D`.
4. Delete the flat and Envelope `doReshapeCurve` copies and any temporary
   Trimesh polarity override.
5. Remove parent/global cursor forcing and retain only leaf-component cursor
   installation.
6. Run the cross-editor sequence tests, refactor/style pass, and inspect the
   production diff before committing.

## Tests

Focused semantic coverage must include:

- Cycle v1/shared-core unit coverage for upward, downward, reverse-direction,
  clamped, and zero-movement reshape updates;
- Guide Curve and Envelope native hover entry, cursor, hover exit, two drag
  updates, commit, visible direction, and undo;
- Trimesh native hover entry, cursor, at least two drag updates, visible curve
  direction, model publication, downstream refresh, commit, and exact undo;
- a drag that leaves panel bounds after mouse-down and continues updating;
- selection stability for linked Envelope vertices and Trimesh hidden
  dimensions;
- an architectural check or source review proving that concrete Cycle v2
  panels no longer override `doReshapeCurve` and production code no longer
  calls `MouseInputSource::showMouseCursor` for panel hover.

Tests that only observe a changed serialized mesh, a stored cursor value, or a
single delivered mouse event do not satisfy the contract. The rendered point
must move in the pointer direction and the full gesture must commit and undo.

## Complexity And Change Envelope

- Each mouse move remains proportional to the number of vertices selected by
  the existing domain selection rule; it performs no graph scan, sort,
  serialization, or topology rebuild.
- The shared-core change should be tens of lines, not a new interaction
  subsystem.
- Concrete panel production code should shrink because duplicated reshape
  implementations are deleted.
- No new `NodeKind` switch, compatibility bridge, or Cycle v2 domain type may
  enter `lib/`.

Expected production files are limited primarily to `Interactor2D`, the three
Cycle v2 curve clients, and the narrow Trimesh association boundary identified
by the diagnostic gate. A substantially larger diff requires design review.

## Deletion Targets

Completion requires deletion of:

- the flat-curve and Envelope copies of `doReshapeCurve`;
- every Trimesh-specific curve polarity override or sign correction;
- temporary gesture diagnostics;
- parent-editor and `NodeCanvas` cursor propagation for child panel hosts;
- native smoke assertions that accept any model difference without checking
  rendered direction.

## Completion Criteria

This TDD could not be marked
`Implemented` until:

- Cycle v1, Guide Curve, Envelope, and Trimesh use one shared reshape core;
- no client-specific polarity calculation remains;
- all positive gesture and cursor sequences pass;
- exact undo and downstream publication are demonstrated;
- the deletion targets are complete; and
- the implementation review reports diff size, largest files, new branches,
  shared code reused, and remaining compatibility code.

## Implementation Review

- `Interactor2D` is the only 2D `doReshapeCurve` implementation. It owns
  selection framing, domain vertex resolution, signed distance-to-control
  calculation, clamping, listener notification, and change marking.
- `CurveReshapeStrategy` is a small pure calculation seam used by that shared
  sequence and covered for upward, downward, reverse, stationary, scaled, and
  clamped edits.
- Prepared `Curve` copies retain transform metadata and arrays; a snapshot
  regression test protects this rasterizer-to-interaction boundary.
- Flat and Envelope panel copies were deleted. The remaining Envelope hook
  selects linked vertices and synchronizes loop/sustain seams; the Trimesh hook
  selects hidden-dimension vertices. Neither contains hit testing or polarity.
- Native Waveshaper/Guide-style, Envelope, and Trimesh fixtures target the
  rendered waveform, assert hover/cursor state, send multi-update gestures,
  check rendered direction and model publication, and exercise exact undo.
  Existing held-drag fixtures cover capture outside the initial hit region and
  existing causal assertions cover downstream Envelope and Trimesh refresh.
- The production diff adds no `NodeKind` branch or Cycle v2 domain type to
  `lib`. Production code no longer calls
  `MouseInputSource::showMouseCursor`, and concrete Cycle v2 panels do not
  override `doReshapeCurve`.
- Review size before documentation was 314 additions and 129 deletions across
  ten tracked files, dominated by 153 lines of native automation. The largest
  production changes were `Interactor2D.cpp` (+76/-24),
  `EnvelopeCurvePanel.cpp` (+14/-46), and `FlatCurvePanels.cpp` (-44). The
  interaction implementation itself shrank across clients; the remaining
  compatibility code is limited to flat rasterizers translating a selected
  vertex to the padded curve index when they lack waveform ownership offsets.
