# Cycle V2 Trimesh Vertex Gesture Semantics

Status: Implemented (2026-09-10)

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
  transient graph publication. They may capture pointer identity and report
  selection, but must not reimplement mesh movement or curve shaping.

## Interaction Contract

- Pointer movement continuously resolves and repaints the closest displayed
  intercept without changing the selected vertex.
- A point gesture captures its vertex and owning cube at pointer-down. Every
  drag update manipulates that identity even when rasterization changes hover.
- Selection remains stable for the whole gesture and is published independently
  from mesh-content changes.
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

## Implementation Evidence

- `TrimeshInteractor2D` and `TrimeshInteractor3D` capture the pointer-down cube
  and vertex, then continue delegating movement and reshaping to the mature
  shared interactors.
- `TrimeshPanelBridge` keeps hover state transient, publishes selection as its
  own semantic edit, and keeps panel hosts stable for the lifetime of a native
  gesture.
- The focused interaction suite passes 29 assertions across five complete
  hover, point-drag, and curve-drag cases. The full Cycle V2 Trimesh suite
  passes 1,910 assertions across 58 cases.
- The native large-drag regression sequence passed with 20 movement updates,
  durable selected-vertex identity, and a materially larger endpoint delta.
- `/private/tmp/cycle-v2-trimesh-gesture-captures/trimesh-point-markers.png`
  records the doubled production-size 2D intercept footprints.
