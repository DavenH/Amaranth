# Refactor Notes

## Cycle 2 Node Canvas Responsibilities

`cycle-v2/src/UI/NodeCanvas.*` is currently carrying rendering, hit-testing, graph editing dispatch, snapshot save/load, undo/redo, and graph-status presentation. This was acceptable for fast prototyping, but it should be split before deeper interaction work lands.

Suggested extraction points:

- `NodeCanvasViewTransform` for pan/zoom and world/screen projection.
- `NodeCanvasHitTester` for nodes, ports, edges, palette, and minimap.
- `NodeCanvasRenderer` or smaller render helpers for grid, cables, nodes, previews, minimap, palette, and status.
- `NodeCanvasController` for graph edit commands, undo/redo, and snapshot persistence.

Keep the graph mutation rules in `GraphEditor`; the canvas layer should remain a client of that API rather than duplicating graph semantics.
