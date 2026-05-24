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

## Cycle 2 Trimesh Rasterizer Compatibility Boundary

`VoiceMeshRasterizer` is still defined under `cycle/` and depends on Cycle 1.x
application state such as `SingletonAccessor`, `CycleState`, and legacy voice
chaining ownership. Cycle 2 currently links `AmaranthLib`, so its first trimesh
node backend should reuse `Rasterization::TrilinearMeshRasterizer` from `lib/`
as the stable shared core.

Suggested direction:

- Move the reusable voice-chain behavior behind a library-level façade that
  does not depend on Cycle 1.x app singletons.
- Keep Cycle 2 node DSP modules under `cycle-v2/src/Nodes/Trimesh/`.
- Preserve separate blockwise audio and gridwise UI/update surfaces, matching
  the FFT folder pattern.
- Let the node model own mesh data and pass explicit mesh/context references to
  DSP and widget modules instead of reaching through `MeshLibrary`.

## Cycle 2 OpenGL Cable Tessellation

The prototype GL cable renderer exposed platform-dependent artifacts with wide
`GL_LINE_STRIP` strokes and hand-built triangle strips: square cutouts on
curves, disappearing vertical sections, and poor selected-line readability.
Cycle 2 currently keeps cables on the JUCE path renderer while the background
remains OpenGL-backed.

Suggested direction:

- Re-enable GL cables only after adding a real stroked-path tessellator that
  emits joins, caps, and dash runs as explicit geometry.
- Treat selection as a separate narrow highlight stroke rather than by making
  the halo heavily opaque.
- Keep node shells and node contents in the same render layer unless the whole
  node widget moves to GL, because split shell/content rendering breaks
  overlap z-order.
