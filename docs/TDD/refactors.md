# Refactor Notes

## Cycle 2 Node Canvas Responsibilities

`cycle-v2/src/UI/NodeCanvas.*` is currently carrying rendering, hit-testing, graph editing dispatch, snapshot save/load, undo/redo, and graph-status presentation. This was acceptable for fast prototyping, but it should be split before deeper interaction work lands.

Suggested extraction points:

- `NodeCanvasViewTransform` for pan/zoom and world/screen projection.
- `NodeCanvasHitTester` for nodes, ports, edges, palette, and minimap.
- `NodeCanvasRenderer` or smaller render helpers for grid, cables, nodes, previews, minimap, palette, and status.
- `NodeCanvasController` for graph edit commands, undo/redo, and snapshot persistence.

Keep the graph mutation rules in `GraphEditor`; the canvas layer should remain a client of that API rather than duplicating graph semantics.

## Cycle 2 Graph Audio Buffer Ownership

`cycle-v2/src/Runtime/AudioProcessBlock` currently owns `std::vector<float>`
sample storage. This is acceptable scaffolding, but it should not become the
long-term DSP storage model.

Suggested direction:

- Use `Buffer<float>` and the repo's stereo buffer conventions for node DSP
  APIs wherever possible.
- Keep `AudioProcessBlock` as a lightweight graph-runtime view/metadata wrapper
  for domain, channel layout, and port routing.
- Introduce a compiled-plan work arena backed by `ScopedAlloc<float>` or a
  similar preallocated block allocator.
- Let the graph execution plan precompute buffer sizes, lifetimes, and port
  mappings.
- During processing, hand node modules `Buffer<float>` views into that arena
  instead of allocating per-node/per-port vectors.

This should be addressed before hardening the Cycle 2 runtime for realtime
audio-thread execution.
