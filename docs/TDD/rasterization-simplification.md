# Rasterization Simplification TDD

## Overview

The first rasterization migration removed inheritance from the production
`MeshRasterizer` subclasses and captured a large amount of behavior in tests,
policies, pipelines, facades, and adapters. That made behavior safer to change,
but it did not reach the intended design. The current branch still has too many
domain-specific rasterizer types, composer branches, pipeline classes, and
compatibility interfaces.

This document replaces the incremental migration plan. The new goal is a
smaller rasterization system with one concrete composed rasterizer shape and
domain-specific configuration at the edges. The work should delete transitional
types as it proceeds. A phase is not complete if it only adds another wrapper.

## Current State

Production rasterizers no longer derive from or own `MeshRasterizer`; the old
implementation now exists only as the test fixture `LegacyMeshRasterizer`.
The replacement is still domain-shaped:

- `FXRasterizer` owns point-list rendering directly through `RenderResult` and
  `CurveWaveformBuilder`.
- mesh waveform paths now share `CurveWaveformBuilder` for curve/waveform
  construction, and mesh-wide slicing is owned by `TrilinearMeshSlicer`.
- voice paths no longer have a separate voice slice pipeline, but
  `VoiceMeshRasterizer` still has a rasterizer-shaped owner surface.
- envelope paths still combine rasterization, playback state, marker handling,
  loop/release behavior, and audio rendering.
- the remaining public surface is concentrated in `Rasterizer`, `SamplerView`,
  `SnapshotView`, `BaseRasterizer`, and `TrilinearMeshRasterizer`.

The result is safer than the original inheritance hierarchy, but not simpler.
The branch is only worth keeping if the next work collapses these layers into
less code and a smaller public surface.

## Target Design

The concrete rasterization type used by panels and DSP should have a small
runtime API. It should own rendering and results, but it should not become the
sampling interface itself:

```cpp
class Rasterizer {
public:
    const RenderResult& render(const RenderRequest& request);
    void clear();

    const RenderResult& result() const;
    SnapshotView snapshot() const;
    SamplerView sampler() const;
};
```

Configuration belongs in construction, not in the runtime interface:

```cpp
auto rasterizer = RasterizerComposer::build()
    .withSource(MeshCubeSource(mesh))
    .withSlicer(TrilinearMeshSlicer())
    .withMorphProvider(GraphicMorphPositionPolicy(...))
    .withGuides(ComponentGuidePolicy(...))
    .withPadding(CyclicPaddingPolicy(...))
    .withWaveform(WaveformBakePolicy(...))
    .withSnapshot(RasterizerDataSnapshot())
    .build();
```

FX, point-list, mesh, graphic, spectral, and voice rasterization should be
different configurations of the same engine where possible, not separate
families of composer, composed rasterizer, adapter, and pipeline classes.

The sampler view should be the narrow surface consumed by audio, analysis, and
visual-DSP code:

```cpp
class SamplerView {
public:
    bool isSampleable() const;
    bool isSampleableAt(float x) const;

    float sampleAt(double phase) const;
    float sampleAt(double phase, int& currentIndex) const;
    void sampleAtIntervals(Buffer<float> intervals, Buffer<float> dest) const;

    template<typename T>
    T sampleWithInterval(Buffer<float> dest, T delta, T phase) const;

    float samplePerfectly(double delta, Buffer<float> dest, double phase) const;
};
```

`renderToBuffer` is not part of `Rasterizer` or `SamplerView`. Existing call
sites use it for envelope playback because it advances loop/release/sustain
state while filling a buffer. That belongs on an envelope playback/render
service that consumes a sampler and envelope state.

`SnapshotView` is the panel-facing projection snapshot. It replaces the current
mixed `RasterizerData` access pattern for drawing, hit-testing, and UI
inspection:

```cpp
class SnapshotView {
public:
    Span<const Intercept> intercepts() const;
    Span<const Curve> curves() const;
    Span<const ColorPoint> colorPoints() const;

    CriticalSection& lock() const;
};
```

The distinction is intent rather than storage duplication:

- `SamplerView` is behavior over the generated waveform buffers owned by
  `RenderResult`.
- `SnapshotView` is the UI projection: waveform trace plus intercept markers,
  curve handles, and hidden-dimension `ColorPoint` depth/color overlays.
- `RenderResult` owns the data; views only borrow it.

`RenderResult` is the single owned output of a rasterization pass. It should
replace the current mixture of pipeline `Output` structs, `RasterizerStorage`,
`RasterizerRuntime`, and output-publication policies:

```cpp
struct RenderResult {
    std::vector<Intercept> intercepts;
    std::vector<Intercept> frontPadding;
    std::vector<Intercept> backPadding;
    std::vector<Curve> curves;
    std::vector<GuideCurveRegion> guideCurveRegions;
    std::vector<ColorPoint> colorPoints;

    ScopedAlloc<float> waveformMemory;
    WaveformBuffers waveform;

    int paddingSize {};
    bool sampleable {};
    bool needsResorting {};

    void clear();

    SamplerView sampler() const;
    SnapshotView snapshot() const;
};
```

The generic `Rasterizer` owns the latest `RenderResult` and returns a reference
from `render(...)`:

```cpp
class Rasterizer {
public:
    const RenderResult& render(const RenderRequest& request);
    const RenderResult& result() const;
};
```

This matters because callers should not need publication copies like
`waveX = output.waveX`, nor should pipelines render into external mutable
runtime bundles. The normal flow should be:

1. stage code fills a `RenderResult`,
2. the rasterizer stores it as the latest result,
3. panels/audio consume `snapshot()` or `sampler()`,
4. legacy `RasterizerData` is populated only at compatibility boundaries until
   those panels move to `SnapshotView`.

Long template names are acceptable internally, but callers should not have to
name them. Local code can use `auto`; owner classes can use a small type-erased
handle only if a concrete member type would leak too much implementation detail.

## Design Rules

- Prefer delete-first vertical replacements over preparatory infrastructure
  phases.
- Do not add infrastructure unless the same phase deletes or collapses the old
  path it replaces.
- Do not add new compatibility adapters.
- Do not add new domain-specific composed rasterizer classes.
- Do not add a new pipeline if an existing generic stage can express the work.
- Every implementation phase should remove or collapse at least one old type,
  source file, interface, or domain branch.
- Domain-specific names may exist as small factory helpers, policies, or owner
  classes, but not as separate rasterizer architectures.
- Prefer one generic render result over copied member bundles.
- Prefer views/accessors over broad inherited interfaces.
- Keep validation strong enough that wholesale replacement is still safe:
  unit tests, preset smoke, cropped UI captures, PSNR checks, and targeted
  automation for known risky paths.

## Implementation Strategy

This plan intentionally takes more implementation risk than the previous
migration. The repository history and regression coverage are the safety net.
The main risk to avoid is not breakage; it is growing a second abstraction tree
next to the first one.

Each implementation phase should:

1. delete or disconnect the old named path first,
2. let compile errors expose the real replacement surface,
3. add only the generic result/view/stage code needed by that replacement,
4. validate behavior,
5. remove dead tests or compatibility code for the path that no longer exists.

There should be no infrastructure-only phase. `RenderResult`, `SamplerView`,
`SnapshotView`, and the generic `Rasterizer` are introduced as part of deleting
real old code, not as speculative scaffolding. If a proposed view has no real
caller, delete it rather than keeping a target-shaped placeholder.

## Target File Shape

The final layout should be closer to:

```text
lib/src/Curve/Rasterization/
    Rasterizer.h
    RasterizerComposer.h
    RenderRequest.h
    RenderResult.h

    Sources/
        MeshCubeSource.h
        PointListSource.h
        VertexListSource.h

    Stages/
        MeshSliceStage.h
        PointListStage.h
        InterceptTransformStage.h
        CurveBuildStage.h
        WaveformBuildStage.h
        SnapshotStage.h

    Policies/
        CorePolicies.h
        PaddingPolicies.h
        MeshPolicies.h
        EnvelopePolicies.h

    Views/
        SnapshotView.h
        SamplerView.h
```

Cycle-specific policies should stay under `cycle/src/Curve/Rasterization/`
only when they depend on Cycle application state. Cross-product envelope and FX
policies can remain in `lib/`.

## Validation Baseline

Each nontrivial phase must build and pass tests:

```sh
cmake --preset tests
cmake --build --preset tests
ctest --test-dir build/tests --output-on-failure
cmake --build --preset standalone-debug --target Cycle
```

For visual paths, use focused Cycle automation from
`docs/cycle-agent-runbook.md`. Prefer app-side state assertions for ordinary
logic, and OS-cropped panel screenshots for OpenGL-backed rasterizer panels.

The preferred rasterizer visual baseline is the bongo panel set because it
exercises waveform, guide curve, waveshaper FX, spectrum magnitude, and
spectrum phase paths in one launch. Capture candidates into `/tmp` so checked-in
baselines are not overwritten:

```sh
CYCLE_PANEL_BASELINE_BUILD_FIRST=1 \
    scripts/capture_cycle_panel_baselines.sh /tmp/cycle-panel-candidate
```

Compare each candidate PNG against `docs/media/images/cycle-panel-baselines`
with PSNR:

```sh
for image in docs/media/images/cycle-panel-baselines/*.png; do
    name="$(basename "$image")"
    scripts/compare_png_psnr.py \
        "docs/media/images/cycle-panel-baselines/$name" \
        "/tmp/cycle-panel-candidate/$name" \
        --min-psnr 48
done
```

`inf` PSNR is expected when the crop is pixel-identical. Finite values can still
be acceptable when mouse hover overlays, platform rendering, or expected preset
changes are understood, but they must be inspected before accepting a phase.

For spectrum-specific work, also run the targeted mode-switch fixture:

```sh
scripts/run_cycle_agent.sh \
    scripts/fixtures/cycle-agent-spectrum-mode-switch.json \
    /tmp/cycle-spectrum-mode-switch.json
```

For preset-loading regressions, run the AfricanHorn-to-Icycle fixture:

```sh
scripts/run_cycle_agent.sh \
    scripts/fixtures/cycle-agent-africanhorn-icycle-preset-switch.json \
    /tmp/cycle-africanhorn-icycle.json
```

When voice rasterization changes are nontrivial, run the MIDI-note automation
fixture:

```sh
scripts/run_cycle_agent.sh \
    scripts/fixtures/cycle-agent-midi-note.json \
    /tmp/cycle-midi-note.json
```

When a change affects render-time audio or envelope playback, prefer an audio
capture fixture in addition to visual inspection:

```sh
scripts/run_cycle_agent.sh \
    scripts/fixtures/cycle-agent-audio-capture.json \
    /tmp/cycle-audio-capture.json
```

Known existing crash/leak noise should not be mixed into acceptance criteria.
If a validation failure is unrelated, document it in the phase notes before
continuing.

## Current Census

At the start of this plan, `lib/src/Curve/Rasterization` and
`cycle/src/Curve/Rasterization` contain 78 files and roughly 5,435 lines.

Call-site review shows these separate consumer roles:

- panel drawing consumes `RasterizerData`/snapshot output,
- current panel snapshot output includes `waveX`, `waveY`, `zeroIndex`,
  `oneIndex`, `intercepts`, `curves`, and `colorPoints`,
- visual-DSP and audio paths consume a sampler with point and buffer sampling,
- `Interactor`, `Multisample`, and `PitchedSample` currently receive separate
  mesh binding, update, sampler, and snapshot interfaces,
- `IrModeller` and `Waveshaper` want a waveform/sampler provider, not a full
  rasterizer,
- envelope `renderToBuffer` call sites need playback-state advancement, not
  generic waveform sampling.

Initial transitional deletion targets:

- facade wrappers that only forward to policies,
- interface headers whose callers are not genuinely disjoint.

## Phase 0: Deletion Budget And First Cut Boundary

Goal: make the simplification measurable and set the first delete-first
boundary.

Tasks:

- count files and lines under `lib/src/Curve/Rasterization` and
  `cycle/src/Curve/Rasterization`,
- list all current rasterizer/composer/pipeline/facade/adapter/interface types,
- classify each type as target, transitional, or legacy,
- define the deletion target for the first hard cut,
- preserve the current bongo panel baseline and confirm the verification
  commands still work before replacement begins.

Acceptance:

- the census is added to this document or a short linked note,
- Phase 1 has explicit files/classes it intends to delete,
- no code changes are made in this phase.

## Phase 1: Hard-Cut FX To Generic Point Rasterization

Goal: remove the clearest domain-shaped architecture first and introduce only
the generic pieces needed by the replacement.

Tasks:

- delete `FxRasterizerAdapter`,
- delete `FxRasterizationPipeline`,
- delete `ComposedFxRasterizer` and `FxComposer`,
- introduce `RenderResult` as the owned replacement for the deleted FX pipeline
  output,
- introduce `SamplerView` only far enough to support the FX, `IrModeller`, and
  `Waveshaper` call sites,
- make `FXRasterizer` directly own the generic point-list rasterizer path,
- keep `FXRasterizer` as the public owner for current UI/audio callers, but
  shrink it to source binding, request creation, render, sampling, and snapshot
  access,
- keep `FXRasterizer::setMesh` only as a legacy source conversion if current
  callers still require it,
- remove or rewrite tests that only prove the old FX composer/adapter plumbing.

Acceptance:

- FX behavior matches existing unit tests,
- `FXRasterizer` no longer constructs any FX-specific composer, adapter, or
  pipeline,
- `RenderResult` replaces FX output publication copies rather than sitting
  beside them,
- `IrModeller` and `Waveshaper` consume the new sampler shape or a narrow
  compatibility boundary over it,
- the phase removes more rasterization code than it adds,
- FX panel, waveshaper, and IR modeller validation still pass.

## Phase 2: Collapse Point-List And Composer Branches

Goal: use the FX success to remove the remaining non-mesh composer branches and
make point-list rasterization another configuration of the same shape.

Tasks:

- delete `ComposedPointListRasterizer` and `PointListComposer`,
- replace point-list rendering outputs with `RenderResult`,
- keep generic intercept-to-waveform rendering in `CurveWaveformBuilder`,
- replace `RasterizerComposer::fx()` and `pointList()` with one composer entry
  point or thin factory that returns the same concrete rasterizer shape,
- keep domain-specific helper functions only as thin presets if they reduce
  call-site noise,
- remove `PointListOutputPolicy` if `RenderResult` removes the need for runtime
  publication copies.

Acceptance:

- existing point-list and FX characterization tests use the same result/view
  family,
- no call site names a domain-specific composed rasterizer type,
- unsupported combinations fail with a clear assertion or compile error,
- line count under `RasterizerComposer.h` decreases substantially.

## Phase 3: Hard-Cut Mesh Composer To The Generic Engine

Goal: make mesh slicing and mesh waveform rendering use the same composed
rasterizer shape without keeping `ComposedMeshRasterizer` as a separate public
concept.

Tasks:

- delete `ComposedMeshRasterizer` and `MeshComposer` as public concepts,
- replace mesh slice output publication with `RenderResult`,
- keep `TrilinearMeshSlicer`, guide policies, and depth/color projection as
  stages or policies feeding the generic result,
- keep `ComposedMeshWaveformRasterizer` only if it is renamed/scoped as a
  temporary private implementation detail, otherwise delete it in this phase,
- update callers to consume views rather than copying `waveX`, `waveY`,
  `diffX`, `slope`, `area`, `zeroIndex`, and `oneIndex` member-by-member.

Acceptance:

- mesh characterization tests compare `RenderResult` values or views directly,
- the repeated waveform member-copy pattern is gone,
- no public call site names `ComposedMeshRasterizer`,
- existing panel snapshot publication remains visually equivalent,
- bongo panel PSNR validation passes.

## Phase 4: Collapse Interface Soup Into Views

Goal: reduce the public call surface inherited from the old rasterizer shape.

Tasks:

- replace broad internal use of `RasterizerSampler`,
  `RasterizerSnapshotProvider`, `RasterizerVertexDomain`, and
  `WaveformProvider` with `SamplerView` and `SnapshotView`,
- move `sampleAt`, `sampleAtIntervals`, `sampleWithInterval`, and
  `samplePerfectly` behind `SamplerView` rather than exposing them directly on
  the generic rasterizer,
- keep envelope `renderToBuffer` out of `SamplerView`; move it toward an
  envelope playback/render service because it mutates envelope state,
- keep legacy interfaces only where external ownership or JUCE update wiring
  still requires them,
- remove any interface that has no genuinely disjoint caller set.

Acceptance:

- callers that only sample do not also require snapshot/mesh/update methods,
- callers that only draw snapshots do not require waveform sampling methods,
- `Interactor`, `Multisample`, and `PitchedSample` depend on narrower role
  objects or views,
- at least one obsolete interface header is deleted,
- no new adapter is introduced to preserve the old interface bundle.

## Phase 5: Replace Graphic And E3 Rasterizer Classes With Owners

Goal: remove ex-subclass behavior from the graphic mesh paths.

Tasks:

- make graphic/spectrum/time rasterization a generic mesh-rasterizer
  configuration with Cycle-specific morph and axis policies,
- make E3 rasterization another configuration of the same engine,
- keep repository object names stable only where `SingletonRepo` wiring
  requires them,
- convert `GraphicRasterizer` and `E3Rasterizer` from rasterizer-shaped classes
  into small owners around a generic rasterizer plus Cycle state,
- delete any graphic/E3 facade or pipeline class whose only job was to emulate
  old subclass hooks.

Acceptance:

- Spectrum2D/Spectrum3D, Waveform2D/Waveform3D, and Envelope3D visual captures
  match baselines,
- switching magnitude/phase panels still refreshes the correct curve,
- no graphic path owns `ComposedMeshWaveformRasterizer` directly.

## Phase 6: Replace Voice Mesh Rasterizer With A Voice Owner Plus Generic Engine

Goal: keep voice-specific chaining while deleting the rasterizer-shaped class.

Tasks:

- move voice chaining state into a small voice owner/service,
- express the slice, padding, waveform build, and sampling work through the
  generic rasterizer engine,
- keep voice slicing local to the voice owner while looking for a smaller
  voice-specific service boundary,
- keep voice policy names only for genuinely voice-specific chaining behavior,
- keep `renderToBuffer` out of the generic rasterizer surface.

Acceptance:

- MIDI-note automation passes,
- voice waveform and sampling unit tests pass,
- no voice class implements the broad old rasterizer interface.

## Phase 7: Split Envelope Playback From Envelope Rasterization

Goal: avoid forcing envelope state-machine behavior into the generic rasterizer.

Tasks:

- separate envelope playback/release/loop/sustain state from curve
  rasterization,
- use the generic rasterizer for envelope intercept/curve/waveform production,
- keep envelope render-time advancement as an envelope playback service,
- make `renderToBuffer` a method of that playback service or envelope owner,
  not the generic rasterizer,
- remove envelope facade/policy classes that only forward between old runtime
  bundles and new runtime bundles.

Acceptance:

- envelope panel interaction, playback-bar clamping, release simulation, and
  audio render tests pass,
- envelope rasterization does not require mesh-only dimensions that envelopes
  do not own,
- guide-curve-heavy presets still render correctly.

## Phase 8: Delete Or Rename Legacy `MeshRasterizer`

Goal: remove the god-object name from production code.

Tasks:

- move any remaining characterization-only behavior into tests or fixtures,
- delete `MeshRasterizer` if no production caller needs it,
- otherwise rename it to make its legacy/characterization role explicit,
- remove includes that pull `MeshRasterizer` into new code.

Acceptance:

- production code does not instantiate `MeshRasterizer`,
- no active rasterizer class derives from or mimics its full public surface,
- tests still preserve behavior coverage through generic engine fixtures.

Implemented state:

- production code has no `MeshRasterizer` include, owner, or base class,
- characterization coverage uses `lib/tests/Support/LegacyMeshRasterizer.*`,
- production mesh-waveform owners share `TrilinearMeshRasterizer` for mesh,
  morph-position, guide-curve, and request forwarding,
- production non-mesh-waveform owners share `BaseRasterizer` for snapshot
  storage and publication.
- production rasterizers expose stage-oriented `updateGeometry()` and
  `updateWaveform()` methods; `performUpdate(Update)` is now a compatibility
  bridge that maps to the waveform stage,
- the old `calcCrossPoints` name remains only in the test-only legacy
  characterization fixture.

## Phase 9: Final Aesthetic Cleanup

Goal: make the final tree navigable and smaller.

Tasks:

- group small policy headers into logical aggregate files where that improves
  discoverability,
- remove dead facades, adapters, compatibility output policies, and duplicate
  tests that only covered transitional plumbing,
- update ADR 002 to describe the final design rather than the old migration,
- update docs that still recommend old composer branches or compatibility
  layers.

Acceptance:

- rasterization code has a lower file count and lower line count than the
  current branch state,
- public rasterization API fits on a short page,
- the remaining files map directly to source, stage, policy, result, view, or
  owner roles,
- full tests and visual validation pass.

Implemented state:

- `TestMeshRasterizerFacade.cpp` was renamed to the behavior it actually
  tests: `TestCurveResolutionPolicy.cpp`,
- ADR 002 now describes the final production state rather than a retained
  production compatibility shell,
- policy documentation points readers to the shared runtime surfaces.

## Stop Conditions

Stop and reassess if:

- a phase needs a new adapter to preserve behavior,
- a phase adds generic infrastructure without deleting the old path it replaces,
- the generic engine becomes a new god object,
- code size grows for two consecutive phases,
- a domain path cannot be expressed without reintroducing a full separate
  rasterizer architecture.

In those cases, either narrow the target abstraction or abandon this branch
before more complexity accumulates.
