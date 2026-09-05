# Cycle V2 Trimesh OpenGL Panel Clipping

Status: Implemented 2026-09-05

## Objective

Keep every OpenGL pass of the expanded Trimesh 2D and 3D panels inside its
declared host rectangle at normal and Retina scale, including after editor
resize. No JUCE paint mask or panel-specific overpaint may hide escaped pixels.

## Authoritative Implementation

`CurvePanelInfrastructure` already owns the mature top-left JUCE-coordinate to
bottom-left framebuffer scissor conversion. Its scoped guard also preserves and
restores the prior OpenGL scissor enablement and box. That behavior is
authoritative and must be shared rather than copied.

`PanelHostContext` and `PanelRenderContext` are authoritative for a hosted
panel's absolute bounds, clip, and rendering scale. `TrimeshPanelHosts` already
sets the 2D and 3D clip to their exact content bounds. The defect is that
`GLPanelRenderer` currently ignores that clip.

## Design

Extract the Curve host's scoped OpenGL scissor into shared panel
infrastructure. Curve and Trimesh hosts install the guard around the mature
panel render call using their absolute host bounds and rendering scale.

The stable end state is host-level enforcement: hosts that composite mature
panels into a shared OpenGL canvas declare the clip through `PanelHostContext`
and install the shared guard for the corresponding render lifetime. Panel
domain code remains unaware of framebuffer coordinate conventions, while
unrelated legacy renderer callers retain their existing behavior.

## Test-First Contract

1. The shared conversion maps top-left logical bounds into a bottom-left GL
   scissor box at 1x and 2x rendering scales.
2. A non-zero framebuffer viewport origin is preserved in the vertical
   conversion used by the mature Curve implementation.
3. Trimesh 2D and 3D hosts continue declaring their individual content bounds
   as their clips.
4. Production OpenGL screenshots of an expanded phase-spectrum mesh show no
   2D background, grid, or overlay pixels outside the 2D content rectangle.

## Negative Boundaries

- Do not add a JUCE paint mask or paint over escaped OpenGL content.
- Do not duplicate scissor arithmetic in Trimesh, Curve, or domain panels.
- Do not alter Trimesh rasterization, mesh data, 2D/3D layout, or draw order.
- Do not leave `GL_SCISSOR_TEST` or `GL_SCISSOR_BOX` changed after a panel pass.

## Completion Criteria

- Shared numerical tests and focused panel-host tests pass.
- Curve uses the shared guard with no duplicate scoped implementation.
- A production-size OpenGL screenshot passes at the current display scale.
- Standalone Debug builds; the Cycle V2 suite, `git diff --check`, style review,
  hot-loop review, and production-diff review are complete.

## Implementation Evidence

- The Curve-local guard is now `ScopedGLScissor` in shared panel
  infrastructure, and both Curve and Trimesh hosts use it around their exact
  render lifetimes. No rasterizer or JUCE paint path changed.
- Conversion tests pass at 1x, 2x, and a non-zero framebuffer viewport origin.
  The complete AmaranthLib suite passes 64,718 assertions in 220 test cases.
- Standalone Debug builds and the phase-editor production fixture reaches the
  expanded `phaseLayer1` editor without command failures. The full Cycle V2
  suite passes 10,665 of 10,666 assertions; the only failure is the known
  edge-hover help regression in `TestNodeCanvasHitRouter.cpp:66`.
- The production phase-editor fixture passes without command failures and a
  3456 x 1924 Retina capture confirms the 2D phase background, grid, waveform,
  and overlays stop at the lower host boundary. No pixels spill into the 3D
  surface or controls. Evidence is in
  `/private/tmp/cycle-v2-phase-clipping-verify-report.json` and
  `/private/tmp/cycle-v2-phase-clipping-verify.png`.
