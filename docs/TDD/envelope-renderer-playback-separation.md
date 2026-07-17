# Envelope Renderer And Playback Separation

## Status

Implemented.

## Problem

`EnvRasterizer` combines mesh slicing, curve construction, waveform baking,
snapshot publication, note lifecycle, looping, release preservation,
per-unison cursors, scalar simulation, vectorized playback, preparation/copying,
and random seeds in more than 900 lines.

It slices into a temporary `RenderResult` and copies geometry into another
result. Its manual copy/adopt operations reproduce only selected state. Cleanup
can publish empty intercepts and waveform data alongside retained curves.

## Goals

- Produce one in-place cross-section `RenderResult`.
- Represent prepared envelope data as an immutable complete value.
- Move note, loop, sustain, release, and per-unison cursor state into a playback
  engine.
- Give display padding, playback padding, and release preservation distinct
  concepts.
- Retain mature Cycle v1 envelope behavior without reimplementation.

## Target Design

```text
EnvelopeMesh + EnvelopeRenderRequest
        -> EnvelopeRenderer
        -> PreparedEnvelope
        -> EnvelopePlaybackState / EnvelopePlaybackEngine
        -> samples

PreparedEnvelope -> explicit panel snapshot publication
```

The renderer owns geometry and curve construction. The playback engine owns
time evolution. Neither owns UI publication policy.

## Migration

1. Characterize preparation and playback separately using Cycle v1 fixtures.
2. Make the slicer fill the envelope result in place.
3. Extract prepared envelope data and eliminate manual partial adoption.
4. Extract playback state and preserve note/loop/release transitions.
5. Reduce `EnvRasterizer` to a temporary compatibility facade, then remove or
   narrowly rename it.

## Semantic Tests

- Preparation is independent of note lifecycle and playback cursor position.
- Two playback states can independently consume one prepared envelope.
- Note-on, sustain, loop, note-off, release, logarithmic, and one-sample-per-
  cycle behaviors match Cycle v1.
- Cleanup publishes either one complete empty snapshot or the previous complete
  snapshot—never mixed-generation fields.
- Copying/publishing prepared data preserves every observable field.

## Completion Criteria

- No temporary geometry result is copied into the authoritative result.
- Playback code does not mutate prepared geometry or curves.
- No class combines rendering, playback state machine, and panel publication.

## Implemented Preparation Slice

- `TrilinearMeshSlicer` now fills the envelope's authoritative `RenderResult`
  directly. The temporary result and geometry-vector copies are gone.
- `renderWaveformOnly()` and `renderGeometryOnly()` prepare data without
  snapshot locks or copies; `publishCurrentResult()` is explicit.
- `adoptPreparedData()` no longer publishes while Cycle v2 adopts a prepared
  configuration on its processing path.
- Cleanup clears curves, geometry, guide regions, and waveform together before
  publishing one complete empty generation.
- Cycle v1 and Cycle v2 envelope audio paths call render-only operations.
- Boundary tests cover unpublished preparation, explicit publication,
  complete cleanup, and unpublished prepared-data adoption.
- Note lifecycle, release intent, per-voice cursors, sustain levels, and guide
  sampling cursors now live in `EnvelopePlaybackState` rather than as loose
  renderer members. Independent-state tests prove lifecycle changes and cursor
  resets do not cross between playback instances.
- Preparation now bakes display and looping sampling results before playback.
  Loop and release transitions select an existing result; they no longer
  rebuild curves, replace waveform views, or mutate the display result on the
  processing path. A lifecycle regression test compares all prepared
  intercepts, curves, and waveform samples before and after both transitions.
- `EnvelopePlaybackEngine` now owns note lifecycle, per-voice cursors, loop and
  release transitions, release scaling, state validation, simulation, output
  memory, and sample production. It consumes a const
  `PreparedEnvelopePlaybackView` containing the complete display and loop
  results plus their marker metadata.
- `EnvRasterizer` remains as a compatibility facade for existing Cycle v1 and
  Cycle v2 callers, but delegates playback behavior to the engine. Its
  preparation path no longer implements or reaches into the playback state
  machine.
- A direct semantic test gives two playback engines the same prepared envelope,
  advances them independently, enters release on only one, and proves neither
  mutates the shared intercept or waveform data.
- Focused proof: `AmaranthLib_tests '[rasterization][env][playback]'` passes 26
  assertions without JUCE assertions. The full 421-test suite passes 420 tests;
  the sole failure is the already documented exact-float reverb comparison in
  `audio-bugs.md`, which is unrelated to envelope playback.
