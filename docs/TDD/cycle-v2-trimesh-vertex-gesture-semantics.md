# Cycle V2 Trimesh Vertex Gesture Semantics

Status: Implemented after native regression correction (2026-09-11)

## Reopened Defect

The initial implementation retained raw `Vertex*` and `VertCube*` identities
across drag callbacks. A transient mesh publication can also trigger guide
preparation, whose prepared mesh is deep-copied into the panel model. That path
was not covered by the ordinary node-sync gesture guard, so the retained
pointers became dangling. The resulting native crash was captured in
`CycleV2-2026-09-11-093351.ips`: `Interactor::getClosestLine(Vertex*)`, called
from `setMovingVertsFromSelected()` during `mouseDrag`, dereferenced the stale
vertex.

The direct-event test did not exercise `PanelInputHostComponent` routing or the
guide-preparation rebind caused by transient publication. Its pointer mutation
was synthetic and validated the unsafe workaround rather than the product
lifecycle. It was replaced with a hosted gesture sequence that attempts the
real replacement path between multiple drag updates.

## Problem

The expanded Trimesh panels currently allow hover resolution to replace the
vertex that an active point drag manipulates. A short drag can therefore acquire
another nearby vertex after an intermediate rasterization, grow or change the
selection, and stop following the original vertex. The same state also makes
hover and selection appear conflated. Curve reshaping gives a nearby intercept
priority over an already-highlighted curve, contrary to the visible affordance.

The rendered intercepts are also too small at the production panel size.

## Authoritative Behavior

- The mature `Interactor2D` and `Interactor3D` implementations remain
  authoritative for mesh selection, linked-vertex movement, collision handling,
  curve sharpness, and undo framing.
- Cycle 1's interaction contract is retained: `currentIcpt`/`currentVertex`
  describe hover, while `getSelected()` describes stable selection.
- The Cycle V2 Trimesh specializations translate hosted panel events and
  transient graph publication. They report selection but must not retain mesh
  object pointers across a publication boundary or reimplement mesh movement
  and curve shaping.

## Interaction Contract

- Pointer movement continuously resolves and repaints the closest displayed
  intercept without changing the selected vertex.
- A point gesture keeps the shared interactor selection stable while the panel
  model retains one mesh instance. Mesh-replacing sync and guide preparation
  are deferred until the gesture is complete.
- Selection remains stable for the whole gesture. Selection-only gestures are
  published independently; mesh gestures consolidate selection and mesh
  content into the same transaction.
- A highlighted curve captures a reshape gesture at pointer-down. The visible
  intercept hit zone suppresses curve highlight so a drag there manipulates the
  closest vertex instead.
- Hover changes after pointer-up do not change selection.
- Trimesh intercept black, white, selected, and hover footprints are twice the
  prior production radii. Hit testing remains unchanged.

## Boundaries

- Do not change Envelope, Guide, Waveshaper, or shared Cycle 1 interaction.
- Do not copy movement, collision, curve evaluation, or sharpness algorithms.
- Do not use transient `editingGraph()` revisions as durable gesture bases.
- Keep hover identity transient; do not serialize it into the graph.

## Completion Criteria

- Focused tests cover hover changes without selection changes, curve-versus-point
  gesture routing, and a multi-update point drag that retains one selected
  vertex through intermediate publications.
- A native point drag travels materially farther than the old narrow smoke and
  reaches the expected endpoint without selection identity changing.
- A production-size capture confirms the larger intercept footprints and
  distinct hover/selection treatment.
- Focused tests, Standalone Debug, `git diff --check`, hot-loop review, and
  changed-file style review pass.

## Corrected Implementation Evidence

- The unsafe raw pointer capture has been removed from both Trimesh
  interactors. Mesh identity now remains stable because `syncFromNode()` and
  guide preparation both decline mesh replacement during an active gesture.
- `TrimeshPanelBridge` keeps hover state transient, emits selection-only edits
  separately, and keeps panel hosts stable for the lifetime of a native
  gesture. Mesh edit publication continues to consolidate mesh and selection.
- The hosted regression dispatches through `PanelInputHostComponent`, runs 12
  timed drag updates, attempts guide mesh replacement after each throttled
  publication, and passes 55 assertions without replacement or selection
  drift. The full Cycle V2 Trimesh suite passes 1,956 assertions across 58
  cases.
- The command-service sequence confirms transient and committed topology plus
  selected-vertex identity share one transaction and one undo.
- The external native `trimesh-point-drag` sequence passes a 20-update long
  drag with stable selected index. The focused native `trimesh-curve-drag`
  sequence passes hover routing, visible reshape, stable selected index,
  commit, undo, and redo.
- The previously reported native result did not prove the hosted publication
  lifecycle and has been replaced by the hosted and external checks above.
- `/private/tmp/cycle-v2-trimesh-regression-captures/trimesh-point-markers.png`
  records the doubled production-size 2D intercept footprints; the capture was
  inspected at original resolution.
