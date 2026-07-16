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

## Open Product Contract

Implementation requires an authoritative upper bound on voice mesh topology
(or a non-realtime re-preparation contract when topology grows). The current
application permits mesh edits after voice construction and defines no maximum
cube/intercept count. Preparing from the current mesh would therefore become
invalid after a larger edit, while substituting another fixed capacity for
2048 would preserve the original defect under a different name.

The owning voice/layer lifecycle must specify one of these before this TDD can
be implemented honestly:

- a supported maximum cube/intercept count enforced by mesh authoring; or
- a non-audio-thread topology publication step that suspends/re-prepares voice
  rasterizers before the enlarged mesh becomes audible.

## Semantic Tests

- Allocation guards cover ordinary and chained rendering at supported maxima.
- Capacity boundaries fail safely without changing the last valid result.
- Slice, sort, bake, and publication counters match their lifecycle contracts.
- Audio output remains equivalent to the current mature voice rasterizer.

## Completion Criteria

- No `ensureSize`, vector growth, or snapshot copy is reachable from a prepared
  realtime voice render.
- The former fixed 2048 capacity is absent.
