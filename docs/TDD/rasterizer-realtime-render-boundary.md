# Rasterizer Realtime Render Boundary

## Status

In progress. The voice-render boundary is implemented; envelope realtime
rendering remains under `envelope-renderer-playback-separation.md`.

Depends on `envelope-grid-time-application.md` for envelope-grid semantics.

## Problem

Cycle v1 voice processing calls render-and-publish operations from synthesis
paths. Snapshot publication locks and copies vectors and waveform buffers, and
may allocate. `VoiceRasterizer::updateChainedWaveform` has no render-only
counterpart and creates a fresh local `RenderResult` on each call.

There is also a correctness defect: chained rendering selects the chained
result, but ordinary rendering never selects the ordinary result again. A live
Chain-to-Interpolate change can render ordinary data while `sampler()` returns
stale chained data.

## Goals

- Make ordinary and chained render-only operations explicit.
- Keep snapshot publication entirely outside audio and analysis paths.
- Make the returned/selected result an outcome of each operation, not sticky
  ambient state.
- Reuse prepared storage for every voice render.
- Preserve Cycle v1 synthesis and chaining behavior.

## Target Design

Audio-facing operations return or expose the result they produced:

```cpp
const RenderResult& renderOrdinary(const VoiceRenderRequest& request);
const RenderResult& renderChained(const ChainedVoiceRenderRequest& request);
SamplerView sampler(const RenderResult& result) const;
```

Panel orchestration may explicitly publish one such result. Render operations
must not publish implicitly.

## Semantic Tests

- Chain-to-Interpolate and Interpolate-to-Chain transitions sample the newly
  selected result on the first call.
- The sampler and any publication refer to the same selected result.
- A counting publisher observes zero publications during prepared voice audio,
  including invalid meshes and cleanup.
- Allocation instrumentation observes zero allocations after preparation for
  ordinary and chained rendering across changing phases.
- Existing voice continuity and spillover characterization remains equivalent.

## Completion Criteria

- No Cycle v1 or Cycle v2 audio callback invokes a publishing rasterizer verb.
- No fresh result/vector storage is constructed per voice render.
- Publication and render-only APIs are distinguishable by type and name.

## Implemented Voice Slice

- `VoiceRasterizer::renderOrdinary()` and `renderChained()` return the exact
  reusable `RenderResult` produced by the operation. Both select that result
  immediately, so changing between Interpolate and Chain cannot leave the
  sampler on stale output.
- `publishCurrentResult()` is the sole explicit snapshot-copying operation for
  voice results. Rendering invalid input and cleanup do not publish.
- The temporary per-call voice-slice result is now retained storage. Tests
  verify stable ordinary curve/intercept/waveform storage and the intentional
  bounded two-buffer intercept rotation used by chained continuity.
- Cycle v1 realtime time, frequency, and phase voice calculations now call
  render-only APIs.
- Full regression proof: 414 of 414 discovered tests pass.
