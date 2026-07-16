# Cycle v2 Render Invalidation Coalescing

## Status

Proposed.

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

