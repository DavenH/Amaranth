# ADR 002: Composable Rasterization Pipeline

## Status

Accepted

## Context

The original `MeshRasterizer` combined mesh slicing, guide-curve deformation,
point scaling, wrapping, hidden-dimension color projection, curve padding,
waveform baking, sampling, panel snapshot publication, and mesh pointer cache
state.

Its subclasses reused this behavior through inheritance, but most subclasses
only needed a subset of the work. That made `MeshRasterizer` large, stateful,
hard to unit test, and prone to abstraction leaks such as `colorPoints` living
on the base rasterizer even though they are panel visualization output.

`FXRasterizer` also shows that not every rasterization client should need a
full `Mesh`. Some clients only need a lightweight list of points that can be
interpolated, padded, baked into curves, and sampled.

## Decision

We migrated rasterization toward a composable pipeline made of explicit
sources, interpolators, policies, builders, samplers, and narrow rasterizer
owners.

The target model separates:

- input source shape, such as mesh slices, envelope meshes, vertex lists, or
  point lists,
- interpolation strategy, such as mesh, envelope, or simple point
  interpolation,
- policies for scaling, wrapping, guide curves, depth projection, restriction,
  padding, curve resolution, waveform baking, and snapshot publication,
- builders for intercepts, curves, waveform buffers, and panel snapshots,
- samplers for normal, integral, and guide-decoupled sampling.

The migration completed the inheritance and ownership removal in favor of
composition: `FXRasterizer`, `GraphicRasterizer`, `E3Rasterizer`,
`VoiceMeshRasterizer`, and `EnvRasterizer` no longer derive from or own
`MeshRasterizer`. Production code no longer includes or instantiates
`MeshRasterizer`; characterization coverage now lives in the test-only
`LegacyMeshRasterizer` fixture.

Current production rasterizers share only non-leaky necessities:
`Rasterizer` defines the runtime view surface, `BaseRasterizer` owns snapshot
publication storage, and `TrilinearMeshRasterizer` centralizes the mesh,
morph-position, guide-curve, and request forwarding that is intrinsically tied
to trilinear mesh waveform rasterization.

## Consequences

### Positive

- Rasterization stages become independently testable.
- `colorPoints` become an explicit optional depth-projection output instead of
  base rasterizer state.
- FX rasterization can move toward lightweight point-list inputs without
  depending on `Mesh`.
- Former subclasses shrink into domain owners instead of inheriting a large
  protected state machine.
- The migration can be validated incrementally with characterization tests,
  dual-run comparisons, UI screenshots, and focused audio/UI fixtures.

### Negative

- The migration introduces more named concepts and files.
- Characterization tests keep a test-only legacy fixture so extracted behavior
  can still be compared against the old implementation.
- Some policy boundaries, especially guide curves, wrapping, and depth
  projection, are tightly coupled in the current code and must be extracted
  carefully.
- Envelope and voice rasterization should migrate late because they include
  domain-specific playback and chaining behavior.
- Policy files are grouped by domain to keep the added names navigable.

## Implementation Plan

The migration followed the staged plan in
[Rasterization Pipeline Migration TDD](../TDD/rasterization-pipeline-migration.md).

The high-level sequence was:

1. Add characterization baselines and a dual-run comparison harness.
2. Introduce lightweight data shapes and conversion helpers.
3. Extract policies and builders behind the existing APIs.
4. Move simple point-list and FX paths off unnecessary mesh dependencies.
5. Introduce narrow production rasterizer owners behind existing caller names.
6. Migrate graphic, voice, and envelope rasterizers only after shared stages are
   proven.
7. Group policy files by responsibility and keep legacy behavior isolated to
   tests.

## Validation

Each migration phase must preserve a buildable state and pass existing tests.
Risk-specific validation should include:

- unit tests for extracted policies,
- old/new output comparison for intercepts, curves, color points, waveform
  buffers, indices, and sampled output,
- default UI screenshot and cropped panel baselines for visual phases,
- focused UI automation fixtures for panel and preset behavior,
- offline audio capture for voice and envelope phases.

## Alternatives Considered

### Keep the inheritance hierarchy and add more tests

Rejected.

This would improve confidence but not address the protected-state coupling or
UI/DSP abstraction leaks.

### Rewrite rasterization in one pass

Rejected.

Current behavior works and is visually/audio sensitive. A flag-day rewrite
would be difficult to validate and hard to roll back.

### Make every policy a compile-time template parameter

Rejected for now.

The project needs clarity and staged migration more than maximum static
composition. Small runtime-composed policies and context structs are easier to
debug while preserving current behavior.
