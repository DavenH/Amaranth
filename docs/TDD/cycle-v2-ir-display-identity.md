# Cycle V2 IR Display Identity

Status: Implemented (2026-09-05)

## Objective

Remove the extra attack inflection from the IR modeller's sampled trace when
High Pass is 0 Hz and Post Gain is unity. The displayed trace must recover
Cycle 1's visual identity with the editable curve while the convolution kernel
continues to use Cycle 1's oversampled audio preparation.

## Authoritative Implementations

- `cycle/src/Audio/Effects/IrModeller.cpp::rasterizeImpulse` is authoritative
  for Cycle 1's intentional split: audio uses
  `CycleDsp::rasterizeIrImpulse`, while the graphic impulse uses the same
  prepared sampler with `sampleWithInterval`.
- `CycleDsp::rasterizeIrImpulse`, `buildIrPrefilterLevels`, and
  `applyIrFrequencyPrefilter` remain authoritative for the audio sampling and
  shared filtering operations.
- `FlatCurvePreparation` remains the Cycle V2 boundary that constructs one
  mature `FXRasterizer` sampler from immutable node state.
- `ImpulseResponseAnalysis` owns non-realtime source and filtered display
  preparation. `ImpulseResponseCurvePanel` only transforms immutable prepared
  values into OpenGL geometry.

The former Cycle V2 contract described the visible and audible traces as the
same samples. Production evidence and the Cycle 1 source show that this is not
the mature behavior: the oversampling low-pass can ring around the deliberately
sharp default attack. This slice preserves one model, one sampler, one domain,
one parameter mapping, and one filter implementation, but restores distinct
audio and display sample views at that established boundary.

## State And Ownership

`ImpulseResponseSource` owns two arrays prepared together:

1. the audio impulse sampled through Cycle 1's 2x oversampling/downsampling
   path; and
2. the display impulse sampled directly at the final IR interval, as Cycle 1's
   graphic path does.

`ImpulseResponseAnalysis` retains the filtered audio impulse for convolution
and adds a filtered display impulse for the OpenGL trace and spectral field.
Both are filtered with the shared prefilter levels. Direct audio resources use
identical source arrays because no curve resampling is involved.

## Test-First Contract

1. A modelled source exposes both audio and display samples, and the display
   samples match the editable production waveform at the IR sample positions.
2. At 0 Hz, each filtered result is an identity of its corresponding source.
3. The audio configuration continues to consume the filtered audio result, not
   the display result.
4. The panel consumes the filtered display result and reports it separately in
   automation state.
5. The default model's displayed samples introduce no turning points absent
   from the editable waveform at the same positions.

## Negative Boundaries

- Do not change the audio kernel to make the picture smoother.
- Do not approximate, duplicate, or edit the mature sampler, oversampler,
  prefilter, FFT, or curve evaluator.
- Do not filter, resample, allocate, or inspect graph state in OpenGL drawing.
- Do not use JUCE painting or a masking overlay for the correction.
- Do not make High Pass or Post Gain rebuild the curve sampler.

## Completion Criteria

- The focused source, analysis, audio-configuration, controller, and panel
  tests pass, including topology and zero-cutoff identity assertions.
- A production 0 Hz/unity-gain screenshot shows the sampled trace overlaying
  the editable default curve without the reported second attack inflection.
- Raised High Pass remains live and Post Gain remains a cheap display scalar.
- Standalone Debug builds; `git diff --check`, hot-loop review, style review,
  and production-diff review pass before commit.

## Implementation Evidence

- `ImpulseResponseSource` now prepares Cycle 1's audio and graphic sample
  views together from one `FlatCurvePreparation` sampler. Direct resources
  share identical arrays; modelled curves retain the mature oversampled audio
  path and direct display path.
- `ImpulseResponseAnalysis` applies the same shared high-pass levels and FFT
  implementation to both views. `IrSignalProcessor` still consumes only the
  filtered audio impulse, while the OpenGL panel consumes only the filtered
  display impulse and its spectrum.
- The OpenGL trace now spans the exact final-sample domain endpoint instead of
  advancing by `1 / sampleCount`.
- Focused tests guard zero-cutoff identity for both views, audio configuration
  ownership, panel display ownership, editable-curve RMS agreement below
  `0.005`, and absence of additional significant turning points.
- The cached live-control fixture passes with one source preparation, one
  initial analysis, one raised-cutoff analysis, and no Post Gain analysis
  rebuild. The fixture expectations were updated for the cache optimization
  merged from master.
- Production evidence was inspected at
  `/private/tmp/cycle-v2-ir-display-identity.png`; at 0 Hz and 0 dB the pale
  display trace follows the editable curve without the former filter-ringing
  shape. The filtered display remains entirely in the OpenGL canvas.
- Standalone Debug builds successfully. The full Cycle V2 suite completed with
  11,079 of 11,080 assertions passing; the sole failure is the pre-existing
  edge-help assertion at `TestNodeCanvasHitRouter.cpp:66`. `git diff --check`,
  hot-loop review, style review, and production-diff review passed. The local
  environment does not provide `clang-tidy`.
