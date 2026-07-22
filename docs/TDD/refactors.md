# Refactor Notes

## Cycle V2 JSON Graph and Typed Node Models

Status: Production refactor complete; native verification blocked (2026-07-22).

Active TDD:
[`cycle-v2-json-graph-and-typed-node-models.md`](cycle-v2-json-graph-and-typed-node-models.md).

Completed:

- Replaced XML `.cyclegraph` persistence with deterministic canonical JSON.
- Removed escaped `mesh.topology` and `curve.modelSnapshot` parameters.
- Separated scalar parameters, aggregate model publication, and editor state.
- Converted bundled presets without a graph-format compatibility layer.
- Added conflict-checked model replacement, undo/redo, semantic persistence
  tests, and native save/reload coverage.
- Replaced `var` payload holders with immutable concrete Trimesh, Envelope,
  and flat-curve snapshots.
- Removed runtime and presentation JSON reconstruction from synchronization,
  DSP preparation, preview, and audio paths.
- Added decode instrumentation proving already-loaded graphs remain outside
  JSON during presentation and runtime consumption.
- Renamed graph snapshot and automation outputs to `.cyclegraph`.
- Persisted authored port-side overrides without duplicating definition-owned
  port declarations, restoring the reviewed bundled layouts.

Remaining verification:

- Complete one reliable native macOS save/reload run across Trimesh, Envelope,
  and flat curves. The full suite and standalone build pass, but the native
  fixture currently misses pointer gestures nondeterministically and can fail
  before its persistence assertions. Keep the TDD in progress until this gate
  runs reliably or the native fixture is repaired and passes.

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
