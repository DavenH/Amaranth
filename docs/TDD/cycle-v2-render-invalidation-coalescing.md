# Cycle v2 Render Invalidation Coalescing

## Status

Completed 2026-07-16.

## Problem

Curve editors, Trimesh panel bridges, hosted editors, and `NodeCanvas` can each
request repaint or OpenGL texture baking during one semantic edit. JUCE may
coalesce some repaint messages, and bake flags are often idempotent, but the
architecture does not state or measure the contract. Expensive panel work can
therefore drift from once per committed frame to once per callback.

## Cost Contract

One authoring event may publish model state once and enqueue at most one dirty
request per affected render surface. A drag may enqueue once per delivered
frame, but never once per parameter, downstream listener, and host adapter.
Painting visible nodes and cables remains `O(Vvisible + Evisible)`.

## Design

- Represent panel/texture/canvas invalidation as category bits accumulated by
  the editor host.
- Flush dirty categories at one render-scheduling boundary.
- Keep model publication separate from repaint scheduling.
- Preserve dirty-region clipping and OpenGL bake requirements.

## Semantic Tests

- Counters cover flat curve drag, Envelope drag, Trimesh morph edit, editor
  open/close, and graph node movement.
- Multiple invalidations in one command cause one repaint and one required bake.
- Separate UI frames remain separately visible.
- Hidden/offscreen panels do not bake textures until made visible.

## Completion Criteria

- Repaint and bake multiplicity is deterministic and fixture-tested.
- Domain editors do not directly fan out host repaint callbacks.

## Implemented

- `RenderInvalidationAccumulator` provides one thread-safe, host-owned
  scheduling boundary with category masks, hidden-surface deferral, and
  deterministic diagnostics.
- Curve panel repaint, owner repaint, and texture-bake requests accumulate at
  `CurvePanelHost`; OpenGL snapshot publication no longer posts an independent
  repaint callback.
- Trimesh 2D and 3D texture categories accumulate independently while sharing
  one owner repaint dispatch. Rasterization no longer bakes 3D textures and
  separately requests a panel repaint for the same edit.
- `NodeCanvas` routes authoring, editor lifecycle, pointer, viewport, and graph
  movement repaints through one canvas category instead of calling JUCE
  repaint from every path.
- Texture categories remain pending while their render surface is unavailable
  and flush when a visible render pass restores availability.

## Proof

- `TestRenderInvalidationAccumulator.cpp` verifies repeated-category
  coalescing, distinct UI frames, and hidden texture deferral.
- `CycleV2_tests`: 2,773 assertions in 253 test cases.
- `CycleV2` standalone target builds successfully.
- The focused Waveshaper editor fixture completed both commands and its macOS
  OS capture showed the hosted OpenGL editor and surrounding canvas correctly
  composited (`/private/tmp/cycle-v2-invalidation-os.png`).
