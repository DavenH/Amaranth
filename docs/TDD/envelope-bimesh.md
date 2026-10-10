# Envelope Bimesh Investigation

## Status

Proposed; no implementation approved or begun (2026-09-16).

Update 2026-09-17: Envelope selection presentation now collapses each
Time-pole pair to one logical selected marker and reports four logical moving
corners where the existing `VertCube` still moves eight physical vertices.
This is not a genuine bimesh; old storage, rasterization, and DSP remain
unchanged. The migration below is still required to eliminate Time-pole
vertices from the model itself.

## Motivation

The Envelope editor presents Time as a morph axis because its current model is
an `EnvelopeMesh` made of `VertCube` objects. Time-pole vertices are storage
artifacts, not meaningful editable points in the Envelope view. The immediate
UI repair hides the Time link and leaves Time implicitly linked; it does not
change the persistent mesh or DSP behavior.

## Existing Authority And Scope

`lib/src/Curve/Mesh/EnvelopeMesh.*` and `VertCube` are authoritative for
topology and editing. `EnvRasterizer`, `EnvelopeMaterialization`, and
`EnvelopePolicies` consume that representation; Cycle V2's
`EnvelopeMeshState`, `CurveNodeModels`, `EnvelopeCurvePanel`, and
`EnvelopeSignalProcessor` add persistence, editing, and audio/preview use.
This is a cross-cutting domain change, not a vertex-class template swap.
Cycle 1 also uses `EnvelopeMesh`, so replacing it in the shared library would
need compatibility and parity tests for both clients.

## Design Questions To Resolve

- Define a genuine two-morph-axis envelope topology and vertex identity,
  including loop/sustain markers, intersections, guide attachment, and undo.
- Decide whether a small shared mesh interface can express the existing
  rasterizer/editor operations without per-kind branches. Do not introduce a
  generic base that merely hides a second implementation of the algorithms.
- Specify lossless migration of existing serialized `VertCube` envelopes,
  including old presets and graph revisions. Preserve old-format loading.
- Keep UI gesture updates proportional to changed vertices; no whole-mesh
  copy, serialization, or rasterizer rebuild on each movement.
- Establish audio, grid, editor, automation, and Cycle 1 parity tests before
  changing storage.

## Proposed Sequence

First enumerate the exact operations shared by the mature envelope rasterizer
and editor, then extract only a useful common core. Next implement the
two-axis model behind that core, add migration and parity fixtures, and only
then remove Time-pole construction and UI compatibility branches. A separate
review is required before selecting inheritance, templates, or a new mesh
type: the right abstraction follows those operation and ownership contracts.

## Effort/Risk

This is a substantial multi-subsystem refactor, likely several coherent
implementation slices rather than a minor UI patch. The main risk is silently
changing envelope evaluation or old-preset semantics while removing the
storage-only pole vertices.
