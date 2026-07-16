# Rasterizer Immutable Snapshot Exchange

## Status

Implemented.

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

## Implementation Evidence

- `RasterizerSnapshotData` is a complete owned generation containing geometry,
  colour points, curves, waveform storage, indices, padding, wrapping, and
  sampleability. Publication builds this value privately and exchanges a
  `shared_ptr<const RasterizerSnapshotData>` with acquire/release atomic shared-
  owner operations.
- `SnapshotView` owns the loaded immutable generation. It contains no producer
  pointer and acquires no producer lock while callers retain geometry or
  waveform views. It is safely copyable, movable, and default-constructible as
  a complete empty generation.
- Snapshot construction and every vector/waveform copy occur before exchange.
  Publishing performs one atomic shared-owner store whose work is independent
  of intercept, curve, colour-point, or waveform lengths.
- One publication allocates one combined snapshot/control block, capacity for
  each non-empty intercept, colour-point, and curve vector, and one contiguous
  two-waveform buffer. It performs at most three vector copies and two waveform
  buffer copies. No copy or waveform allocation occurs during the exchange.
- Tests retain an old generation across another publication and producer
  destruction, verify both generations remain internally coherent, and prove
  an asynchronously publishing writer completes within 100 ms while a reader
  retains the old generation.
- Focused proof: `AmaranthLib_tests '[rasterization][snapshot]'` passes 22
  assertions in debug, AppleClang AddressSanitizer/UndefinedBehaviorSanitizer,
  and ThreadSanitizer builds. ThreadSanitizer reports no race.
- All 426 other discovered tests pass with the already documented exact-float
  reverb comparison excluded.
