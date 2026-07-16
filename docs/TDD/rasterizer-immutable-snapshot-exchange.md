# Rasterizer Immutable Snapshot Exchange

## Status

Proposed.

Depends on `rasterizer-realtime-render-boundary.md`.

## Problem

`SnapshotView` holds a `CriticalSection` while UI consumers retain references.
A long JUCE/OpenGL paint can block publication. The view retains a raw producer
pointer, default construction is unsafe when accessed, and publication performs
copies and possible allocation while holding the same lock.

## Goals

- Publish one immutable, complete snapshot atomically.
- Let readers retain snapshot storage without holding a producer lock.
- Encode snapshot lifetime in ownership.
- Build/copy snapshot contents outside the exchange lock.
- Keep all publication work outside realtime paths.

## Target Design

Use an owned immutable snapshot, for example a reference-counted value exchanged
under a very short lock or with the platform-appropriate atomic shared-owner
mechanism. Do not expose mutable buffers from the published value.

## Semantic And Concurrency Tests

- Readers always observe geometry, waveform, indices, sampleability, padding,
  and wrapping from one generation.
- A retained old snapshot remains valid after multiple publications and after
  the publisher changes its working result.
- A deliberately stalled reader does not block result construction.
- Producer destruction cannot leave a live view pointing at freed storage.
- ThreadSanitizer publication/read stress reports no race.
- Publication allocation and copy counts are measured and documented.

## Completion Criteria

- Snapshot reads hold no rasterizer-owned lock during painting.
- There is no nullable usable snapshot state and no raw producer lifetime.
- Exchange time is independent of snapshot vector sizes.

