# Rasterization V2 Realtime Pipeline

## Status
Proposed

## Date
2026-02-15

## Parity Requirements (Consolidated)

### Non-Negotiable Rule
- V2 must preserve legacy behavior unless explicitly approved otherwise.
- No heuristic substitutions for legacy logic in parity paths.

### Critical Parity Gaps (Open)

#### Voice chaining parity is not complete
- Intercept-stage sequencing parity exists, but full parity is not complete until end-to-end legacy-vs-v2 behavior is validated for all chaining state and padding edge cases, including advancement semantics under non-zero `MinLineLength`.

What must be ported for voice parity:
1. Legacy chaining state machine behavior:
   - first-call behavior vs subsequent-call behavior,
   - advancement semantics,
   - carry of prior intercept sets.
2. Legacy front/back chained padding construction:
   - equivalent front/back intercept derivation policy,
   - equivalent curve triplet assembly order.
3. Legacy boundary continuity characteristics:
   - block-to-block continuity under evolving mesh/morph states.

Mandatory validation for voice:
1. Keep parity tests directly comparing legacy chaining behavior vs V2 for same fixtures:
   - same mesh,
   - same morph evolution sequence,
   - same phase progression.
2. Validate:
   - phase wrap behavior,
   - cycle boundary continuity,
   - block concatenation equivalence where legacy guarantees it,
   - non-zero advancement (`MinLineLength`) behavior.

#### Waveform-piece path/deformer parity is not complete
- Intercept-stage path positioning is in-progress, but parity is not complete until legacy `MeshRasterizer::calcWaveform` component/time deformer behavior is matched.
- Remaining gap includes both non-decoupled path-aware wave construction and decoupled sampler-time path deformation (`deformRegions` / `sampleAtDecoupled` behavior).

### Guardrail for Future Changes
- Any V2 voice chaining change must state:
  - which legacy behavior is being matched,
  - which test proves parity for that behavior.
- Any V2 path/deformer wave behavior change must state:
  - which legacy `calcWaveform` / decoupled behavior is being matched,
  - which test proves parity for that behavior.

## Context
The current rasterization refactor direction is correct at a high level (interpolate -> position -> curve build -> sample), but the implementation has architectural friction:

- Stage contracts are not yet stable.
- Ownership/state boundaries are unclear (especially envelope state).
- Some interfaces imply dynamic allocation or copying in hot paths.
- Realtime constraints require deterministic audio-thread behavior.

This project is a v2 refactor with freedom to redesign call sites and types. We should optimize for a coherent architecture, not incremental compatibility.

## Decision
Adopt a strict, allocation-free-on-audio-thread pipeline design with explicit state and caller-provided storage.

### Core Rules
1. No heap allocation on audio thread.
2. No locks, blocking calls, file/network I/O, or syscalls on audio thread.
3. No hidden global mutable state in stage implementations.
4. Processing functions use caller-provided spans/scratch and return counts/status, not owning containers.
5. All capacity planning and memory reservation happens in prepare/reset/update phases off audio thread.

### Pipeline Model
`MeshSnapshot -> Intercepts -> PositionedIntercepts -> CurvePieces -> WaveTable/View -> AudioBuffer`

Each stage has:
- Input view(s): `std::span<const T>`
- Output view(s): `std::span<T>`
- Mutable state: explicit `State&` argument
- Config: immutable `const Params&`
- Result: `bool` or status enum + output count

Example function shape:

```cpp
bool produceCurvePieces(
    std::span<const Intercept> in,
    std::span<CurvePiece> out,
    size_t& outCount,
    CurveBuildState& state,
    const CurveBuildParams& params) noexcept;
```

## Architecture

### 1) Data Ownership
- `RasterizerInstance` owns all long-lived state and fixed-capacity buffers.
- `RasterizerWorkspace` is preallocated scratch memory for one rasterizer instance.
- `RasterizerPipeline` owns concrete stage objects and wires execution order.

### 2) Stage Interfaces
- `InterpolatorStage`
- `PositionerStage`
- `CurveBuilderStage`
- `WaveBuilderStage`
- `SamplerStage`

Each interface is non-templated where practical to keep compile-time complexity down. Prefer concrete value types for core wireframe domain types (`Intercept`, `CurvePiece`, `DeformRegion`, etc.).

### 3) Stateful Subsystems
- `EnvStateMachine` is a dedicated module (Normal/Looping/Releasing transitions).
- `PathDeformEngine` owns deform region generation and deterministic noise context updates.
- `VoiceChainState` owns continuity/chaining data for oscillator chaining behavior.

### 4) API Boundary
Public rasterizer API should expose explicit phases:

- `prepare(const PrepareSpec&)` (alloc/reserve/reset capacities)
- `setMeshSnapshot(const MeshSnapshot&)`
- `updateControlData(const ControlSnapshot&)`
- `renderBlock(RenderRequest, RenderResult&)`


### 5) Artifact Pipeline Interface (Implemented 2026-02-24)
V2 rasterizers now implement a shared staged interface in `lib/src/Curve/V2/Orchestration/V2RasterizerPipeline.h`:

- `renderIntercepts(V2RasterArtifacts&)`
- `renderWaveform(V2RasterArtifacts&)`
- `renderArtifacts(V2RasterArtifacts&)` (composed default: intercepts then waveform)

`V2RasterArtifacts` is a shared payload for stage outputs. It carries pointers/views for intercepts/curves and waveform buffers (`waveX`, `waveY`, `diffX`, `slope`) plus `zeroIndex`/`oneIndex`.

Counts are not duplicated in artifacts. Container/buffer `size()` is the source of truth.

## Realtime Memory Strategy

### Containers
- Prefer fixed-capacity buffers or pre-reserved vectors with enforced max capacity.
- Use `std::span` for function boundaries.
- If polymorphic allocation is needed, only use `std::pmr` with a monotonic/static arena created off-thread.

### Capacity Policy
- Define max capacities per rasterizer type:
  - max intercepts
  - max curves
  - max waveform points
  - max deform regions
- Enforce with debug assertions and runtime guard returns.

### Copying Policy
- Copying is allowed when bounded and predictable.
- Avoid repeated full-buffer copies in per-block rendering unless required by state transitions.
- Use delta updates for incremental edits when feasible.

## Testing Strategy
Use characterization first, then redesign behind tests.

### Test Types
1. Golden-buffer tests:
   - oscillator cyclic
   - oscillator chaining continuity
   - envelope normal/looping/release
   - one-sample-per-cycle mode
   - tempo-sync and logarithmic post mapping
2. Determinism tests:
   - identical output across repeated runs given same seed/state
3. Capacity/guard tests:
   - verify no growth/alloc in render paths
4. Property tests:
   - monotonic x ordering where required
   - sampleability bounds and zero/one index invariants

## Implementation Stages

### Stage 0: Contract Freeze
- Define v2 stage interface headers.
- Define all state/config structs and ownership boundaries.
- Add max-capacity constants and validation helpers.

Exit criteria:
- Headers compile cleanly.
- No old/new mixed signatures for the same concept.

### Stage 1: Workspace + No-Alloc Guardrails
- Implement `RasterizerWorkspace` and `prepare`.
- Wire debug instrumentation for allocation detection in render paths.
- Add unit tests to assert no capacity changes during render.

Exit criteria:
- Render paths run without allocation in debug instrumentation.

### Stage 2: Interpolator + Positioner Port
- Port trilinear/bilinear/simple interpolation into v2 stage contracts.
- Port padding/cyclic/chaining/env positioning into v2.
- Validate intercept equivalence against characterization fixtures.

Exit criteria:
- Intercept outputs match golden fixtures within tolerance.

### Stage 3: Curve/Wave Build Port
- Port curve generation and waveform build logic.
- Port path deform integration in wave build.
- Validate waveform tables and sampling slopes against fixtures.

Exit criteria:
- Curve and wave outputs match fixtures.

### Stage 4: Env State Machine + Sampler Port
- Implement `EnvStateMachine`.
- Port loop/release transitions, release scaling, and decoupled sampling.
- Support per-voice state arrays with bounded capacity.

Exit criteria:
- Envelope mode transitions and buffer outputs match fixtures.

### Stage 5: Rasterizer Graph Assembly
- Assemble concrete rasterizers:
  - `GraphicRasterizerV2`
  - `VoiceRasterizerV2`
  - `EnvRasterizerV2`
  - `FxRasterizerV2`
- Remove legacy façade usage from call sites.

Exit criteria:
- New rasterizers drive all call sites.

### Stage 6: Cleanup
- Delete dead legacy types and compatibility glue.
- Finalize docs and performance notes.

Exit criteria:
- Legacy classes removed from active build.

## TODO

### Progress Snapshot (2026-02-15)
- Completed scaffold under `lib/src/Curve/V2/`:
  - `Core/V2RenderTypes.h`
  - `Core/V2RasterizerWorkspace.h/.cpp`
  - `Stages/V2StageInterfaces.h`
  - `State/V2EnvStateMachine.h/.cpp`
- Added concrete stage implementations:
  - `Stages/V2InterpolatorStages.h/.cpp`
  - `Stages/V2PositionerStages.h/.cpp`
  - `Stages/V2CurveBuilderStages.h/.cpp`
  - `Stages/V2WaveBuilderStages.h/.cpp`
  - `Stages/V2SamplerStages.h/.cpp`
- Added graph orchestration:
  - `Runtime/V2RasterizerGraph.h/.cpp`
- Added initial concrete v2 graphic rasterizer:
  - `Rasterizers/V2GraphicRasterizer.h/.cpp`
- Added initial concrete v2 env rasterizer:
  - `Rasterizers/V2EnvRasterizer.h/.cpp`
- Added initial concrete v2 voice rasterizer:
  - `Rasterizers/V2VoiceRasterizer.h/.cpp`
- Added initial concrete v2 fx rasterizer:
  - `Rasterizers/V2FxRasterizer.h/.cpp`
- Added tests:
  - `lib/tests/TestV2EnvStateMachine.cpp`
  - `lib/tests/TestV2RasterizerPipeline.cpp`
  - `lib/tests/TestV2GraphicRasterizer.cpp`
  - `lib/tests/TestV2EnvRasterizer.cpp`
  - `cycle/tests/TestV2VoiceRasterizer.cpp`
  - `lib/tests/TestV2FxRasterizer.cpp`
  - `lib/tests/TestV2MeshInterpolation.cpp`
- Updated render-path guardrails and waveform validation:
  - `V2RasterizerGraph` now rejects intercept/curve over-capacity outputs.
  - `V2WaveBuilderStages` slope derivation corrected (`dx = x[i+1] - x[i]`).
  - Added strict oracle-based sample-by-sample waveform tests (saw/square + generic oracle case).
  - Added repeated-render capacity-stability and overflow rejection tests.
  - Added deterministic output characterization test for fixed pipeline inputs.
  - Added envelope loop/release sampling characterization test.
  - Added voice block-continuity and phase-wrap characterization tests.
  - Added chaining-style voice interpolation continuity test with pole/time modulation and softened curve values.
  - Added FX precondition/determinism/mode characterization tests.
  - Added mesh->intercept interpolation characterization tests for determinism, bounds, and wrap-phases behavior.
  - Introduced shared staged artifact pipeline (`V2RasterizerPipeline`) and migrated V2 rasterizers to `renderIntercepts` + `renderWaveform` + `renderArtifacts`.
  - Removed V2 wrapper extraction methods in favor of staged artifact rendering at call sites/tests.
  - Removed redundant artifact count fields (`interceptCount`, `curveCount`, `wavePointCount`, `colorPointCount`); artifacts now use container/buffer `size()` directly.
  - Added a graphic call-site adapter in `cycle/src/Curve/GraphicRasterizer.cpp` to route `calcCrossPoints()` through `V2GraphicRasterizer`, with legacy fallback when V2 render cannot produce output.
  - Added an FX call-site adapter in `lib/src/Curve/FXRasterizer.cpp` to route `calcCrossPoints()` through `V2FxRasterizer`, with legacy fallback when V2 render cannot produce output.
  - Added `V2GraphicRequest` / `V2GraphicResult` types in `V2RenderTypes.h`.
- Build status:
  - `AmaranthLib` and `AmaranthLib_tests` build successfully.
  - `Cycle_tests` build successfully.
  - `ctest -R V2EnvStateMachine` passes.
  - `ctest -R V2 --output-on-failure` passes (39/39 tests).

### Latest Progress (2026-03-01)
- V2 voice chaining advancement semantics were fixed and validated with explicit non-zero `minLineLength` coverage:
  - `V2VoiceRasterizer` now applies chaining advancement via direct morph-time update in intercept rendering.
  - Added `[curve][v2][voice][chaining][advancement]` test and validated passing at `minLineLength = 0.01f`.
  - Added chaining oracle sequencing test coverage to guard legacy-style carry behavior across calls.
- Core render-domain types were moved to V2 runtime types:
  - `ScalingType` canonicalized as `V2ScalingType` in `V2RenderTypes.h`.
  - `DeformRegion` canonicalized as `V2DeformRegion` in `V2RenderTypes.h`.
  - `MeshRasterizer` now aliases these V2 types for compatibility while V2 stage/runtime code consumes V2-native types directly.

### Immediate
- [x] Create `Curve/V2/` module layout (`Stages`, `State`, `Runtime`).
- [x] Define `GraphicRequest` and `GraphicResult`.
- [x] Define `PrepareSpec`, `RenderRequest`, and `RenderResult`.
- [x] Port legacy deformer-aware intercept positioning into V2 intercept pipeline:
  - [x] Add explicit deformer context/state inputs to v2 intercept-stage contracts (`V2PositionerContext::PointPathContext` with deformer handle, noise seed, offset seeds, enabled flag).
  - [x] Apply deformer adjustments to intercept fields (`adjustedX`, `y`, `shp`) for all five dimensions (Red, Blue, Amp, Phase, Curve) in `V2PointPathPositionerStage`.
  - [x] Preserve cyclic wrap/resort parity behavior for phase deformer repositioning — phase wrapping implemented via `wrapAdjustedX` and sequencing covered by chaining parity tests.
  - [x] Add `noOffsetAtEnds` suppression for Amp/Phase/Curve deformers when time progress is 0 or 1 (legacy `applyDeformers` line 978).
  - [x] Validate time-progress calculation parity path: V2 supports legacy-compatible reduct-based progress (`useLegacyTimeProgress`) for parity-sensitive paths.
- [~] Port legacy waveform-piece path/deformer integration into V2 wave pipeline:
  - [x] Add a composable wave-builder chain with explicit non-path baseline stage + path-aware stage (avoid duplicating non-path curve-blend logic).
  - [x] Port component/time deformer branch from legacy `calcWaveform` (`getCompDfrm`) including per-piece resolution policy and deterministic noise-context use.
  - [x] Reintroduce decoupled path deformation behavior (`decoupleComponentDfrms`) via explicit deform-region artifacts (`V2DeformRegion`, `V2DecoupledDeformContext`) and sampler-time application (`sampleAtPhaseDecoupled`). Remaining: end-to-end parity validation.
  - [ ] Ensure path-aware wave behavior remains allocation-free and bounded by prepared capacities (`maxDeformRegions`, `maxWavePoints`).
- [ ] Define fixed-capacity policies for intercepts/curves/waves/deform regions.
- [ ] Add allocation guard helper for render-path tests.
- [ ] Create initial golden fixtures from current behavior for:
  - [x] envelope normal
  - [x] envelope loop
  - [x] envelope release
  - [ ] oscillator chaining

### Short Term
- [x] Implement v2 interpolator stages.
- [x] Implement v2 positioner stages.
- [x] Add `V2RasterizerGraph` orchestration over `V2RasterizerWorkspace`.
- [x] Implement v2 curve builder stage.
- [x] Implement v2 wave builder stage.
- [x] Implement `EnvStateMachine`.
- [x] Implement v2 sampler stage.

### Architecture Cleanup
- [x] Extract `ScalingType` from `MeshRasterizer` into a standalone V2 enum (e.g. in `V2RenderTypes.h`) to eliminate the `#include "MeshRasterizer.h"` dependency in `V2StageInterfaces.h`.
- [x] Consolidate per-rasterizer artifact wiring — `V2RasterizerGraph` provides composable entry points (`buildInterceptArtifacts`, `buildCurveAndWaveArtifacts`, `buildArtifacts`, `render`), `V2RasterizerControls.h` provides factory functions for context construction, and `V2CommonControlSnapshot` hierarchy gives each rasterizer a typed control surface. Concrete rasterizers now only contain domain-specific logic (env loop/sustain, voice chaining, fx padding policy).

### Integration
- [x] Implement `GraphicRasterizerV2`.
- [x] Implement `VoiceRasterizerV2`.
- [x] Implement `EnvRasterizerV2`.
- [x] Implement `FxRasterizerV2`.
- [ ] Swap all call sites to v2 API.
  - [x] Graphic path defaults to V2 adapter with legacy fallback on render failure.
  - [x] FX path defaults to V2 adapter with legacy fallback on render failure.
- [ ] Remove legacy rasterizer codepaths from build.

### Validation
- [ ] Run full `ctest` suite for tests preset.
- [x] Resolve strict sample-by-sample waveform characterization failures for saw/square in `TestV2RasterizerPipeline.cpp`.
- [ ] Add legacy-vs-v2 parity tests for deformer-aware intercept positioning:
  - [ ] red/blue deformers affecting `adjustedX` ordering and clamp/wrap behavior.
  - [ ] phase deformer wrap semantics in cyclic mode (including reorder points).
  - [ ] amp and curve deformer effects on intercept `y` and `shp`.
  - [ ] `noOffsetAtEnds` suppression at progress boundaries.
  - [ ] time-progress source equivalence (`reduct`-based vs `getPortionAlong`).
  - [ ] voice chaining + deformer combinations across block boundaries.
- [ ] Add legacy-vs-v2 parity tests for waveform-piece path/deformer behavior:
  - [ ] component/time deformer (`getCompDfrm`) wave-shape parity in non-decoupled mode.
  - [ ] deformer-region generation parity and sampler-time application parity in decoupled mode.
  - [ ] zero/one index and slope-table invariants under path-aware wave generation.
  - [ ] deterministic output parity for fixed morph + noise seeds across repeated renders.
- [ ] Add benchmark for audio-block render cost per rasterizer.
- [ ] Verify deterministic output across runs and platforms.
- [ ] Document realtime invariants in code comments and contributor docs.

## Consequences

### Positive
- Clear stage contracts and predictable ownership.
- Realtime-safe processing model.
- Easier independent testing per stage.
- Cleaner future extension for new rasterizer types.

### Tradeoffs
- Upfront effort to define capacities and state boundaries.
- Temporary dual-system complexity while v2 is being assembled.
- Requires disciplined tests to avoid subtle behavior drift.
