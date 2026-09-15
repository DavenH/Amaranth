# Cycle V2 Oscillator Region Performance

## Status

Implemented (2026-09-15).

## Problem

Audio callback telemetry identifies voice rendering as 98.1% of the mean
eight-voice callback for the representative Baroque Flute graph, but does not
attribute work inside its prepared oscillator region. Optimizing FFT, mesh,
envelope, resampling, or Unison code from this broad measurement would be
speculative.

## Authoritative Implementations

- `GraphAudioExecutor` owns prepared-region dispatch and the total region
  boundary.
- `ChainedOscillatorRegionRuntime` owns cycle scheduling and lane buffering for
  chained recipes; `ChainedOscillatorRecipeRenderer` owns recipe evaluation.
- `SpectralOscillatorRegionRuntime` owns shared-frame scheduling, per-lane
  cycle reconstruction, resampling, and final mixing;
  `SpectralOscillatorFrameRenderer` owns spectral recipe evaluation.
- `AudioPerformanceMetrics` owns the lock-free realtime handoff and
  non-realtime aggregation. It must observe these implementations without
  reproducing their behavior.

## Design

Add a small `OscillatorRegionPerformanceCounts` POD to the prepared process
context. When callback telemetry is enabled, `GraphAudioExecutor` supplies one
callback-owned instance and the authoritative renderers accumulate:

- total prepared-region duration and region calls;
- recipe duration and recipe render calls (`renderCycle` or `renderFrame`);
- spectral lane-cycle reconstruction duration and lane-cycle calls;
- final lane-mix duration and mixed-lane count.

These are nested measurements beneath the existing voice-rendering phase, so
the telemetry schema exposes them separately as `oscillatorStages`, not as
additional top-level callback stages. Operation totals are reported alongside
the existing workload counts so results can be normalized by frames, cycles,
and lanes.

Timing reads occur only when the opt-in callback sample exists. The realtime
path performs no allocation, locking, formatting, publication retry, graph
search, or node-kind dispatch.

## Measurement Plan

1. Add aggregation/schema tests and prove output remains identical with
   profiling enabled.
2. Capture stable 0/1/4/8-voice windows for a spectral oscillator graph and a
   chained oscillator graph in Debug and Release standalone builds.
3. Compare absolute time, percentage of region time, and cost per recipe/lane
   operation. Propose an optimization only where the breakdown isolates a
   material stage.

## Completion Criteria

- Nested oscillator distributions and operation counts are available through
  `inspectAudioPerformance` without changing the existing callback phases.
- Focused tests prove aggregation, disabled-path behavior, output parity, and
  zero realtime allocation/locking.
- Representative chained and spectral Debug/Release measurements are recorded
  here with artifacts and an evidence-based optimization recommendation.
- Modified hot loops pass the scalar-math self-check and `git diff --check`;
  applicable targets and focused tests pass.

## Measurements

The fixtures use the same 0/1/4/8-note sequence for both graphs. Later steps
add higher notes, so operation counts and recipe sizes vary with the voice
count; these results identify stage ownership and representative callback cost,
not a claim that each added voice has identical work.

All durations below are mean milliseconds per callback. Counts are totals over
the captured window.

### Spectral: Baroque Flute

| Build | Voices | Callback | Region | Recipe | Lane cycle | Mix | Regions | Recipes | Lane cycles |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Debug | 0 | 0.816 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Debug | 1 | 2.458 | 1.912 | 1.817 | 0.089 | 0.002 | 45 | 275 | 274 |
| Debug | 4 | 3.366 | 3.136 | 3.013 | 0.115 | 0.002 | 184 | 1,582 | 1,579 |
| Debug | 8 | 5.893 | 5.552 | 5.292 | 0.244 | 0.003 | 368 | 2,901 | 4,215 |
| Release | 0 | 0.065 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Release | 1 | 0.340 | 0.260 | 0.244 | 0.013 | 0.001 | 44 | 269 | 268 |
| Release | 4 | 1.340 | 1.209 | 1.144 | 0.053 | 0.002 | 172 | 1,480 | 1,477 |
| Release | 8 | 2.346 | 2.167 | 2.032 | 0.114 | 0.004 | 344 | 2,713 | 3,941 |

At eight voices in Release, recipe generation is 93.8% of region time, lane
reconstruction is 5.2%, and final mixing is 0.2%. The window performs about
63.1 recipes per callback at approximately 32.2 microseconds per recipe.

Artifacts:

- `/private/tmp/cycle-v2-oscillator-spectral-debug.json`
- `/private/tmp/cycle-v2-oscillator-spectral-release.json`

### Chained: African Horn

| Build | Voices | Callback | Region | Recipe | Lane cycle | Mix | Regions | Recipes | Mixed lanes |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Debug | 0 | 0.940 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Debug | 1 | 1.865 | 1.226 | 1.187 | 0 | 0.004 | 43 | 393 | 129 |
| Debug | 4 | 1.952 | 1.706 | 1.658 | 0 | 0.005 | 180 | 2,320 | 540 |
| Debug | 8 | 4.525 | 4.178 | 4.068 | 0 | 0.007 | 344 | 5,915 | 1,032 |
| Release | 0 | 0.100 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Release | 1 | 0.295 | 0.198 | 0.184 | 0 | 0.003 | 44 | 402 | 132 |
| Release | 4 | 1.032 | 0.879 | 0.830 | 0 | 0.009 | 172 | 2,216 | 516 |
| Release | 8 | 2.058 | 1.836 | 1.741 | 0 | 0.015 | 344 | 5,916 | 1,032 |

At eight voices in Release, recipe generation is 94.8% of region time and
final mixing is 0.8%. The window performs about 137.6 recipes per callback at
approximately 12.7 microseconds per recipe. Chained rendering has no separate
spectral lane-reconstruction phase, so its lane-cycle metric is intentionally
zero.

Artifacts:

- `/private/tmp/cycle-v2-oscillator-chained-debug.json`
- `/private/tmp/cycle-v2-oscillator-chained-release.json`

## Recommendation

Do not optimize region dispatch, lane mixing, or spectral reconstruction first.
They collectively account for about 5.5% of spectral region time and less than
1% of chained region time at the representative eight-voice Release workload.

The next profiling boundary should be inside the two authoritative recipe
renderers:

1. Split `SpectralOscillatorFrameRenderer::renderFrameInternal` into graph/time
   evaluation, forward transform, spectral operations, and inverse transform.
2. Attribute `ChainedOscillatorRecipeRenderer::renderCycle` by prepared
   operation family and raster/evaluation work.
3. Add operation counts beside those durations before changing algorithms, so
   repeated invariant work and O(n) scans can be distinguished from required
   transform and raster costs.

This is the narrowest evidence-backed boundary: recipe evaluation consumes
roughly 94--95% of oscillator-region time in Release for both independent
rendering paths.

## Verification

- `CycleV2_tests '[cycle-v2][audio][performance]'`: 308 assertions in 5 cases.
- `CycleV2_tests '[cycle-v2][runtime][realtime]'`: 138 assertions in 13 cases.
- `CycleV2_tests '[cycle-v2][audio-device][realtime][performance]'`: 12
  assertions in 1 case.
- Debug and Release `CycleV2` standalone targets build successfully.
- Both profiling fixtures complete for Debug and Release with 0/1/4/8 voices.
