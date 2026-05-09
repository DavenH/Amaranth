# Rasterization Pipeline Migration TDD

> Superseded: this document captured the incremental extraction/draining
> migration. The active plan is now
> [Rasterization Simplification TDD](rasterization-simplification.md), which
> targets a smaller wholesale composed rasterizer and net code reduction.

## Overview

This document describes a staged migration from the current `MeshRasterizer`
inheritance hierarchy to a composable rasterization pipeline.

The current system works, so the migration must be conservative. Each phase
must build, preserve existing end-to-end behavior, and expose a narrow
validation surface before the next phase starts.

## Problem Statement

`MeshRasterizer` currently combines several independent responsibilities:

- mesh binding and cached lifetime state,
- mesh cross-section extraction,
- guide-curve deformation,
- point scaling and wrapping,
- hidden-dimension color/depth projection,
- intercept restriction and sorting,
- curve padding,
- curve resolution selection,
- waveform buffer baking,
- waveform sampling,
- panel snapshot publication through `RasterizerData`,
- state overrides for batched visual-DSP rendering.

The child classes use inheritance mostly to reuse protected state and helper
methods. This makes the class hard to test because most useful operations are
available only by constructing a large stateful object with mesh, guide curve,
curve, panel, and update-system assumptions.

The strongest abstraction leak is `colorPoints`: these are UI projection
artifacts produced as a convenient byproduct of mesh slice calculation, but
they live on the base rasterizer and are copied into `RasterizerData` for panel
rendering. DSP, voice, envelope, FX, and simple point-list rasterizers inherit
that concern even when they do not need it.

## Goals

- Preserve current behavior throughout the migration.
- Replace broad inheritance reuse with explicit source, interpolation, policy,
  builder, and sampler roles.
- Allow FX rasterization to consume a lightweight point list instead of a full
  `Mesh`.
- Make color/depth point generation optional and testable as UI projection
  output, not base rasterizer state.
- Enable small unit tests for each pipeline stage.
- Keep existing public facades alive while internals are drained into new
  components.
- Preserve existing UI, audio, and preset end-to-end tests after every phase.

## Non-Goals

- Rewriting `EnvRasterizer` or `VoiceMeshRasterizer` first.
- Changing mesh ownership or effective-mesh binding in this migration.
- Replacing `Curve` internals or changing curve table generation.
- Changing rendered visuals intentionally.
- Introducing a heavy template framework for every policy combination.

## Current Specialization Map

### Base Mesh Rasterizer

Current role:

- slices a `Mesh` along the active morph axis,
- applies guide curves,
- scales y values,
- wraps or clamps x values,
- optionally emits hidden-dimension `ColorPoint` projections,
- builds padded curves,
- bakes `waveX`, `waveY`, `slope`, `diffX`, and `area`,
- publishes `RasterizerData`.

Target role:

- compatibility facade over reusable pipeline stages.

### `GraphicRasterizer`

Specializes:

- primary view dimension from `CurrentMorphAxis`,
- morph position from `MorphPanel`,
- scratch-position override for time/spectrum/phase groups,
- cyclic behavior and x limits per layer group,
- panel-facing raster data with depth/color projection.

Target shape:

- `GraphicRasterizerFacade`,
- `MeshSliceSource`,
- `TrilinearMeshInterpolator`,
- `HiddenDimensionDepthProjection`,
- normal guide, scaling, wrapping, padding, and waveform policies.

### `FXRasterizer`

Specializes:

- non-cyclic x/y point rasterization,
- no depth projection,
- no cube interpolation requirement,
- FX-specific padding,
- lightweight curve/waveform generation.

Target shape:

- `FxRasterizerFacade`,
- `VertexListSource` or `PointListSource`,
- `LinearPointInterpolator`,
- `DisabledDepthProjection`,
- `FxPaddingPolicy`.

The target facade must not require a `Mesh` just to rasterize ordered FX
points. A temporary adapter may convert existing FX mesh storage into a point
source during migration.

### `EnvRasterizer`

Specializes:

- envelope mesh binding,
- loop/sustain/release marker interpretation,
- non-cyclic x limits,
- envelope-specific padding for normal, looping, and releasing states,
- render-time state machine,
- one-sample-per-cycle guide-curve decoupling,
- per-unison render parameters.

Target shape:

- `EnvelopeRasterizerFacade`,
- `EnvelopeSliceSource`,
- `EnvelopeInterpolator`,
- `EnvelopeMarkerPolicy`,
- `EnvelopeNormalPaddingPolicy`,
- `EnvelopeLoopPaddingPolicy`,
- `EnvelopeReleasePaddingPolicy`,
- composed `WaveformSampler`.

`EnvRasterizer` should migrate late. It is a playback state machine that uses
rasterized curves; it should not drive the first extraction.

### `VoiceMeshRasterizer`

Specializes:

- audio-thread mesh slicing,
- phase wrapping,
- chaining prior and next intercept windows across audio blocks,
- no depth projection,
- bipolar scaling,
- hot-path waveform baking and sampling.

Target shape:

- `VoiceRasterizerFacade`,
- `MeshSliceSource`,
- `TrilinearMeshInterpolator`,
- `VoiceChainedPaddingPolicy`,
- `DisabledDepthProjection`,
- reusable waveform builder and sampler.

This should migrate after the common builder is proven, because it currently
duplicates and mutates base rasterization internals for continuity.

### `Rasterizer2D`

Specializes:

- point-list input,
- optional cyclic padding,
- curve update for direct point movement.

Target shape:

- `PointListSource`,
- `LinearPointInterpolator`,
- simple cyclic or non-cyclic padding,
- shared waveform builder.

This is a good early migration target because it has no mesh dependency.

### `E3Rasterizer`

Specializes:

- batched column rendering for envelope 3D,
- repeated morph-axis changes,
- low-res curves,
- no depth projection,
- uses the rasterizer as a column producer.

Target shape:

- a grid/column renderer that composes an envelope or mesh rasterizer facade
  per column, or reuses one mutable facade behind a batch context.

## Target File Hierarchy

The target hierarchy should live under a new nested directory so the old and
new systems can coexist during migration.

```text
lib/src/Curve/Rasterization/
  RasterizerTypes.h
  RasterizerOptions.h
  RasterizerCompare.h

  Sources/
    MeshSliceSource.h
    EnvelopeSliceSource.h
    VertexListSource.h
    PointListSource.h

  Interpolation/
    TrilinearMeshInterpolator.h
    EnvelopeInterpolator.h
    LinearPointInterpolator.h

  Policies/
    Core/
      DefaultVertexWrapPolicy.h
      InterceptDegeneracyPolicy.h
      InterceptRestrictionPolicy.h
      InterceptSortPolicy.h
      PaddingPolicy.h
      PointScalingPolicy.h
      RasterizerCleanupPolicy.h
      RasterizerOutputPolicy.h
      SnapshotPolicy.h
    Curves/
      CurveTripletPolicy.h
      CurveResolutionPolicy.h
      CurveWaveformPreparationPolicy.h
      InterceptPaddingFlagPolicy.h
      WaveformBakePolicy.h
      WaveformBuildPolicy.h
    Mesh/
      ComponentGuideSharpnessPolicy.h
      DepthProjectionPolicy.h
      GuideCurvePolicy.h
      MeshSliceOutputPolicy.h
    Envelope/
      EnvelopeMarkerPolicy.h
      EnvelopePaddingPolicy.h
      EnvelopePlaybackPolicy.h
      EnvelopeReleasePolicy.h
      EnvelopeRenderTimingPolicy.h
      EnvelopeStateValidationPolicy.h
      EnvelopeSustainPointPolicy.h

  Builders/
    InterceptBuilder.h
    CurveBuilder.h
    WaveformBuilder.h
    RasterizerSnapshotBuilder.h

  Pipelines/
    CurveWaveformPipeline.h

  Sampling/
    WaveformSampler.h
    IntegralWaveformSampler.h
    DecoupledGuideSampler.h

  Facades/
    MeshRasterizerFacade.h
    GraphicRasterizerFacade.h
    EnvelopeRasterizerFacade.h
    VoiceRasterizerFacade.h
    FxRasterizerFacade.h
```

Cycle-only policies live under:

```text
cycle/src/Curve/Rasterization/Policies/
  Graphic/
    GraphicAxisPolicy.h
    GraphicMorphPositionPolicy.h
  Voice/
    VoiceChainedPaddingPolicy.h
    VoiceChainingPolicy.h
    VoiceCurveResolutionPolicy.h
    VoicePointPositionPolicy.h
    VoiceWaveformUpdatePolicy.h
```

The first files added should be data types and policy extractions, not new
facades. Facades should appear only once at least one real caller can use the
new pipeline behind the old API.

`RasterizerCompare.h` should be introduced during Phase 0 or Phase 1. It is a
test helper, not production logic. Its job is to compare old and new pipeline
outputs from identical inputs with explicit tolerances.

## Core Data Shapes

### `RasterPoint`

`RasterPoint` is the target replacement for using `Intercept` as both curve
input and mesh identity carrier.

```cpp
struct RasterPointSource {
    enum Type {
        None,
        MeshCube,
        EnvelopeMarker,
        FxVertex,
        ExternalPoint
    };

    Type type {};
    int marker {};
};

struct RasterPoint {
    float x {};
    float y {};
    float sharpness {};
    float adjustedX {};
    bool padBefore {};
    bool padAfter {};
    bool isWrapped {};
    RasterPointSource source;
};
```

`RasterPoint` and `RasterPointSource` must remain lightweight. They should not
include `Mesh.h` or `VertCube.h`. Mesh-specific identity should live in a
sidecar used only by mesh-aware sources:

```cpp
struct MeshPointSourceRef {
    VertCube* cube {};
};
```

Conversion helpers may temporarily reattach `VertCube*` when converting to the
legacy `Intercept` shape required by `Curve`, but lightweight FX and point-list
paths must be able to include `RasterizerTypes.h` without pulling in mesh
headers.

Migration rule:

- keep conversion helpers between `RasterPoint` and `Intercept`,
- keep `Curve` using `Intercept` until a later dedicated curve cleanup,
- avoid replacing `Curve` and rasterizer architecture in the same phase.

### `InterceptBuildResult`

```cpp
struct InterceptBuildResult {
    vector<RasterPoint> points;
    vector<ColorPoint> colorPoints;
    bool needsResort {};
};
```

`colorPoints` belong here as optional projection output, not as a mandatory
field on every rasterizer object.

### `CurveBuildResult`

```cpp
struct CurveBuildResult {
    vector<Curve> curves;
    vector<Intercept> frontPadding;
    vector<Intercept> backPadding;
    int paddingSize {};
};
```

`frontPadding` and `backPadding` are included because current UI/debug callers
can inspect front/back intercepts.

### `WaveformResult`

```cpp
struct WaveformResult {
    Buffer<float> waveX;
    Buffer<float> waveY;
    Buffer<float> diffX;
    Buffer<float> slope;
    Buffer<float> area;
    int zeroIndex {};
    int oneIndex {};
    bool sampleable {};
};
```

The initial implementation may keep buffers owned by a facade or `ScopedAlloc`;
the result object should describe the shape even when it does not own the
memory yet.

## Policy Classes

Policy ownership rules:

- prefer stateless policies with all changing inputs passed through context
  structs,
- keep owning storage in facades, builders, or explicit result objects,
- avoid policies that cache mesh, guide-provider, or buffer state across calls,
- use narrow context types for dependencies such as `MorphPosition`,
  `Dimensions`, x limits, seeds, and `GuideCurveProvider`,
- do not recreate `MeshRasterizer` as a collection of mutable policy objects.

Example context shapes:

```cpp
struct RasterizationContext {
    MorphPosition morph;
    Dimensions dims;
    float xMinimum {};
    float xMaximum { 1.f };
    int noiseSeed { -1 };
};

struct GuideCurveContextView {
    GuideCurveProvider* provider {};
    const short* phaseOffsetSeeds {};
    const short* vertOffsetSeeds {};
};
```

### `PointScalingPolicy`

Responsibilities:

- transform incoming y values according to rasterization mode,
- expose minimum and maximum valid output values for guide-curve clamping.

Concrete policies:

- `UnipolarPointScaling`,
- `BipolarPointScaling`,
- `HalfBipolarPointScaling`,
- `IdentityPointScaling`.

Validation:

- table-driven unit tests over representative values,
- compare existing `MeshRasterizer::ScalingType` outputs to policy outputs.

### `WrapPolicy`

Responsibilities:

- apply cyclic phase wrapping or x-range clamping,
- report whether wrapping caused resorting,
- handle current `wrapVertices` behavior for mesh cube endpoints.

Concrete policies:

- `CyclicPhaseWrap`,
- `ClampToRange`,
- `NoWrap`.

Validation:

- unit tests for points below 0, within range, above 1, and crossing the cyclic
  boundary,
- fixture tests for existing waveform wrap behavior.

### `AxisPolicy`

Responsibilities:

- choose the independent morph dimension,
- map visible and hidden dimensions for projection,
- provide x/y vertex dimensions.

Concrete policies:

- `FixedAxisPolicy`,
- `CurrentMorphAxisPolicy`,
- `OverrideAxisPolicy`.

Validation:

- test Waveform2D-style dimensions preserve Time/Red/Blue hidden projection,
- test spectrum/phase axis selection against current settings.

### `GuideCurvePolicy`

Responsibilities:

- apply red/blue/amp/phase/curve guide deformation to points,
- manage noise context lookup,
- report when adjusted x wrapping requires resorting,
- optionally decouple component guide curves for one-sample-per-cycle envelope
  rendering.

Concrete policies:

- `DisabledGuideCurvePolicy`,
- `CoupledGuideCurvePolicy`,
- `DecoupledGuideCurvePolicy`.

Validation:

- no-guide path is byte-for-byte or tolerance equivalent,
- guide deformation changes only `adjustedX`, `y`, and `sharpness` as expected,
- decoupled guide regions match existing envelope one-sample behavior.

### `DepthProjectionPolicy`

Responsibilities:

- optionally emit `ColorPoint` values for hidden dimensions,
- use the same intermediate mesh reduction data as point extraction,
- keep UI-only output separate from sampleable waveform output.

Concrete policies:

- `DisabledDepthProjection`,
- `HiddenDimensionDepthProjection`.

Validation:

- default Waveform2D baseline includes color points,
- Red and Blue hidden dims are not filtered out,
- DSP/FX/voice paths emit no projection output.

### `InterceptRestrictionPolicy`

Responsibilities:

- constrain adjusted x into valid range,
- ensure strict sort order,
- preserve current small-spacing behavior.

Concrete policies:

- `StrictRangeRestriction`.

Validation:

- direct unit tests for duplicate and reversed x values,
- comparison tests against current `restrictIntercepts`.

### `PaddingPolicy`

Responsibilities:

- build curve input triplets from sorted points,
- preserve required sampleable range,
- expose padding size and front/back padding.

Concrete policies:

- `CyclicPaddingPolicy`,
- `NonCyclicPaddingPolicy`,
- `FxPaddingPolicy`,
- `EnvelopeNormalPaddingPolicy`,
- `EnvelopeLoopPaddingPolicy`,
- `EnvelopeReleasePaddingPolicy`,
- `VoiceChainedPaddingPolicy`.

Validation:

- generated curves have the same `a/b/c` continuity as current curves,
- `zeroIndex` and `oneIndex` remain valid after waveform baking,
- envelope loop/release behavior is validated with existing render tests before
  any envelope migration phase is accepted.

### `CurveResolutionPolicy`

Responsibilities:

- assign `Curve::resIndex`,
- handle low-resolution and integral sampling choices,
- account for guide-curve table density.

Concrete policies:

- `NormalCurveResolution`,
- `LowCurveResolution`,
- `IntegralSamplingCurveResolution`,
- `GuideCurveAwareResolution`.

Validation:

- compare existing curve `resIndex` and `curveRes` values on synthetic meshes,
- compare total waveform buffer size where deterministic.

### `WaveformBakePolicy`

Responsibilities:

- bake curves into `waveX`, `waveY`, `diffX`, `slope`, and `area`,
- compute `zeroIndex` and `oneIndex`,
- handle guide-curve component deformation regions.

Concrete policies:

- `DefaultWaveformBakePolicy`,
- `GuideCurveWaveformBakePolicy`.

Validation:

- compare wave buffers against existing `MeshRasterizer::calcWaveform`,
- sample evenly and at intervals with existing sampler tests,
- no scalar math regressions in hot loops.

### `SnapshotPolicy`

Responsibilities:

- copy current pipeline output into `RasterizerData`,
- lock only around publication,
- allow non-UI rasterizers to skip snapshot publication.

Concrete policies:

- `NoSnapshot`,
- `RasterizerDataSnapshot`.

Validation:

- panel draw paths continue to see consistent `RasterizerData`,
- old `makeCopy()` behavior is preserved during facade phases.

## Builder Pipeline

The target pipeline is:

```text
Source
  -> Interpolator
  -> PointScalingPolicy
  -> WrapPolicy
  -> GuideCurvePolicy
  -> DepthProjectionPolicy
  -> InterceptRestrictionPolicy
  -> PaddingPolicy
  -> CurveResolutionPolicy
  -> WaveformBuilder
  -> Sampler / SnapshotPolicy
```

The first implementation should not require every stage to be a separate
object at runtime. The important rule is that each responsibility has a clear
file and test boundary before subclasses are migrated.

## Migration Phases

### Phase 0: Characterization Baseline

Code changes:

- no production behavior changes,
- add or expand characterization tests,
- add deterministic old-output characterization fixtures,
- add a dual-run comparison harness before any behavior extraction,
- capture UI screenshot baseline.

Comparator harness:

- create a test-only comparison helper such as `RasterizerCompare`,
- compare intercepts by count, x, y, sharpness, wrapped/padding flags, and
  source identity where meaningful,
- compare `ColorPoint` count, hidden dimension, and before/mid/after positions,
- compare curves by `a/b/c`, `resIndex`, `curveRes`, and `waveIdx`,
- compare `waveX`, `waveY`, `diffX`, `slope`, `area`, `zeroIndex`, and
  `oneIndex`,
- compare sampled output from `sampleEvenlyTo`, `sampleAtIntervals`, and
  representative `sampleAt` calls.

Initial fixtures:

- cyclic synthetic wave mesh,
- non-cyclic synthetic FX point list or FX mesh adapter,
- Waveform2D-style dimensions with hidden Time/Red/Blue projection,
- duplicate/reversed x intercept list for restriction behavior,
- guide-curve disabled baseline.

Tests:

- `cmake --preset tests && cmake --build --preset tests`,
- `ctest --test-dir build/tests -V`,
- create the baseline screenshot once if it does not already exist,
- otherwise capture repeat validation screenshots to `/tmp` so the preserved
  baseline is not overwritten.

Initial baseline command:

```sh
test -f docs/media/images/cycle-default-ui-rasterization-baseline.png \
    || scripts/capture_cycle_ui.sh \
        docs/media/images/cycle-default-ui-rasterization-baseline.png \
        /tmp/cycle-default-ui-rasterization-baseline-logs.txt
```

Repeat validation command:

```sh
scripts/capture_cycle_ui.sh \
    /tmp/cycle-default-ui-rasterization-candidate.png \
    /tmp/cycle-default-ui-rasterization-candidate-logs.txt
```

Acceptance:

- tests pass,
- screenshot is nonblank and visually representative of the default preset,
- filtered launch log contains no new assertions, crashes, or rasterizer errors.

### Phase 1: Data Shapes And Conversion Helpers

Code changes:

- add `RasterizerTypes.h`,
- add `RasterizerOptions.h` only if options are clearer than passing base
  rasterizer state directly,
- add test-only `RasterizerCompare.h` if it was not added in Phase 0,
- add conversion between `Intercept` and `RasterPoint`,
- keep mesh-specific source refs outside lightweight `RasterPoint`,
- do not change current `MeshRasterizer` behavior.

Tests:

- unit-test lossless conversion for current fields,
- unit-test source metadata for mesh cube, FX point, and external point cases.

Acceptance:

- existing unit and e2e tests pass,
- no production caller has changed behavior.

### Phase 2: Extract `PointScalingPolicy`

Code changes:

- add `Policies/PointScalingPolicy.h`,
- replace inline scaling switch in current mesh and FX paths with policy calls,
- keep `MeshRasterizer::ScalingType` as the public compatibility API.

Tests:

- table-driven scaling policy tests,
- existing `TestMeshRasterizer` passes unchanged,
- add FX point-scaling characterization if an FX fixture exists.

Acceptance:

- wave buffers and sampled output match pre-phase baseline within tolerance,
- no UI screenshot difference is expected.

### Phase 3: Extract `DepthProjectionPolicy`

Code changes:

- add `Policies/DepthProjectionPolicy.h`,
- move hidden-dimension `ColorPoint` generation behind an explicit enabled
  policy,
- keep this extraction behind current base-state dependencies unless
  `AxisPolicy`, `WrapPolicy`, and minimal guide-curve deformation have already
  been extracted,
- keep `MeshRasterizer::colorPoints` and `RasterizerData::colorPoints` as
  compatibility storage.

Prerequisites:

- the projection policy must receive axis/dimension information explicitly,
- the projection policy must receive the same wrapped and guide-adjusted
  positions that current `calcCrossPoints` uses,
- if those inputs are still base-rasterizer state, document the dependency in
  the policy context rather than hiding it in globals or protected members.

Tests:

- synthetic mesh test for expected color point count and hidden dimensions,
- Waveform2D-style dimensions include Time/Red/Blue hidden projection,
- disabled policy emits no color points.

Visual validation:

- recapture default UI screenshot,
- compare waveform/spectrum 2D depth visuals against Phase 0 baseline.

Acceptance:

- UI screenshot has no unintended visible regression,
- existing panel code still reads `RasterizerData::colorPoints`.

### Phase 4: Extract `InterceptRestrictionPolicy`

Code changes:

- add `Policies/InterceptRestrictionPolicy.h`,
- move range clamping and strict x ordering out of `MeshRasterizer`,
- keep old `restrictIntercepts(...)` as a forwarding compatibility method.

Tests:

- direct duplicate/reversed x tests,
- compare old and new outputs on synthetic cyclic and non-cyclic intercept
  lists,
- existing mesh rasterizer tests pass.

Acceptance:

- no behavior change in `waveX` monotonicity,
- no new assertions in UI screenshot logs.

### Phase 5: Extract `PointListSource` And Migrate `Rasterizer2D`

Code changes:

- add `Sources/PointListSource.h`,
- add `Interpolation/LinearPointInterpolator.h`,
- route `Rasterizer2D` through point-list source plus existing curve/waveform
  logic,
- keep `Rasterizer2D` public API stable.

Tests:

- existing point-list rasterization behavior,
- cyclic and non-cyclic point-list output,
- partial waveform update tests if current coverage is insufficient.

Acceptance:

- no mesh dependencies are introduced,
- current `Rasterizer2D` callers do not change.

### Phase 6: Extract `PaddingPolicy`

Code changes:

- add `Policies/PaddingPolicy.h`,
- move cyclic, non-cyclic, and FX padding into explicit policies,
- keep `MeshRasterizer::padIcpts(...)` and `padIcptsWrapped(...)` forwarding
  to policy implementations.

Tests:

- generated curve triplets match current output for representative lists,
- front/back padding and `paddingSize` match current output,
- FX padding matches current `FXRasterizer`.

Acceptance:

- existing UI and DSP tests pass,
- no sampleability regressions.

### Phase 7: Move FX Off `Mesh`

Code changes:

- add `Sources/VertexListSource.h`,
- add a lightweight FX point provider or adapter,
- convert `FXRasterizer` internals to consume point source data,
- keep temporary `setMesh(Mesh*)` adapter only for existing call sites,
- audit every FX `setMesh` caller and classify it as compatibility adapter,
  direct point provider candidate, or future deletion candidate.

Tests:

- FX rasterization from direct point list,
- compatibility test: old mesh adapter and direct point list produce same
  intercepts, curves, wave buffers, and sampled impulse,
- relevant IrModeller/Waveshaper tests or fixtures.

Acceptance:

- `FxRasterizerFacade` no longer requires a `Mesh` for new code,
- existing `FXRasterizer::setMesh(Mesh*)` remains functional until callers are
  migrated,
- the FX `setMesh` audit is recorded in the implementation PR or a follow-up
  migration note,
- no preset-load or effect UI regression.

### Phase 8: Extract `WaveformBuilder`

Code changes:

- add `Builders/WaveformBuilder.h`,
- add `Policies/CurveResolutionPolicy.h`,
- add `Policies/WaveformBakePolicy.h`,
- move `calcWaveform`, `setResolutionIndices`, and guide-curve bake behavior
  behind the builder,
- keep old `MeshRasterizer::updateCurves()` API.

Tests:

- wave buffers match baseline on synthetic meshes,
- sampler output matches existing output for evenly spaced and interval
  sampling,
- guide-curve-aware buffer sizes and values match current behavior.

Acceptance:

- no scalar `std::<math>` calls introduced in hot loops where `Buffer`/`VecOps`
  equivalents exist,
- all existing tests pass.

### Phase 9: Extract Snapshot Publication

Code changes:

- add `Builders/RasterizerSnapshotBuilder.h`,
- add `Policies/SnapshotPolicy.h`,
- make `makeCopy()` forward to snapshot publication,
- allow non-UI facades to skip snapshot work.

Tests:

- `RasterizerData` contains copied intercepts, curves, color points, buffers,
  `zeroIndex`, and `oneIndex`,
- lock scope remains limited to publication.

Acceptance:

- panels continue to render from `RasterizerData`,
- no UI screenshot regression.

### Phase 10: Introduce Facades Behind Existing Classes

Code changes:

- add `Facades/MeshRasterizerFacade.h`,
- make `MeshRasterizer` own or wrap the facade internally,
- keep public methods stable,
- do not migrate envelope or voice yet.

Tests:

- all existing mesh rasterizer tests,
- Cycle UI screenshot,
- focused UI automation fixture for read-only/default startup.

Acceptance:

- old API remains source-compatible,
- behavior remains equivalent.

### Phase 11: Migrate `GraphicRasterizer`

Code changes:

- add `Facades/GraphicRasterizerFacade.h` if the base facade is not enough,
- move morph-position pulling and axis selection into explicit policies,
- keep `GraphicRasterizer` as the public class for now.

Tests:

- Waveform2D and Spectrum2D color/depth output tests,
- default UI screenshot,
- focused fixture for switching morph axis if available.

Acceptance:

- `colorPoints` still match hidden-dimension expectations,
- no visible 2D/3D panel regression.

### Phase 12: Migrate `VoiceMeshRasterizer`

Code changes:

- add `VoiceChainedPaddingPolicy`,
- compose mesh slicing, guide curves, wrapping, and waveform building,
- keep audio-thread allocation behavior unchanged.

Tests:

- existing voice/audio tests,
- offline audio capture fixture,
- continuity tests across chained calls.

Acceptance:

- no audio clicks/regressions in capture metrics,
- no allocation added to audio hot path.

### Phase 13: Migrate `EnvRasterizer`

Code changes:

- add envelope source, marker, and padding policies,
- keep render state machine in the envelope facade,
- reuse waveform builder and samplers.

Tests:

- envelope normal, loop, release, and one-sample-per-cycle tests,
- existing preset round-trip tests,
- offline audio capture fixture for envelope-driven sound.

Acceptance:

- envelope render output matches baseline within tolerance,
- loop/release state transitions match existing behavior.

## Composer Migration

The first migration sequence drains specialized behavior out of subclasses into
facades and policies. The next sequence changes the ownership model: subclasses
should become compatibility adapters over a composed rasterization pipeline.

The target configuration style is:

```cpp
auto rasterizer = RasterizerComposer::mesh()
    .withSource(MeshCubeSource(mesh))
    .withSlicer(TrilinearMeshSlicer())
    .withMorphProvider(GraphicMorphPositionPolicy(...))
    .withPadding(CyclicPaddingPolicy(...))
    .withSnapshot(RasterizerDataSnapshot())
    .build();
```

This builder API is the desired direction, but migration phases must keep
existing public rasterizer classes buildable and behavior-compatible until
callers have moved to narrower interfaces.

### Phase 14: Introduce `RasterizationRequest`

Code changes:

- add `RasterizationRequest` as the immutable render input/config shape,
- include dimensions, morph position, x limits, scaling mode, wrapping,
  low-resolution, depth-projection, and snapshot flags,
- add conversion helpers from the current `MeshRasterizer` state,
- keep existing fields on `MeshRasterizer` for compatibility.

Tests:

- request defaults are explicit and match current rasterizer defaults,
- conversion from `MeshRasterizer` state preserves values,
- existing mesh rasterizer characterization tests remain unchanged.

Acceptance:

- no caller is forced to construct the new request yet,
- no behavior changes.

### Phase 15: Introduce `RasterizerRuntime`

Code changes:

- add a runtime/state bundle for mutable pipeline outputs:
  intercepts, curves, color points, front/back padding, guide-curve regions,
  waveform buffers, sample indices, and sampleability state,
- provide a compatibility view over existing `MeshRasterizer` storage first,
- do not move buffer ownership until tests prove equivalence.

Tests:

- runtime view exposes the same vectors/buffers as `MeshRasterizer`,
- snapshot publication can consume runtime data,
- existing sampling tests remain unchanged.

Acceptance:

- runtime shape is available to pipeline code,
- `MeshRasterizer` still owns storage for this phase.

### Phase 16: Introduce `RasterizationPipeline`

Code changes:

- add a composed pipeline object that orchestrates:
  source, slicer/interpolator, point processing, restriction, padding,
  curve resolution, waveform building, and snapshot publication,
- begin with a point-list pipeline because it has the smallest dependency
  surface,
- add an FX pipeline as the first non-`Mesh` vertex-list pipeline; it should
  consume `VertexListSource`, apply FX scaling, restrict x values, use
  `FxPaddingPolicy`, and bake the waveform without requiring a `Mesh`,
- share common waveform transfer-table setup through a small builder/helper
  rather than duplicating table initialization per pipeline,
- keep policy objects stateless or explicitly configured.

Tests:

- point-list pipeline output matches `Rasterizer2D`,
- FX vertex-list output still matches `FXRasterizer`,
- empty and single-point inputs become unsampleable without assertions.

Acceptance:

- a non-subclass pipeline can render a simple point-list waveform,
- existing rasterizers still use their legacy paths.

### Phase 17: Add `RasterizerComposer`

Code changes:

- add a fluent builder that configures a `RasterizationPipeline`,
- support an initial point-list composer and a mesh composer stub,
- make unsupported builder combinations fail at compile time where practical,
  or at construction time with clear assertions during migration.

Tests:

- composer can build a point-list rasterizer with explicit padding,
- composer API supports the desired mesh shape without changing behavior,
- built pipeline remains deterministic.

Acceptance:

- new code can choose composition instead of subclassing for point-list/FX
  rasterization,
- mesh composer API exists as the target seam for later phases.

### Phase 18: Rebuild Plain `MeshRasterizer` On The Pipeline

Code changes:

- first extract mesh slicing into `MeshSlicePipeline`, including cube source,
  default vertex wrapping, point scaling, intercept restriction, and explicit
  depth/color projection,
- characterize `MeshSlicePipeline` against the old `MeshRasterizer` intercept
  and `colorPoints` output before wiring it into runtime behavior; keep the
  first implementation as a non-runtime characterization seam if Cycle teardown
  or preset validation reveals lifecycle coupling,
- make `MeshRasterizer::calcCrossPoints()` assemble a request/runtime view and
  delegate mesh slicing to the pipeline only after the lifecycle coupling is
  understood,
- preserve legacy virtual hooks by adapting them to delegates temporarily,
- leave padding, curve resolution, waveform baking, subclass hooks, and
  snapshot publication on the legacy path until their own characterization
  tests are in place,
- keep `MeshRasterizer` getters and setters source-compatible.

Tests:

- all existing `TestMeshRasterizer` characterization tests,
- `MeshSlicePipeline` output matches `MeshRasterizer` intercepts and
  hidden-dimension `colorPoints`,
- Waveform2D/Spectrum2D panel baseline comparison,
- read-only/default UI automation fixture.

Acceptance:

- plain mesh rasterization behavior is equivalent,
- `MeshRasterizer` no longer directly owns orchestration logic.

### Phase 19: Convert Subclasses To Composer Factories

Code changes:

- first turn `FXRasterizer` into a configured adapter over
  `FxRasterizationPipeline`, because FX can rasterize a direct vertex list
  without depending on cube slicing or `Mesh`,
- expose that path as `RasterizerComposer::fx().withVertices(...).build()` so
  new callers can bypass inheritance immediately,
- then turn `GraphicRasterizer`, `VoiceMeshRasterizer`, and `EnvRasterizer`
  into configured adapters over composer-built pipelines,
- move subclass-specific virtual overrides into source, padding, morph,
  marker, and state-machine delegates,
- keep old class names while callers migrate.

Tests:

- FX direct vertex and mesh-adapter tests,
- graphic panel PSNR baselines,
- voice offline audio capture,
- envelope preset round-trip and audio capture.

Acceptance:

- subclass-specific heavy lifting lives in delegates,
- subclasses no longer need protected access to base pipeline state.

### Phase 20: Introduce Narrow Consumer Interfaces

Code changes:

- add small consumer-facing interfaces such as `RasterizerSampler`,
  `RasterizerSnapshotProvider`, `EnvelopeRenderer`, and `WaveformProvider`,
- migrate callers away from concrete `MeshRasterizer*` where they only need a
  narrow capability,
- keep compatibility adapters at module boundaries.

Tests:

- compile-time migration coverage through touched callers,
- audio and UI fixtures for each migrated capability.

Acceptance:

- most code no longer requires `MeshRasterizer` or a subclass type,
- future rasterizer implementations can be composed without inheriting.

### Phase 21: Retire Subclass Overrides

Code changes:

- delete virtual hooks that have delegate equivalents,
- remove subclasses that are now only named configurations,
- keep type aliases or factory functions where public names are still useful.

Tests:

- full test suite,
- standalone build,
- focused UI automation and audio capture fixtures,
- panel PSNR baselines where screenshot capture is available.

Acceptance:

- rasterization specialization is expressed through composition,
- `MeshRasterizer` is no longer a god object and can be removed or retained as
  a small compatibility shell.

### Phase 22: Extract Rasterizer Storage

Intent:

- make shared rasterizer state explicit and movable before deleting the
  compatibility shell,
- stop using `MeshRasterizer` itself as the owner of waveform, intercept,
  curve, color, guide-region, and snapshot storage.

Code changes:

- add `Rasterization/State/RasterizerStorage.h`,
- move the obvious state bundles into named structs:
  - `RasterizerInterceptStorage`: `icpts`, `frontIcpts`, `backIcpts`,
    `colorPoints`,
  - `RasterizerCurveStorage`: `curves`, guide-curve regions, resolution state
    needed to rebuild curves,
  - `RasterizerWaveformStorage`: `WaveformBuffers` plus zero/one indices and
    sampleability state,
  - `RasterizerSnapshotStorage`: `RasterizerData` and snapshot-publication
    scratch state,
- initially let `MeshRasterizer` own one `RasterizerStorage` member and expose
  its existing public getters as forwarding accessors,
- avoid moving algorithms in this phase except where a helper constructor hides
  mechanical member copying,
- keep public class names, function signatures, and data flow stable.

Tests:

- compile-time coverage through existing callers,
- `TestMeshRasterizer` characterization tests,
- `WaveformBuffers` and snapshot adapter tests,
- full `ctest --test-dir build/tests --output-on-failure`.

Acceptance:

- storage can be passed as `RasterizerStorage&` to policies and adapters,
- `MeshRasterizer` no longer contains unrelated storage bundles as loose
  member fields,
- all existing UI and audio behavior remains unchanged.

### Phase 23: Extract Rasterizer Controller

Intent:

- move provider dispatch and pipeline orchestration out of the compatibility
  class,
- leave `MeshRasterizer` as a thin owner/adapter over storage plus controller.

Code changes:

- add `Rasterization/RasterizerController.h`,
- move provider members from `MeshRasterizer` into a controller configuration:
  - cross-point provider,
  - cleanup provider,
  - padding provider,
  - intercept-processing provider,
  - mesh-assignment provider,
  - dimension and availability providers,
  - offset-seed provider,
- make the controller operate on:
  - `RasterizerStorage&`,
  - `RasterizationRequest`,
  - `RasterizerRuntime`,
  - explicit helper delegates for guide-curve application and waveform baking
    where those are not yet fully decoupled,
- keep `MeshRasterizer` setters such as `setCrossPointProvider(...)` as
  forwarding compatibility APIs,
- move `calcCrossPoints`, `cleanUp`, `padIcpts`, `processIntercepts`,
  `updateCurves`, and `updateOffsetSeeds` dispatch into controller methods
  where feasible,
- keep any still-coupled waveform helper private in `MeshRasterizer` only until
  it has a named policy/controller dependency.

Tests:

- focused tests for each provider dispatch path,
- existing FX, point-list, voice, and envelope compatibility tests,
- `cycle-agent-africanhorn-icycle-preset-switch`,
- `cycle-agent-midi-note`,
- full `ctest`.

Acceptance:

- `MeshRasterizer` mostly forwards to `RasterizerController`,
- new composed rasterizers can own a controller without inheriting from
  `MeshRasterizer`,
- no behavior change in existing adapters.

### Phase 24: Compose Concrete Rasterizers Without Inheritance

Intent:

- prove the replacement shape by making at least one concrete rasterizer own the
  storage/controller pair directly,
- avoid a risky all-at-once deletion of the legacy class.

Code changes:

- choose the lowest-risk concrete class first, preferably `FXRasterizer` or
  `Rasterizer2D`,
- replace `public MeshRasterizer` inheritance with owned
  `RasterizerStorage` + `RasterizerController`,
- expose only the methods that real callers need, forwarding to the composed
  controller/storage,
- keep a temporary adapter if a caller still requires `MeshRasterizer*`,
- repeat for the other concrete rasterizers in this order unless evidence
  suggests otherwise:
  1. `Rasterizer2D`,
  2. `FXRasterizer`,
  3. `GraphicRasterizer` / `E3Rasterizer`,
  4. `VoiceMeshRasterizer`,
  5. `EnvRasterizer`,
- keep each class migration in its own commit.

Tests:

- class-specific unit tests for the migrated rasterizer,
- affected UI fixture:
  - FX: broader controls plus Waveshaper/IrModeller coverage,
  - point-list: `TestPointListRasterizer`,
  - graphic: Spectrum/Waveform panel fixtures and PSNR baselines,
  - voice: `cycle-agent-midi-note` and audio capture where available,
  - envelope: preset switch, MIDI note, envelope loop/release tests,
- full `ctest` after each concrete class migration.

Acceptance:

- at least one production rasterizer no longer derives from `MeshRasterizer`,
- temporary adapters are narrow and explicitly marked as compatibility only,
- validation remains incremental and reversible.

### Phase 25: Migrate Callers To Narrow Interfaces

Intent:

- remove the need for callers to traffic in `MeshRasterizer*` when they only
  need one capability,
- prevent the compatibility shell from re-forming as a new god interface.

Code changes:

- introduce narrow interfaces only when a real caller requires polymorphism:
  - `RasterizerWaveformProvider` / `IWaveformSampler` for sampling,
  - `RasterizerSnapshotProvider` for UI panels that read published data,
  - `MeshBindableRasterizer` only where a real `Mesh*` binding is required,
  - `EnvelopeRenderer` for envelope render/simulate calls,
  - `RasterizerUpdateTarget` only if updater integration needs it,
- migrate call sites from concrete `MeshRasterizer*` to the smallest required
  interface,
- delete compatibility casts and downcasts as they become unnecessary,
- keep ownership boundaries explicit: UI panels should read snapshots or
  waveform providers, not mutate full rasterizer state.

Tests:

- compile-time coverage for migrated call sites,
- focused fixtures for each migrated UI/audio path,
- preset round-trip test to catch serialization side effects,
- full `ctest`.

Acceptance:

- most call sites no longer mention `MeshRasterizer`,
- FX and point-list paths do not depend on `Mesh`,
- envelope and voice paths depend on their domain interfaces rather than base
  protected state.

### Phase 26: Split Interactor Rasterizer Binding

Intent:

- remove the largest remaining non-domain use of `MeshRasterizer*`,
- let panels and interactors depend on the precise capabilities they need:
  mesh binding, update dispatch, sampling, and snapshot reading.

Code changes:

- introduce narrow interfaces only where the existing interactor surface proves
  they are necessary:
  - `MeshBindableRasterizer` for `setMesh`, `getMesh`, and mesh-availability
    queries,
  - `RasterizerUpdateTarget` for `performUpdate`, `reset`, and optional
    lifecycle hooks,
  - `RasterizerVertexDomain` or equivalent only if wrapping, padding size, and
    dimensional setup cannot live on an existing binding interface,
- update `Interactor` to store those narrow interface pointers instead of one
  `MeshRasterizer*`,
- keep `getRasterizer()` only as a temporary compatibility accessor for callers
  that have not yet migrated, and mark the callers in this TDD as remaining
  work,
- keep `RasterizerSampler` and `RasterizerSnapshotProvider` as the read-only UI
  channels for panels,
- remove casts in callers as their required capability becomes explicit.

Tests:

- compile-time coverage for all interactor callers,
- `cycle-agent-africanhorn-icycle-preset-switch`,
- `cycle-agent-midi-note`,
- panel PSNR baselines for the bongo/default rasterizer-heavy preset suite,
- full `ctest`.

Acceptance:

- `Interactor` no longer stores a single all-powerful `MeshRasterizer*` for new
  code paths,
- effect panels can bind FX rasterization through narrow interfaces,
- remaining compatibility accessors are explicitly enumerated and scoped.

### Phase 27: Remove FXRasterizer Inheritance

Intent:

- finish the FX path that Phase 24 and Phase 25 prepared,
- prove that a production UI/audio rasterizer can be composed directly without
  `MeshRasterizer` while still supporting mesh editing and waveform sampling.

Code changes:

- make `FXRasterizer` own:
  - `RasterizerStorage`,
  - `RasterizerController` if provider dispatch remains useful,
  - `FxRasterizerAdapter` or the composed FX pipeline directly,
- implement the narrow interfaces required by `EffectPanel`,
  `Waveshaper`, and `IrModeller`,
- remove `public MeshRasterizer` from `FXRasterizer`,
- delete FX-only provider plumbing that only existed to drive the
  `MeshRasterizer` base,
- keep any temporary adapter from FX to an interactor interface in a clearly
  named compatibility file, not in the core pipeline.

Tests:

- `TestFXRasterizerPointSource`,
- `TestPaddingPolicy` FX coverage,
- Waveshaper and IrModeller UI automation or focused state fixtures,
- `cycle-agent-africanhorn-icycle-preset-switch`,
- full `ctest`.

Acceptance:

- `FXRasterizer` no longer derives from `MeshRasterizer`,
- FX audio consumers depend on `WaveformProvider`,
- FX UI mesh editing does not require a full mesh-rasterizer base pointer.

### Phase 28: Replace Graphic Rasterizer Subclasses

Intent:

- move the visual time/spectrum/phase rasterizers to composed graphic
  pipelines,
- keep visual regression detection tight because this phase touches the most
  visible OpenGL panels.

Code changes:

- make `GraphicRasterizer` own storage/controller/facade dependencies directly,
- move remaining graphic-specific behavior into named collaborators:
  - axis/morph position policy,
  - mesh source and trilinear slicer,
  - hidden-dimension depth projection,
  - guide-curve application,
  - snapshot publication,
- remove inheritance from `GraphicRasterizer`,
- update `E3Rasterizer` separately after the core graphic rasterizer is stable;
  it may become a batch/column renderer over a composed graphic or envelope
  facade instead of a mesh-rasterizer subclass,
- remove `static_cast<MeshRasterizer*>` usages in `VisualDsp` and
  `Spectrum3D`.

Tests:

- `TestGraphicRasterizerPolicies`,
- mesh slice and depth projection tests,
- bongo/default panel PSNR baselines, including magnitude-to-phase switching,
- `cycle-agent-africanhorn-icycle-preset-switch`,
- full `ctest`.

Acceptance:

- `GraphicRasterizer` and `E3Rasterizer` no longer inherit from
  `MeshRasterizer`,
- Spectrum2D/Spectrum3D/Waveform2D/Waveform3D still publish equivalent
  snapshots,
- visual baselines stay within the established PSNR threshold.

### Phase 29: Replace Voice Rasterizer Subclass

Intent:

- remove the audio-thread `VoiceMeshRasterizer` dependency on inherited mutable
  base state,
- keep the change separately verifiable with MIDI automation.

Code changes:

- make `VoiceMeshRasterizer` own composed voice pipeline dependencies directly,
- keep chained intercept windows, curve-resolution behavior, and waveform
  update gating inside voice-specific policies/facades,
- implement only the interfaces required by synth voice callers and
  `Multisample`,
- migrate `SampleUtils` and `PitchedSample` away from `MeshRasterizer*` where
  they only need waveform sampling or period creation,
- remove inherited protected-state access from voice code.

Tests:

- `TestVoiceRasterizerFacade`,
- `cycle-agent-midi-note`,
- offline audio capture if available,
- preset round-trip,
- full `ctest`.

Acceptance:

- `VoiceMeshRasterizer` no longer inherits from `MeshRasterizer`,
- MIDI note automation remains clean,
- voice/chaining behavior remains covered by policy tests and one end-to-end
  automation fixture.

### Phase 30: Replace EnvRasterizer Subclass

Intent:

- migrate the most stateful rasterizer last, after storage, controller,
  waveform, interactor, FX, graphic, and voice seams are proven.

Code changes:

- make `EnvRasterizer` own the envelope facade and render/playback policies
  directly,
- introduce an `EnvelopeRenderer` interface for callers that simulate or render
  envelope output,
- migrate `EnvelopeInter2D`, `PlaybackPanel`, and envelope 3D paths from
  `MeshRasterizer*` to envelope-specific interfaces,
- keep sustain/loop/release marker behavior in envelope policy groups,
- remove remaining inherited base helper calls from envelope rendering.

Tests:

- `TestEnvRasterizerFacade`,
- envelope loop/release tests,
- `cycle-agent-africanhorn-icycle-preset-switch`,
- `cycle-agent-midi-note`,
- preset round-trip,
- full `ctest`.

Acceptance:

- `EnvRasterizer` no longer inherits from `MeshRasterizer`,
- envelope display, simulation, and render-time paths use envelope interfaces,
- no remaining production concrete rasterizer inherits from `MeshRasterizer`.

Implemented state:

- `EnvRasterizer` owns a contained compatibility rasterizer and forwards narrow
  mesh binding, sampling, snapshot, update, guide-curve, and vertex-domain
  interfaces,
- `EnvelopeInter2D`, `EnvelopeDelegate`, `PlaybackPanel`, sample import, and
  preset/bootstrap paths no longer require `EnvRasterizer` to be a
  `MeshRasterizer*`,
- validation used focused envelope tests, preset round trip coverage,
  `cycle-agent-africanhorn-icycle-preset-switch`, `cycle-agent-midi-note`,
  standalone build, and full `ctest`.

### Phase 31: Rename Or Delete The Compatibility Shell

Intent:

- make the final architectural state explicit: either no `MeshRasterizer`, or a
  clearly named legacy adapter with no ownership of core rasterization logic.

Code changes:

- if no callers need it, delete `MeshRasterizer`,
- otherwise rename it to `LegacyMeshRasterizerAdapter` or similar and keep it
  out of new code paths,
- move remaining reusable behavior into:
  - `RasterizerStorage`,
  - `RasterizerController`,
  - named policies,
  - concrete composer factories,
- remove provider setters that only existed for subclass migration once no
  compatibility adapter uses them,
- update ADR 002 and this TDD with the completed target hierarchy.

Tests:

- full test suite,
- standalone build,
- plugin build if the deleted API crosses plugin targets,
- focused UI automation fixtures,
- panel PSNR baselines for bongo/default rasterizer-heavy presets,
- offline audio capture for voice/envelope-sensitive changes.

Acceptance:

- no concrete rasterizer inherits from `MeshRasterizer`,
- reusable rasterization behavior is composed through storage, controller,
  policies, and sources,
- deleted or renamed compatibility shell cannot be used accidentally for new
  rasterizer work.

Implemented state:

- no production concrete rasterizer inherits from `MeshRasterizer`,
- `MeshRasterizer` is retained under its current name as a compatibility shell
  because a few direct legacy callers still own plain rasterizers or need
  legacy render-state helpers,
- new rasterizer-facing call sites use narrow interfaces, domain rasterizer
  classes, composer/pipeline components, storage/runtime views, or policies.

### Phase 32: Final Cleanup Review

Intent:

- pay down migration scaffolding and navigation cost after the behavioral
  migration is complete,
- make the new rasterization tree discoverable without needing to know exact
  class names.

Code changes:

- review every file under `Rasterization/` and classify it as:
  - permanent public interface,
  - permanent source/interpolation/builder/sampling component,
  - domain-specific facade/pipeline,
  - temporary compatibility adapter,
  - obsolete migration scaffolding,
- delete obsolete adapters, duplicate facade wrappers, unused provider setters,
  and one-off tests that only characterized deleted compatibility paths,
- consolidate policy files into logical groups where that improves navigation:
  - `Policies/Core/`: scaling, wrapping, sorting, restriction, degeneracy,
    cleanup,
  - `Policies/Curves/`: curve resolution, waveform preparation, waveform bake,
    padding flags,
  - `Policies/Mesh/`: guide curves, component-guide sharpness, depth
    projection, mesh-slice output,
  - `Policies/Envelope/`: marker, playback, release, render timing, state
    validation, sustain point, envelope padding,
  - `Policies/FX/`: FX padding/scaling adapters if they remain distinct,
  - `cycle/src/Curve/Rasterization/Policies/Graphic/` and
    `cycle/src/Curve/Rasterization/Policies/Voice/` for Cycle-only graphic and
    voice policies,
- add short umbrella headers or README files for the policy groups if they make
  discovery easier without creating another god include,
- update includes mechanically and keep moves in dedicated commits to make
  review easy,
- update ADR 002 and this TDD with the final file hierarchy and any
  intentionally retained compatibility layer.

Tests:

- build after each mechanical move group,
- full `ctest`,
- standalone build,
- plugin build if public include paths changed,
- focused UI automation fixtures,
- bongo/default panel PSNR baselines.

Acceptance:

- policy files are grouped by domain and responsibility rather than living in
  one long flat directory,
- no unused migration adapters/classes remain,
- the final hierarchy communicates where to look even when the exact class name
  is unknown,
- ADR 002 and this TDD match the implemented architecture.

Implemented state:

- shared policies are grouped under `Policies/Core`, `Policies/Curves`,
  `Policies/Mesh`, and `Policies/Envelope`,
- Cycle-only policies are grouped under `Policies/Graphic` and `Policies/Voice`,
- short README files at both policy roots describe the grouping boundary,
- ADR 002 and this TDD describe the retained compatibility shell and final
  policy hierarchy.

### Phase 33: Remove Direct Legacy Ownership From Leaf Callers

Intent:

- remove the remaining low-risk direct `MeshRasterizer` ownership and enum
  leakage from concrete leaf callers,
- keep compatibility internals hidden behind narrow domain facades while the
  heavier batch paths are extracted separately.

Code changes:

- wrap `SynthFilterVoice`'s magnitude and phase filter rasterizers in a
  `SpectralFilterRasterizer` facade that exposes only:
  - magnitude/phase configuration,
  - morph position and noise seed,
  - mesh cross-point calculation at osc phase zero,
  - sampleability and interval sampling,
- wrap `VisualDsp`'s osc-phase helper in an `OscPhaseRasterizer` facade with a
  single `rasterize(Mesh*)` operation,
- replace uses of `MeshRasterizer` enum constants in `FXRasterizer` callers
  with the owning rasterizer's own enum or a neutral scaling policy type,
- avoid changing the generated wave, spectrum, or envelope data in this phase.

Tests:

- `cmake --build --preset tests --target Cycle_tests`,
- `cmake --build --preset standalone-debug --target Cycle`,
- focused rasterizer/voice test subset,
- `cycle-agent-midi-note`,
- filtered launch logs inspected for assertions/crashes.

Acceptance:

- `SynthFilterVoice` no longer includes or owns `MeshRasterizer` directly,
- `VisualDsp` no longer owns a plain osc-phase `MeshRasterizer`,
- FX callers do not reference `MeshRasterizer::ScalingType` for FX scaling,
- voice MIDI automation remains clean.

### Phase 34: Hide Graphic Render-State Compatibility

Intent:

- stop leaking `MeshRasterizer::RenderState` and
  `MeshRasterizer::ScopedRenderState` into graphic callers,
- make `GraphicRasterizer` the compatibility boundary for batch rendering
  until the batch paths have their own composed renderer.

Code changes:

- add `GraphicRasterizer::RenderState`,
  `GraphicRasterizer::ScopedRenderState`, and named graphic scaling values,
- add helpers for preserving/restoring state and constructing batch render
  state,
- update `VisualDsp` batch setup to use `GraphicRasterizer` helpers rather
  than direct `MeshRasterizer` types,
- replace direct `legacyRasterizer()` usage in the time-domain batch loop with
  `GraphicRasterizer` forwarding methods,
- use `Rasterization::GuideCurveContext` directly where guide-curve sampling
  context is needed.

Tests:

- focused rasterizer/graphic test subset,
- standalone build,
- `cycle-agent-midi-note`,
- bongo/default panel PSNR fixture if the change touches rendered output
  rather than only type boundaries.

Acceptance:

- `VisualDsp` does not mention `MeshRasterizer` for render-state setup,
- remaining `legacyRasterizer()` usage is explicitly limited to paths that
  still need deeper batch extraction,
- rendered panels are behavior-compatible.

### Phase 35: Extract `VisualDsp` Batch Rasterizers

Intent:

- move the densest remaining visual-DSP rasterization logic out of
  `VisualDsp`,
- make time and frequency column rendering testable without using
  `VisualDsp` as the owner of all scratch state,
- reduce duplication between time, magnitude, phase, and unison batch paths.

Code changes:

- add small batch renderers under `cycle/src/Curve/Rasterization/` or
  `cycle/src/UI/VisualDsp/` only if they are truly UI-owned:
  - `TimeColumnRasterizer`,
  - `FrequencyColumnRasterizer`,
  - `UnisonPhaseColumnRenderer` or equivalent if the unison path remains
    distinct,
- factor shared column inputs into explicit context structs:
  - layer group,
  - morph axis,
  - zoom progress,
  - scratch context lookup,
  - FFT/log-region buffers,
  - pan and unison options,
- keep `VisualDsp` responsible for orchestration, locks, and publication of
  column vectors,
- keep the first extraction behavior-preserving; do not rewrite the math and
  the ownership boundary in the same step,
- remove remaining `legacyRasterizer()` calls from `VisualDsp` once the batch
  renderers own the compatibility boundary.

Tests:

- add unit or characterization tests for the new batch renderer context where
  practical,
- bongo/default cropped panel PSNR baselines for Waveform2D, Spectrum2D,
  Spectrum3D magnitude, and Spectrum3D phase,
- `cycle-agent-spectrum-mode-switch`,
- `cycle-agent-africanhorn-icycle-preset-switch`,
- full `ctest`.

Acceptance:

- `VisualDsp` no longer contains the full mesh-rasterization inner loops,
- remaining visual batch logic is organized around explicit renderer/context
  shapes,
- no visual regression in panel baselines.

Implemented state:

- the time-domain column mesh-rasterization loop is extracted into
  `Cycle::Rasterization::TimeColumnRasterizer`,
- the pitch/unison phase-shift column pass is extracted into
  `Cycle::Rasterization::UnisonPhaseColumnRenderer`,
- `VisualDsp` still owns allocation, locks, and column publication,
- magnitude/phase frequency column rendering remains to be extracted.

### Phase 36: Migrate Remaining Library Callers To Narrow Interfaces

Intent:

- remove non-UI library dependencies on concrete `MeshRasterizer*`,
- prevent compatibility-shell requirements from leaking back into audio and
  sample utilities.

Code changes:

- audit and migrate:
  - `Multisample`,
  - `PitchedSample`,
  - sample-import period creation,
  - any audio utility that only needs waveform sampling,
- replace concrete rasterizer pointers with the smallest existing interface:
  - `WaveformProvider`,
  - `RasterizerSampler`,
  - `RasterizerSnapshotProvider`,
  - `MeshBindableRasterizer` only where mesh binding is actually required,
- add adapter methods only at module boundaries and name them as compatibility
  adapters,
- avoid broadening the existing interfaces to match `MeshRasterizer`.

Tests:

- compile-time coverage from affected callers,
- sample import/period creation tests if available,
- `cycle-agent-midi-note`,
- offline audio capture fixture where the changed path affects render output,
- full `ctest`.

Acceptance:

- library audio/sample utilities do not require `MeshRasterizer*` unless they
  genuinely bind a mesh,
- voice and sample paths depend on waveform or sampling capabilities rather
  than base rasterizer state.

Implemented state:

- `Multisample` and `PitchedSample` no longer expose concrete
  `MeshRasterizer*` overloads,
- current production callers pass `MeshBindableRasterizer`,
  `RasterizerUpdateTarget`, and `RasterizerSampler` explicitly.

### Phase 37: Complete Interactor Binding Split

Intent:

- finish separating mesh editing from sampling/snapshot/update access,
- remove the largest UI-side reason for keeping `MeshRasterizer` as a concrete
  common type.

Code changes:

- update `Interactor` storage to hold explicit capability pointers instead of
  one concrete rasterizer pointer,
- migrate callers of `Interactor::getRasterizer()` to the narrow capability
  they actually use,
- preserve `getRasterizer()` only as a temporary compatibility accessor while
  the last callers are being migrated, and enumerate those callers in this
  phase before deleting it,
- update mesh-selection clients and keyboard/automation helpers to consume the
  narrow interfaces.

Tests:

- compile-time coverage for all interactor clients,
- mesh selection gesture fixture,
- bongo/default panel PSNR baselines,
- `cycle-agent-africanhorn-icycle-preset-switch`,
- full `ctest`.

Acceptance:

- new interactor code does not store or request `MeshRasterizer*`,
- mesh editing still works for graphic, envelope, and FX panels,
- remaining compatibility callers are explicitly listed or deleted.

Implemented state:

- `Interactor` no longer stores a concrete `MeshRasterizer*`,
- the old `setRasterizer(MeshRasterizer*)` and `getRasterizer()` compatibility
  accessors are removed,
- 3D detail-reduction callers now use `getMeshBindableRasterizer()` and
  dynamic-cast only for the optional detail capability.

### Phase 38: Consolidate Facades, Policies, And Compatibility Adapters

Intent:

- reduce the line-count and navigation cost introduced during migration,
- keep reusable concepts reusable and delete one-off wrappers that only served
  an intermediate phase,
- verify that the new narrow rasterization interfaces are genuinely useful and
  have not simply recreated `MeshRasterizer` as several always-used fragments,
- evaluate current implementation against the desired composer configuration
  style and record the remaining delta.

Code changes:

- audit every new rasterization class for one of these outcomes:
  - keep as a permanent domain abstraction,
  - merge into a nearby facade because it has only one caller and no test seam,
  - move into a policy group README as documented behavior rather than code,
  - delete as obsolete compatibility scaffolding,
- look specifically for duplicated context structs, forwarding methods,
  waveform-copy helpers, and policy wrappers that differ only by enum naming,
- group tiny related policies only when grouping improves discovery without
  creating a new god header,
- prefer deleting code over creating umbrella abstractions that hide important
  specialization details,
- audit each rasterization interface and classify it as:
  - **kept-disjoint**: at least one real caller needs this capability without
    needing most of the others,
  - **compatibility-only**: implemented broadly today only because the
    migration is draining legacy callers,
  - **merge candidate**: overlaps another interface enough that separate
    existence creates noise,
  - **delete candidate**: no production caller remains after compatibility
    adapters are removed.

Interface audit questions:

- Does the caller need to bind or inspect a `Mesh`, or only sample an existing
  waveform?
- Does the caller need update lifecycle (`cleanUp`, `performUpdate`, `reset`),
  or is lifecycle owned by a surrounding facade?
- Does the caller need panel snapshot data, or only numeric samples?
- Does the caller need vertex-domain mutation (`setDims`, wrapping, padding
  size), or should that be construction-time composer configuration?
- Is guide-curve binding a global registration concern, a construction-time
  dependency, or a runtime mutator?
- If nearly every remaining concrete rasterizer still implements an interface,
  is that because the domain genuinely needs it, or because an old caller has
  not yet been migrated?

Current interface expectations to verify:

- `WaveformProvider` should be kept only if audio/effect consumers such as
  Waveshaper and IrModeller can depend on waveform rendering without mesh
  binding, snapshots, or panel lifecycle.
- `RasterizerSampler` should be kept only if there are callers that need random
  sampling of an already-baked waveform without update ownership.
- `RasterizerSnapshotProvider` should remain panel/read-only UI facing and
  should not imply mesh binding or mutation.
- `MeshBindableRasterizer` should remain only for mesh editing/import paths,
  not for FX point-list or audio-only consumers.
- `RasterizerUpdateTarget` should remain only where the updater/interactor
  truly owns lifecycle dispatch.
- `RasterizerVertexDomain` is a likely merge or redesign candidate because it
  groups wrapping, padding size, guide-provider access, and dimension mutation;
  decide whether those belong in construction-time options, a domain descriptor,
  or separate narrow capabilities.
- `GuideCurveBindableRasterizer` should be checked against `SingletonRepo`
  registration. If guide-curve provider binding is no longer global mutation,
  prefer construction-time dependencies or composer configuration.

Composer target audit:

- compare current implementation to the target shape:

  ```cpp
  auto rasterizer = RasterizerComposer::mesh()
      .withSource(MeshCubeSource(mesh))
      .withSlicer(TrilinearMeshSlicer())
      .withMorphProvider(GraphicMorphPositionPolicy(...))
      .withPadding(CyclicPaddingPolicy(...))
      .withSnapshot(RasterizerDataSnapshot())
      .build();
  ```

- document which builder hooks exist today, which are stubs, and which are
  still hidden behind compatibility classes,
- verify whether `withSource(...)` accepts lightweight non-`Mesh` sources for
  FX/point-list paths and mesh cube sources for graphic/voice paths,
- verify whether `withSlicer(...)` has a real mesh slicer implementation or
  whether trilinear logic still lives in `VertCube`/legacy helpers,
- verify whether morph position is construction-time policy, per-render request
  data, or still pulled from panels inside a compatibility class,
- verify whether padding is an explicit policy choice or still implied by
  rasterizer type and mutable flags,
- verify whether snapshot publication is optional and explicit, especially for
  non-UI/audio paths,
- identify the next smallest production caller that can be built directly from
  the composer rather than through a legacy facade.

Tests:

- build after each mechanical deletion/move group,
- full `ctest`,
- standalone build,
- affected focused UI fixtures,
- PSNR baselines if files moved across graphic render paths.

Acceptance:

- rasterization LOC decreases or the remaining lines have clear ownership,
- no duplicate policy/facade pair exists without a reason documented in the
  TDD or a README,
- the directory hierarchy remains navigable by domain and responsibility,
- each remaining rasterization interface has at least one concrete caller story
  that does not require most of the other interfaces, or it is marked for merge
  or deletion,
- the TDD records a composer gap analysis: implemented hooks, missing hooks,
  and the next concrete caller to migrate to the desired configuration style.

Implemented state:

- mesh composer exposes target-style hooks for source, slicer, morph position
  or morph provider, padding policy, and snapshot policy,
- the hooks currently map onto `RasterizationRequest`; shared snapshot-source
  publication is available through `ComposedMeshWaveformRasterizer`,
- the `GraphicRasterizer::legacyRasterizer()` escape hatch is removed after
  migrating the last production caller,
- `OscPhaseRasterizer` and `SpectralFilterRasterizer` no longer own
  compatibility `MeshRasterizer` instances; they build from the composed mesh
  slicer and, for spectral filters, the point-list waveform pipeline,
- `E3Rasterizer`, `GraphicRasterizer`, `VoiceMeshRasterizer`, and
  `EnvRasterizer` no longer own compatibility `MeshRasterizer` instances,
- `ComposedMeshWaveformRasterizer` now provides the shared mesh-slice to
  waveform primitive, including snapshot-source publication and target-style
  guide-curve applier creation for domain pipelines,
- `VoiceMeshRasterizer` keeps the voice-specific chained pipeline and storage
  directly, rather than routing chained audio through a generic compatibility
  rasterizer,
- `EnvRasterizer` owns explicit envelope request, storage, runtime, guide-curve
  offset seeds, waveform baking, render-time padding, decoupled guide sampling,
  and snapshot publication directly,
- interface audit has removed concrete audio/import and interactor
  compatibility paths that no longer had production callers.

### Phase 39: Retire Or Rename `MeshRasterizer`

Intent:

- make accidental new usage of the compatibility shell difficult,
- finish the migration from inheritance reuse to composition.

Code changes:

- after Phases 35-37 remove the last production direct callers,
- choose one final outcome:
  - delete `MeshRasterizer` if tests and callers no longer need it, or
  - rename it to `LegacyMeshRasterizerAdapter` if characterization tests or a
    small compatibility path still need it,
- move any remaining reusable pieces into storage, controller, policies,
  sources, slicers, samplers, or composer factories,
- update public includes and comments so new code reaches for the composed
  rasterizer APIs first,
- update ADR 002 with the final decision.

Tests:

- full `ctest`,
- standalone build,
- plugin build if affected public headers cross plugin targets,
- bongo/default PSNR baseline suite,
- `cycle-agent-africanhorn-icycle-preset-switch`,
- `cycle-agent-midi-note`,
- audio capture fixture.

Acceptance:

- no production concrete rasterizer derives from or owns `MeshRasterizer`,
- either no `MeshRasterizer` type remains, or the retained type is explicitly
  named and documented as legacy-only,
- new rasterizer construction goes through composer/facade/domain types.

Implemented state:

- the smallest audio/phase compatibility facades have been drained:
  `OscPhaseRasterizer`, `SpectralFilterRasterizer`, `E3Rasterizer`,
  `GraphicRasterizer`, `VoiceMeshRasterizer`, and `EnvRasterizer`,
- no production concrete rasterizer derives from or owns `MeshRasterizer`,
- `MeshRasterizer` is retained under its current name as a
  characterization-tested compatibility shell for now; production rasterizer
  construction has moved to composer/facade/domain-owned primitives.

### Phase 40: Final Duplication And Regression Review

Intent:

- close the refactor with an explicit reuse, dead-code, and regression audit,
- avoid carrying the migration's temporary scaffolding as permanent architecture.

Code changes:

- run a final search for:
  - direct `MeshRasterizer` mentions,
  - `legacyRasterizer()`,
  - duplicated `WaveformResult`/buffer-copy code,
  - duplicated point scaling, padding, restriction, and snapshot code,
  - compatibility adapters with no production caller,
  - tests that only cover deleted migration seams,
- compare rough rasterization LOC before/after this cleanup slice and explain
  intentional increases,
- update `docs/ADR/002-rasterization-pipeline.md`,
  `docs/TDD/rasterization-pipeline-migration.md`, and policy READMEs with the
  final structure,
- keep any remaining compatibility exceptions in a short explicit list.

Tests:

- full `ctest`,
- standalone build,
- plugin build if available,
- focused UI automation fixtures,
- bongo/default panel PSNR baselines,
- audio capture fixture,
- manual visual review only for cases automation cannot currently inspect.

Acceptance:

- no unused migration scaffolding remains,
- duplication is either removed or documented as deliberate domain separation,
- final docs describe the implemented architecture rather than the migration
  path alone,
- rasterizer changes are ready to stop being treated as an active migration.

Implemented state:

- final production ownership scan found no concrete rasterizer deriving from or
  owning `MeshRasterizer`,
- remaining `MeshRasterizer` mentions are legacy compatibility tests, the
  retained compatibility shell, naming in `MeshRasterizerState`, and domain type
  names such as `VoiceMeshRasterizer`,
- duplicated mesh-to-waveform rendering for graphic, spectral, oscillator phase,
  and E3 paths is consolidated through `ComposedMeshWaveformRasterizer`,
- voice and envelope keep separate domain-owned storage because chaining,
  release, loop, and decoupled guide-curve playback are not the same lifecycle
  as stateless panel mesh slicing,
- parallel Cycle agent UI fixture runs still contend on macOS application
  activation; run those fixtures serially.

## Regression Baselines

### Unit And Integration Baseline

Before each phase:

```sh
cmake --preset tests
cmake --build --preset tests
ctest --test-dir build/tests -V
```

If a phase touches Cycle UI or application code:

```sh
cmake --preset standalone-debug
cmake --build --preset standalone-debug
```

### UI Screenshot Baseline

The default full UI screenshot should be captured before Phase 1 and recaptured
after phases that touch:

- mesh slicing,
- depth/color projection,
- graphic rasterizers,
- snapshot publication,
- curve/waveform generation used by panels.

Baseline command:

```sh
test -f docs/media/images/cycle-default-ui-rasterization-baseline.png \
    || scripts/capture_cycle_ui.sh \
        docs/media/images/cycle-default-ui-rasterization-baseline.png \
        /tmp/cycle-default-ui-rasterization-baseline-logs.txt
```

The full UI screenshot is a broad smoke artifact. It includes the menu bar,
dock, desktop background, window placement, and time-dependent OS state, so it
should not be treated as a strict pixel-diff oracle.

For rasterization work, prefer cropped panel baselines when validating a
candidate change:

- Waveform2D for hidden-dimension color/depth projection,
- Spectrum2D for magnitude/phase 2D raster data,
- Waveform3D for mesh-surface visual continuity when the phase/waveform
  pipeline changes.

Use the agent runner or OS crop path from `docs/cycle-agent-runbook.md` to
capture stable panel targets. The fixture should first inspect target bounds,
then capture only the relevant panel rectangle. Full-window screenshots remain
useful for catching gross layout or launch regressions.

Comparison command:

```sh
scripts/capture_cycle_ui.sh \
    /tmp/cycle-default-ui-rasterization-candidate.png \
    /tmp/cycle-default-ui-rasterization-candidate-logs.txt
```

Inspect filtered logs first:

```sh
sed -n '1,160p' /tmp/cycle-default-ui-rasterization-candidate-logs.txt
```

Use image comparison only as a regression signal. Some platform differences,
window placement changes, and animation timing may cause benign pixel
differences. For `colorPoints`, prefer structural unit tests and use the
screenshot to catch missed visual wiring.

Current baseline:

- image: `docs/media/images/cycle-default-ui-rasterization-baseline.png`,
- dimensions: 3456 x 2234,
- captured preset: factory default `AfricanHorn`,
- filtered log: `/tmp/cycle-default-ui-rasterization-baseline-logs.txt`.

The current filtered log includes an App Transport Security warning for the
HTTP preset-list fetch. That warning is unrelated to rasterization and should
not fail this baseline unless the launch behavior changes.

### Focused UI Automation

For UI-heavy phases, prefer a focused fixture over a broad smoke run. Useful
fixtures:

```sh
scripts/run_cycle_agent.sh \
    scripts/fixtures/cycle-agent-readonly.json \
    /tmp/cycle-agent-readonly-report.json \
    /tmp/cycle-agent-readonly-logs.txt

scripts/run_cycle_agent.sh \
    scripts/fixtures/cycle-agent-waveform3d-state.json \
    /tmp/cycle-agent-waveform3d-state-report.json \
    /tmp/cycle-agent-waveform3d-state-logs.txt
```

For audio-sensitive phases:

```sh
scripts/run_cycle_agent.sh \
    scripts/fixtures/cycle-agent-audio-capture.json \
    /tmp/cycle-agent-audio-capture-report.json \
    /tmp/cycle-agent-audio-capture-logs.txt
```

## Test Strategy By Layer

### Policy Tests

Policy tests should use simple table-driven inputs and avoid constructing a
full `MeshRasterizer`.

Examples:

- y scaling maps values exactly,
- cyclic wrap maps x into range and sets wrapped state,
- clamp policy preserves boundaries,
- restriction policy enforces strict monotonic x,
- padding policy emits expected curve triplets.

### Builder Tests

Builder tests should compare current and new output from the same inputs.

Examples:

- `MeshSliceBuilder` output equals old `calcCrossPoints` intercepts and
  `colorPoints`,
- `WaveformBuilder` output equals old `calcWaveform`,
- snapshot builder output equals old `makeCopy`.

### Facade Compatibility Tests

Facade tests should use the old public class names and APIs. They should prove
that migration did not leak new concepts into existing callers.

Examples:

- `MeshRasterizer` still produces finite monotonic buffers,
- `FXRasterizer::setMesh(Mesh*)` still works until removed,
- `GraphicRasterizer` still publishes `RasterizerData::colorPoints`,
- `EnvRasterizer` render methods remain source-compatible.

### End-To-End Tests

End-to-end validation should focus on risk:

- visual phases: screenshot and read-only UI fixture,
- FX phases: IrModeller/Waveshaper preset and impulse tests,
- voice phases: offline audio capture,
- envelope phases: loop/release render tests and preset round trip.

## Completion Criteria

The migration is complete when:

- `MeshRasterizer` no longer owns all pipeline state directly,
- FX rasterization has a point-list path that does not require `Mesh`,
- `colorPoints` are produced only by an explicit depth projection policy,
- envelope and voice code compose shared builders instead of depending on base
  protected state,
- old public class names either wrap composed internals or are removed after
  all callers migrate; the retained `MeshRasterizer` shell is explicitly
  treated as compatibility-only,
- default UI screenshot and existing e2e fixtures remain stable.

## Open Questions

- Should `RasterPoint` replace `Intercept` broadly, or should it remain only a
  pipeline boundary type while `Curve` keeps `Intercept`?
- Should policy objects be runtime-composed structs, static helper namespaces,
  or small classes with dependency references?
- Should envelope loop/sustain marker handling live in `EnvelopeSliceSource` or
  a separate `EnvelopeMarkerPolicy`?
- Should screenshot baselines be committed as media artifacts, or generated
  locally and compared outside source control after Phase 0?
