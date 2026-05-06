# Rasterization Pipeline Migration TDD

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
  RasterizerResult.h
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
    AxisPolicy.h
    PointScalingPolicy.h
    WrapPolicy.h
    GuideCurvePolicy.h
    DepthProjectionPolicy.h
    InterceptRestrictionPolicy.h
    PaddingPolicy.h
    CurveResolutionPolicy.h
    WaveformBakePolicy.h
    SnapshotPolicy.h

  Builders/
    InterceptBuilder.h
    CurveBuilder.h
    WaveformBuilder.h
    RasterizerSnapshotBuilder.h

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
- add `RasterizerResult.h`,
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
- old public class names either wrap facades or are removed after all callers
  migrate,
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
