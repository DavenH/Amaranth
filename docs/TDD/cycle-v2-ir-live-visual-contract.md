# Cycle V2 IR Live Visual Contract

Status: Implemented (2026-08-31)

## Objective

Make the IR modeller's diagnostic backdrop a truthful, immediate explanation
of the sound produced by its controls. High Pass must update the filtered
impulse and spectrum during the gesture, Post Gain must scale the displayed
post-filter impulse, and unrelated controls must not force synchronous curve
rasterization or FFT work.

The same slice also improves low-frequency High Pass manipulation without
changing stored values, preset compatibility, or Cycle 1 DSP behavior.

## Authoritative Implementations

- `cycle/src/Audio/Effects/IrModeller.cpp` owns the mature separation between
  raw impulse rasterization, `prefilterChg`, and graphical filtering.
- `CycleDsp::rasterizeIrImpulse`, `buildIrPrefilterLevels`, and
  `applyIrFrequencyPrefilter` remain the only implementations of sampling and
  filtering used by Cycle V2 audio and visualization.
- `ImpulseResponseAnalysis` remains the non-realtime audio/visual preparation
  boundary. This work splits that boundary into a source stage and an analysis
  stage; it does not copy either domain algorithm.
- `CurveExpandedEditorComponent::publishCurrentState` owns the editor gesture
  order. Its existing control-state callback occurs before graph publication
  and is the synchronous visual-update boundary.

Cycle 1 and Cycle V2 both store High Pass in `[0, 1]` and use the same cubic
mapping to `[0, Nyquist]`. Cycle 2's spectral rows also use Cycle 1's log
mapping with tension `1000`. The reported breadth is therefore a physical
adjustment-resolution issue, not a different spectral or DSP mapping.

## State And Ownership

The controller owns two derived states:

1. a raw sampled impulse, keyed by Size, curve-model revision, and copied audio
   resource revision; and
2. filtered analysis, keyed by raw-source revision and High Pass.

Size, curve edits, and resource changes may rebuild both stages when the node
or resource is rebound. High Pass may rebuild only the second stage from the
cached raw source during its UI gesture. Post Gain changes only a scalar owned
by the IR panel and must not rebuild either stage.

Prepared source and analysis data cross into the panel as immutable values.
The OpenGL draw pass may apply the cached Post Gain scalar with existing vector
operations, but it must not allocate, rasterize a curve, run an FFT, or rebuild
filter coefficients.

## Interaction And Display Contract

- Every High Pass value change updates the filtered impulse and spectral field
  before the corresponding transient node publication completes.
- Repeated High Pass changes in one gesture reuse the same raw source.
- At `0 Hz`, the filtered analysis equals the sampled source within the shared
  filter's established tolerance.
- Post Gain scales the displayed post-high-pass impulse using
  `CycleDsp::irPostGain`; its spectrum remains the normalized diagnostic
  spectrum and does not change with Post Gain.
- Size and other unrelated control callbacks do not synchronously rerasterize
  the sampled curve. Their normal node synchronization refreshes the source.
- High Pass keeps its full stored and audible range. The physical slider uses
  a skew whose midpoint is the stored value for `1 kHz` at the 44.1 kHz
  reference rate, providing half of its track to the range most useful for
  removing DC and low-frequency energy. Keyboard and numeric entry continue to
  operate in Hz over the full `0`–`22.05 kHz` range.

## Test-First Slices

1. Characterize source preparation separately from filtering, including exact
   zero-cutoff identity and source reuse across different cutoffs.
2. Drive the real editor control callback and prove that High Pass changes the
   panel analysis synchronously while its source revision remains fixed.
3. Prove that Post Gain changes displayed impulse amplitude without changing
   source or analysis revisions, and that Size does not rebuild synchronously.
4. Prove the slider's physical midpoint represents `1 kHz` while frequency
   formatting, entry, keyboard steps, endpoints, and stored values remain
   compatible with the shared Cycle 1 mapping.
5. Capture default, zero-cutoff, raised-cutoff, and raised-Post production
   states. Compare the sampled trace against the editable curve at zero cutoff
   and record any remaining sampling/display mismatch before changing the
   mature rasterization boundary. The follow-up
   `cycle-v2-ir-display-identity.md` restores Cycle 1's distinct graphic sample
   view after production evidence exposed audio-downsampler ringing.

## Negative Boundaries

- Do not change `irPrefilterFrequency`, serialized High Pass values, presets,
  or audio cutoff behavior to improve slider ergonomics.
- Do not rerasterize curve geometry for High Pass or Post Gain changes.
- Do not run source preparation, filtering, FFT analysis, or allocation from
  `paint`, `preDraw`, or the per-frame OpenGL render callback.
- Do not add an editor-local transfer function, sampler, FFT, or filter.
- Do not rebuild audio configuration synchronously from the visual callback.
- Do not normalize the Post Gain-scaled waveform back to its old height.

## Completion Criteria

- Source preparation and filtering are separate shared operations, and audio
  configuration still consumes the same resulting filtered impulse.
- Focused interaction tests observe immediate High Pass visuals, source reuse,
  cheap Post Gain display scaling, and no synchronous Size rerasterization.
- Zero cutoff is an identity at the preparation boundary. Production evidence
  either shows the sampled trace nearly overlaying the editable curve or
  identifies a specific authoritative sampling/transform mismatch for the next
  slice; no approximation is accepted as parity.
- The High Pass slider devotes half its physical travel to `0`–`1 kHz` while
  preserving the full Cycle 1 stored/audio range and exact Hz entry.
- Focused tests, Standalone Debug, production screenshots, `git diff --check`,
  hot-loop review, style review, and production-diff review pass before commit.

## Implementation Evidence

- `ImpulseResponseSource` now owns raw sampling separately from
  `ImpulseResponseAnalysis`. The audio convenience path composes the same two
  operations, so convolution still receives the shared filtered impulse.
- The IR panel controller keys raw source preparation by Size, model revision,
  and copied resource revision. High Pass reuses that source and rebuilds only
  filter/FFT analysis from the pre-publication control callback. Automation
  source/analysis revisions make that boundary observable.
- Post Gain is applied with `Buffer::mul` immediately before the OpenGL trace
  is unpolarized. It changes neither source nor analysis revisions and does not
  alter the normalized spectrum.
- The High Pass slider's physical midpoint is `1 kHz` while stored values,
  numeric entry, keyboard stepping, and the shared cubic Cycle 1 transfer keep
  the full `0`–`22.05 kHz` reference range.
- Focused tests prove zero-cutoff filter identity below `1e-5`, sampled/editor
  waveform RMS identity below `0.03`, live High Pass analysis with source
  reuse, cheap Post Gain scaling, deferred Size sampling, and the `1 kHz`
  physical midpoint.
- The follow-up `cycle-v2-ir-display-identity.md` tightens sampled/editor RMS
  identity below `0.005` and guards visible topology. Audio and display now
  share model, sampler, domain, controls, and filtering, while retaining Cycle
  1's intentionally distinct final sampling paths.
- Production fixtures passed for the cached live-control sequence and the
  zero-cutoff identity state. Screenshots were inspected at
  `/private/tmp/cycle-v2-ir-live-visuals.png` and
  `/private/tmp/cycle-v2-ir-zero-cutoff-identity.png`.
- Standalone Debug builds successfully. The full Cycle V2 suite completed with
  10,616 of 10,617 assertions passing; the sole failure is the pre-existing
  edge-help assertion at `TestNodeCanvasHitRouter.cpp:66`. `git diff --check`,
  hot-loop review, style review, and production-diff review passed.
