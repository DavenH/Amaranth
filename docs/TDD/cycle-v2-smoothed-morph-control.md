# Cycle V2 Smoothed Morph Control

## Status

Proposed.

## Problem

Cycle v2 graph morph inputs are correctly modeled as absolute effective
positions, but the realtime consumers currently treat the first control sample
of a block as an immediate raw float. Trimesh calls `setValueDirect()`, while
Envelope publishes the raw red/blue pair as an asynchronous rasterization
request. Abrupt control changes can therefore create discontinuous mesh output
or discrete Envelope geometry replacements.

Persisted authoring values and graphic requests are immutable data and may
remain scalar. A connected realtime control is mutable per-voice DSP state and
must use the mature `SmoothedParameter` behavior already carried by
`MorphPosition`.

## Design

- Each retained audio voice owns a `MorphPosition` for effective morph state.
- Trimesh uses yellow, red, and blue. Envelope pins time/yellow to zero and uses
  red and blue; do not introduce a partially strong `BiMorphPosition` until a
  genuine Bilinear mesh/domain API exists.
- At block start, connected signals set targets. Unconnected axes set their
  persisted fallback targets independently.
- Advance smoothing by the block's sample count using the established
  sample-rate-normalized convention, then consume current values.
- Never store stateful smoothing objects in immutable DSP configuration or
  graphic rendering state.
- Graphic and traversal requests continue to accept explicit positions and
  must not inherit the audio voice's smoothing history.

Envelope preparation cannot run for every sample. Its smoothed current position
feeds a bounded request policy:

- publish only when movement exceeds an explicit perceptual/domain threshold;
- cap preparation cadence independently of block size;
- retain only the newest request;
- adopt at a block boundary without resetting playback;
- crossfade old/new prepared output if semantic tests show geometry adoption is
  still audibly discontinuous.

## Complexity And Realtime Contract

- Target updates and smoothing are `O(1)` per voice per block.
- Trimesh adds no allocation, graph mutation, or parameter lookup to its
  prepared realtime path.
- Envelope keeps at most one pending request and bounded immutable prepared
  generations; smoothing must not create a queue proportional to automation
  event count.
- No smoothing state is shared between voices.

## Semantic Tests

- A step control input approaches its target monotonically rather than jumping.
- Equivalent block partitions reach equivalent smoothed positions.
- Partially connected axes smooth toward connected values while other axes use
  persisted fallbacks.
- Two voices receiving different targets remain isolated.
- Trimesh output reflects successive smoothed positions without using
  `setValueDirect()` for live input.
- Envelope requests are thresholded, cadence-bounded, and coalesced while the
  final target is eventually prepared and adopted.
- Realtime allocation instrumentation remains zero.
- Graphic rendering is deterministic for an explicit morph position regardless
  of prior audio activity.

## Completion Criteria

- Every realtime morph input is represented by per-voice smoothed state.
- Immutable configurations contain base values, not live smoothing state.
- Envelope preparation remains bounded and continuity is characterized with an
  audible-output semantic test.
- Direct assignment is reserved for initialization/reset and stateless graphic
  requests.
