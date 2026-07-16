# Voice Rasterizer Realtime Storage

## Status

Implemented.

## Problem

`VoiceRasterizer` preallocates a fixed chained waveform capacity of 2048, but
`updateChainBuffers` may grow `ScopedAlloc` from the render path when a chain
requires more samples. The required maximum is not derived from voice quality,
mesh, or block preparation, so realtime allocation safety is accidental.

## Cost Contract

Voice preparation establishes all ordinary, slice, chained-intercept, curve,
and waveform capacities. Rendering a prepared voice performs zero allocation
and zero snapshot publication. Sorting remains permitted where deformed
intercepts genuinely require phase ordering.

## Design

- Add an explicit voice rasterization preparation specification.
- Derive required intercept, curve, and waveform capacities from the published
  mesh topology and quality settings instead of a magic constant.
- Make insufficient prepared capacity an explicit failure, not an allocation.
- Build enlarged storage off the audio thread while the established Cycle
  callback lock excludes rendering. Commit the enlarged topology and prepared
  storage together at that safe boundary.
- Keep publication as a non-realtime consumer action.

## Topology Growth Contract

Mesh topology remains unbounded for ordinary authoring. Cycle already commits
time-mesh edits while its callback lock excludes audio rendering. That same
non-audio-thread boundary now derives capacity from every time-layer mesh and
prepares every voice and per-layer chaining state before releasing the lock.
No active rasterizer is resized while rendering. If a caller misses preparation
or presents a larger mesh, the rasterizer explicitly rejects it and preserves
the last sampleable waveform. A defensive file-size limit may protect malformed
input, but does not define the musical authoring contract.

## Semantic Tests

- Allocation guards cover ordinary and chained rendering at supported maxima.
- Capacity boundaries fail safely without changing the last valid result.
- Topology growth does not affect an active voice until safe-boundary adoption.
- Slice, sort, bake, and publication counters match their lifecycle contracts.
- Audio output remains equivalent to the current mature voice rasterizer.

## Completion Criteria

- No `ensureSize`, vector growth, or snapshot copy is reachable from a prepared
  realtime voice render.
- The former fixed 2048 capacity is absent.

## Implementation Notes

- `VoiceRasterizerPreparation` derives intercept, curve, and worst-case guide
  waveform capacities from the largest published time-layer topology.
- `VoiceRasterizer::prepare` reserves ordinary, sliced, chained, state, and
  waveform storage outside rendering. Prepared waveform placement cannot call
  `ensureSize`.
- Ordinary and chained renders validate mesh/state capacity before mutation.
  Capacity failure preserves the last sampleable result and is observable in
  diagnostics.
- Cycle prepares every voice at startup, preset replacement, layer-count edits,
  and time-mesh edit commit while its existing audio lock excludes callbacks.
- Allocation guards prove zero heap allocation for prepared ordinary and
  chained renders. Semantic tests cover topology rejection, later preparation,
  storage reuse, publication separation, and output parity.
