# Refactor Notes

## Cycle V2 JSON Graph and Typed Node Models

Proposed TDD:
[`cycle-v2-json-graph-and-typed-node-models.md`](cycle-v2-json-graph-and-typed-node-models.md).

Replace XML `.cyclegraph` persistence and escaped JSON parameters with one
canonical JSON document. Separate scalar parameters, immutable aggregate node
models, and editor intent; embed mature `Mesh`/`EnvelopeMesh` JSON directly;
remove `mesh.topology` and `curve.modelSnapshot` parameter strings; and convert
Cycle V2 presets directly without a compatibility layer.

This is the prerequisite persistence/model boundary for the causal update
graph and should be implemented first.

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
