# Refactor Notes

## Cycle V2 JSON Graph and Typed Node Models

Status: In progress (2026-07-22).

Active TDD:
[`cycle-v2-json-graph-and-typed-node-models.md`](cycle-v2-json-graph-and-typed-node-models.md).

Completed:

- Replaced XML `.cyclegraph` persistence with deterministic canonical JSON.
- Removed escaped `mesh.topology` and `curve.modelSnapshot` parameters.
- Separated scalar parameters, aggregate model publication, and editor state.
- Converted bundled presets without a graph-format compatibility layer.
- Added conflict-checked model replacement, undo/redo, semantic persistence
  tests, and native save/reload coverage.

Remaining:

- Replace `TrimeshNodeModelState::meshState` and
  `CurveNodeModelState::state` `var` payloads with immutable concrete domain
  state. JSON must remain a serializer/codec boundary, not the in-memory model.
- Delete `readJSON(var)` reconstruction from Trimesh synchronization,
  flat-curve preparation and panel refresh, and envelope DSP preparation.
  These consumers must use or clone the already validated typed state.
- Prove through instrumentation that pointer movement, preview traversal,
  preparation from a loaded graph, and realtime processing perform no JSON
  decoding or domain reconstruction from JSON values.
- Rename remaining Cycle V2 graph snapshot and automation output paths that
  still use `.xml`, then verify all graph fixtures consistently describe JSON
  `.cyclegraph` files.
- Rerun the full semantic, standalone, and native smoke gates, then perform the
  production-diff/refactor audit before changing the TDD status to Implemented.

This is the prerequisite persistence/model boundary for the causal update
graph and must be completed before that boundary is considered closed.

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
