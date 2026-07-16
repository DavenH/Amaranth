# Cycle V2 Envelope Preparation Exchange

## Status

Proposed.

Depends on `cycle-v2-smoothed-morph-control.md` for the request source.

## Problem

`EnvelopeSignalProcessor` currently contains request seqlock fields, prepared
slot ownership, publication indices, generation reconciliation, diagnostic
counters, per-voice morph policy, playback state, legacy model state, traversal
storage, and orchestration. The lock-free handoff is required, but its
implementation details leak into a high-level signal processor and obscure the
Envelope lifecycle.

## Design

Extract two narrow concurrency primitives:

```text
LatestEnvelopePreparationRequest
    audio producer -> non-realtime consumer
    complete morph/note request + monotonic generation

PreparedEnvelopeExchange
    non-realtime producer -> audio consumer
    immutable prepared generation + bounded ownership slots
```

The request type owns coherent publication and newest-request coalescing. The
prepared exchange owns triple-buffer lifetime, release/acquire ordering, stale
generation rejection, and selection of a slot that is neither active nor
published. Neither type knows about graph nodes, playback cursors, traversal
grids, or JUCE components.

`EnvelopeSignalProcessor` retains only domain orchestration:

1. advance smoothed morph state;
2. publish a preparation request according to Envelope policy;
3. adopt an available immutable generation at block start;
4. validate existing playback against it;
5. render audio and optional traversal output.

Diagnostics should be returned by the exchange types. Counters need atomic
storage only when genuinely read concurrently.

## Ownership And Realtime Contract

- Audio never increments/decrements the last owning reference to a prepared
  generation and never destroys it.
- The producer never overwrites the active or currently published slot.
- Publication is wait-free for audio and bounded independently of mesh size.
- The request side stores one latest value, not a queue.
- Reset/reconfiguration has an explicit non-concurrent ownership boundary.

## Semantic Tests

- A reader observes either the complete old request or complete new request,
  never mixed red/blue/note fields.
- A burst coalesces to the newest request.
- A stale preparation cannot replace a newer published generation.
- Producer slot reuse never destroys the active reader's generation.
- Adoption performs no allocation, lock, serialization, or reference-count
  destruction on the realtime thread.
- The Envelope integration preserves note position, loop, and release state.

## Deletion Targets

Remove request/published generation atomics, slot arrays, publication indices,
and concurrency counter fields from `EnvelopeSignalProcessor`. Do not leave
forwarding helpers that reproduce the exchange implementation in the processor.

## Completion Criteria

- The processor header exposes domain state rather than memory-ordering
  mechanics.
- Concurrency primitives are independently tested and reusable only where the
  same single-producer/single-consumer contract genuinely applies.
- Existing dynamic Envelope behavior and realtime allocation guarantees remain
  intact.
