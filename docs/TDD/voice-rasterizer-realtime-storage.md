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
- Derive required intercept, curve, and waveform capacities from the published
  mesh topology and quality settings instead of a magic constant.
- Make insufficient prepared capacity an explicit failure, not an allocation.
- Build enlarged storage off the audio thread and publish an immutable prepared
  replacement. Active voices retain the last valid preparation until they
  adopt the replacement at a safe voice boundary.
- Keep publication as a non-realtime consumer action.

## Topology Growth Contract

Mesh topology remains unbounded for ordinary authoring. When an edit requires
larger rasterizer storage, the model/configuration layer prepares a replacement
off the audio thread and publishes it immutably. The replacement owns an
immutable mesh snapshot as well as its derived capacities; retaining only old
buffers while dereferencing the editor's mutable mesh would not isolate an
active voice from topology changes. Existing voices continue with their last
valid prepared topology; no active rasterizer is resized in place.
Voices adopt the replacement at note start/reset or another explicitly safe
boundary. A defensive file-size limit may protect malformed input, but does
not define the musical authoring contract.

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
