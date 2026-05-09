# ADR 002: Composable Rasterization Pipeline

## Status

Accepted

## Context

`MeshRasterizer` currently combines mesh slicing, guide-curve deformation,
point scaling, wrapping, hidden-dimension color projection, curve padding,
waveform baking, sampling, panel snapshot publication, and mesh pointer cache
state.

Its subclasses reuse this behavior through inheritance, but most subclasses
only need a subset of the work. This has made `MeshRasterizer` large, stateful,
hard to unit test, and prone to abstraction leaks such as `colorPoints` living
on the base rasterizer even though they are panel visualization output.

`FXRasterizer` also shows that not every rasterization client should need a
full `Mesh`. Some clients only need a lightweight list of points that can be
interpolated, padded, baked into curves, and sampled.

## Decision

We will migrate rasterization toward a composable pipeline made of explicit
sources, interpolators, policies, builders, samplers, and compatibility
facades.

The target model separates:

- input source shape, such as mesh slices, envelope meshes, vertex lists, or
  point lists,
- interpolation strategy, such as mesh, envelope, or simple point
  interpolation,
- policies for scaling, wrapping, guide curves, depth projection, restriction,
  padding, curve resolution, waveform baking, and snapshot publication,
- builders for intercepts, curves, waveform buffers, and panel snapshots,
- samplers for normal, integral, and guide-decoupled sampling.

Existing public classes such as `MeshRasterizer`, `GraphicRasterizer`,
`FXRasterizer`, `VoiceMeshRasterizer`, and `EnvRasterizer` will remain as
compatibility facades during migration. The work will be staged so every phase
builds, existing tests continue to pass, and visual/audio behavior is validated
before moving to the next phase.

The migration completed the inheritance and ownership removal in favor of
composition: `FXRasterizer`, `GraphicRasterizer`, `E3Rasterizer`,
`VoiceMeshRasterizer`, and `EnvRasterizer` no longer derive from or own
`MeshRasterizer`. `MeshRasterizer` remains for now as a characterization-tested
compatibility shell, but production rasterizers should prefer the source,
pipeline, policy, storage, runtime, and narrow-interface types under
`Rasterization/`.

## Consequences

### Positive

- Rasterization stages become independently testable.
- `colorPoints` become an explicit optional depth-projection output instead of
  base rasterizer state.
- FX rasterization can move toward lightweight point-list inputs without
  depending on `Mesh`.
- Subclasses can shrink into facades or domain owners instead of inheriting a
  large protected state machine.
- The migration can be validated incrementally with characterization tests,
  dual-run comparisons, UI screenshots, and focused audio/UI fixtures.

### Negative

- The migration introduces more named concepts and files.
- Compatibility tests coexist with the retained `MeshRasterizer` shell until it
  is deleted or renamed in a later legacy cleanup.
- Some policy boundaries, especially guide curves, wrapping, and depth
  projection, are tightly coupled in the current code and must be extracted
  carefully.
- Envelope and voice rasterization should migrate late because they include
  domain-specific playback and chaining behavior.
- Policy files are grouped by domain to keep the added names navigable.

## Implementation Plan

Use the staged plan in
[Rasterization Pipeline Migration TDD](../TDD/rasterization-pipeline-migration.md).

The high-level sequence is:

1. Add characterization baselines and a dual-run comparison harness.
2. Introduce lightweight data shapes and conversion helpers.
3. Extract policies and builders behind the existing APIs.
4. Move simple point-list and FX paths off unnecessary mesh dependencies.
5. Introduce facades behind existing rasterizer classes.
6. Migrate graphic, voice, and envelope rasterizers only after shared stages are
   proven.
7. Group policy files by responsibility and keep the retained compatibility
   shell out of new rasterizer APIs.

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
