# Voice Rasterizer Realtime Storage

## Status

Proposed.

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
- Derive worst-case intercept, curve, and waveform capacities from mesh/quality
  limits instead of a magic constant.
- Make insufficient prepared capacity an explicit failure, not an allocation.
- Keep publication as a non-realtime consumer action.

## Semantic Tests

- Allocation guards cover ordinary and chained rendering at supported maxima.
- Capacity boundaries fail safely without changing the last valid result.
- Slice, sort, bake, and publication counters match their lifecycle contracts.
- Audio output remains equivalent to the current mature voice rasterizer.

## Completion Criteria

- No `ensureSize`, vector growth, or snapshot copy is reachable from a prepared
  realtime voice render.
- The former fixed 2048 capacity is absent.

