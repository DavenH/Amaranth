# Cycle V2 Impulse Modeller Visual Parity

Status: Implemented (2026-08-30)

## Objective

Restore the diagnostic visual layers of Cycle 1's impulse modeller inside the
Cycle V2 IR curve panel: the high-pass-filtered impulse backdrop and the
frequency-magnitude colour field. Preserve Cycle V2's editor chrome and
property controls while making the OpenGL canvas itself as close as practical
to the mature Cycle 1 presentation.

## Authoritative Implementation

- `cycle/src/Audio/Effects/IrModeller.cpp::filterImpulse` owns the mature
  relationship between the raw impulse, high-pass levels, filtered impulse,
  and FFT magnitudes.
- `cycle/src/UI/Effects/IrModellerUI.cpp::preDraw` owns the mature layer order
  and rendering language: padded dark attack region, burntalum magnitude
  gradient, translucent pink filtered impulse, then editable curve geometry.
- `CycleDsp::rasterizeIrImpulse`, `buildIrPrefilterLevels`, and
  `applyIrFrequencyPrefilter` are the shared production DSP operations.
- `FlatCurvePanelBase`, `ImpulseResponseCurvePanel`, `CommonGfx`, and
  `CommonGL` remain authoritative for panel transforms, zoom, grids, curves,
  and OpenGL drawing.

The implementation extracts one non-realtime IR response preparation used by
both `IrSignalProcessor::buildConfiguration` and the visual panel. It reuses
the mature curve rasterization and filter operations unchanged. The panel
receives immutable filtered samples and normalized magnitudes; it does not
perform FFT analysis during painting or rendering.

## Visual Contract

The OpenGL panel draws, from back to front:

1. the existing canvas-family panel ground and zoom-aware grid;
2. the dark pre-impulse region ending at domain x `irDomainPadding`;
3. the Cycle 1 burntalum spectral gradient from the padded impulse origin to
   domain x `1`, with alpha reduced to the established 40-percent level;
4. the filtered bipolar impulse as a 1.5-pixel translucent pale-pink trace;
5. the editable IR curve, vertices, selection, and interaction overlays; and
6. the existing zoom-aware sample ruler outside the OpenGL panel.

The spectrum uses the filtered FFT magnitudes, Cycle 1's magnitude log mapping
and normalization, and Cycle 1's frequency-row tension. High Pass must visibly
remove low-frequency colour and alter the filtered impulse. Size changes the
analysis resolution without changing its normalized domain support. Direct
embedded audio and modelled curves use the same presentation pipeline as their
production IR configuration.

## Ownership And Lifecycle

- IR response preparation runs only when model, relevant parameters, or bound
  audio-resource content changes. It is forbidden in JUCE `paint`, OpenGL
  `preDraw`, or per-frame host rendering.
- The prepared data is immutable across the controller/panel boundary. The
  OpenGL panel may cache resized vertex/color buffers derived from it.
- The editor's JUCE paint path remains limited to shell chrome, title, ruler,
  and property controls. It must not draw the impulse or spectrum and must not
  contain a fallback raster image for either layer.
- Direct-resource samples cross through `NodePreviewResources`, which already
  owns the durable graph read boundary. The panel never retains a pointer into
  mutable graph storage.
- Audio-thread behavior, convolution, post gain, graph mutation, undo, and
  resource serialization remain unchanged.

## Implementation Slices

1. Characterize the shared IR preparation with tests proving high-pass changes
   both the filtered impulse and low-frequency magnitude while leaving the raw
   source unchanged.
2. Extract the preparation from `IrSignalProcessor::buildConfiguration` and
   reuse it for audio configuration without changing the resulting kernel.
3. Forward direct-resource content and prepared immutable visual analysis
   through the existing resource, widget, controller, and IR-panel contracts.
4. Port Cycle 1's burntalum gradient and filtered-impulse draw passes to
   `ImpulseResponseCurvePanel::preDraw` using only `CommonGfx`/`CommonGL`.
5. Add automation evidence for prepared point counts and OpenGL pass activity,
   then compare default, high-pass endpoint, direct-resource, and zoomed
   production screenshots.

## Negative Boundaries

- Do not use `juce::Graphics` to paint the spectrum, filtered impulse, or a
  cached image of either layer.
- Do not copy or approximate the curve sampler, FFT, filter, or high-pass
  transfer in editor or panel code.
- Do not calculate FFTs, rebuild IR kernels, allocate analysis vectors, or load
  gradient assets in `paint`, `preDraw`, or the per-frame OpenGL render call.
- Do not read graph resources from the panel, retain resource pointers, or add
  IR-specific branches to generic canvas/editor hosting.
- Do not change serialized parameters, impulse length stops, post-gain
  behavior, curve editing, zoom behavior, or ruler geometry.

## Completion Criteria

- The OpenGL IR canvas contains the Cycle 1 filtered-impulse and spectral
  backdrop layers in the authoritative order and palette.
- High Pass produces a truthful, observable change in both layers using shared
  production preparation.
- Modelled curves and direct embedded audio both produce the backdrop.
- JUCE painting contains no implementation or fallback for either layer.
- Preparation is cached outside frame rendering and no audio-thread work or
  allocation changes.
- Focused DSP, panel, editor, resource, and automation tests pass. Production
  screenshots cover default and high-pass states; direct-resource handoff is
  guarded by an exact panel-analysis assertion, and the existing zoom fixture
  guards the shared transform.
- Standalone Debug, the full Cycle V2 suite, `git diff --check`, hot-loop review,
  style review, and production-diff review complete before commit.

## Implementation Evidence

- `ImpulseResponseAnalysis` is the single non-realtime preparation used by
  audio configuration and the visual controller. It owns the mature IR
  rasterization, prefilter, FFT magnitude mapping, and 512-row reduction.
- The controller caches analysis by size, high pass, model revision, and copied
  resource revision. The OpenGL render pass only transforms prepared buffers;
  its line splice storage and colour rows are allocated when analysis changes.
- `ImpulseResponseCurvePanel::preDraw` uses `CommonGfx` for the burntalum
  vertical gradient and filtered pink line strip. No JUCE paint implementation
  or fallback was added.
- Focused tests cover filtered audio/visual parity, high-pass magnitude
  removal, source immutability, panel preparation, and direct-resource handoff.
- Production OpenGL crops were inspected at
  `/private/tmp/cycle-v2-ir-parity-default-os.png` and
  `/private/tmp/cycle-v2-ir-parity-high-pass-os.png`. The existing zoom-ruler
  fixture continues to cover the shared zoom transform.
- The full Cycle V2 run completed with 10,596 of 10,597 assertions passing.
  Its sole failure is the pre-existing edge-help assertion at
  `TestNodeCanvasHitRouter.cpp:66`, unrelated to IR rendering.
