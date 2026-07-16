# Envelope Renderer And Playback Separation

## Status

Proposed.

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

