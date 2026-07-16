# Refactor Notes

## Cycle 2 Semantic Test Audit

Cycle 2 currently has many unit assertions but did not detect loss of vertex
hover, drag classification, edit publication, or stable Envelope parameter
selection. Audit the suite by observable contract rather than assertion count.

- Identify tests that only cover constructors, getters, scaffolding, fake
  payloads, or implementation details and remove or consolidate them.
- Build a smaller semantic matrix for graph mutation, DSP parity, pointer
  interaction sequences, model publication, persistence, and visible output.
- Require focused automation for hover, mouse-down classification, drag,
  mouse-up publication, and downstream recomputation.
- Prefer parity tests that run Cycle 1 and Cycle 2 through the same extracted
  core over separate tests that bless two implementations.
- Report which product risks remain untested; do not use aggregate assertion
  counts as evidence of behavioral coverage.

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

## Trilinear Mesh Intercept Ownership

`TrilinearMeshRasterizer` currently keeps its update-geometry intercept output
in `meshIntercepts` and publishes that into the rasterizer snapshot, while the
base rasterizer also has `rasterizerData.intercepts`. Refactor this to a single
authoritative intercept store so UI overlays, interactors, and waveform baking
cannot accidentally read different intercept sources.

## Trimesh Topology Snapshot

Trimesh vertex field parameters preserve value edits but do not explicitly
encode cube/vertex topology. Add/delete works in the live editor, but durable
save/reload parity needs a typed topology snapshot rather than inferring a mesh
from sparse `mesh.vertex.<index>.<field>` overrides. Reuse the mature `Mesh`
serialization boundary behind a narrow Trimesh model adapter; do not invent a
second topology format in the canvas or editor.
