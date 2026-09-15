# Cycle V2 Oscillator Region Performance

## Status

Source-rendering optimization in progress (2026-09-15). The original region
and recipe attribution slices are implemented.

## Source Optimization Design

Start from `3db65105` on `cycle2/optimization-2`. First split each source into
morph resolution, rasterization (including request/configuration), sampling,
gain/phase scaling, and stereo copy. Use optional callback-owned POD counters
with nanosecond accumulation for sub-microsecond stages; export totals, calls,
and means through the existing non-realtime telemetry handoff. Shared lane
rasterization accepts the same optional measurements without depending on
Cycle V2. No render behavior, ownership, random draws, or lifecycle changes
belong in this instrumentation slice.

The authoritative implementations remain `TrilinearMeshRasterizer`,
`VoiceRasterizer`, `WaveformBakePolicy`, and `WaveformSampler`. Prepared meshes
and guide providers retain their current ownership. Each voice/lane retains
its mutable chaining, smoothing, and random state. Configuration setters and
mesh-validity checks inspected so far are O(1); harmonic positions already
reuse the shared default `LogRegions` table when dimensions match. Do not add
another position cache without measured evidence.

After baseline captures, record a concrete optimization and its complexity
contract here before implementation. Preserve shared-core behavior exactly;
do not cache a complete source solely on unchanged morph because guide noise,
chaining advancement, and lifecycle state can still change. No new adapter or
replacement rasterization algorithm is planned.

The spectral frame renderer was already at the translation-unit review
threshold. Its capture plumbing repeated frame identity at every stage. Move
that metadata into `SpectralFrameCapture` beside the authoritative capture sink;
the renderer delegates publication and becomes smaller while timing is added.
This helper translates capture metadata only and owns no rendering behavior.

Completion for this follow-up requires a measured source optimization,
operation-count evidence for any complexity claim, repeated Release spectral
and chained 0/1/4/8-voice captures, exact-output/parity and realtime checks,
refactor/style review, and committed changes. Deferred speculative work must
be identified explicitly rather than represented as implemented.

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

## Production Hotspot Attribution

The follow-up slice keeps Release telemetry available and divides recipe time
by the prepared operation families that perform production work:

- time-domain mesh source rendering, including morph resolution;
- spectral mesh source rendering, including morph resolution;
- forward transforms;
- inverse transforms;
- graph combining: pan, magnitude transfer, add, and multiply.

Each family reports callback duration distributions and executed-operation
counts. Unattributed recipe time remains visible as the difference from total
recipe duration and covers fixed per-frame setup and final output copies. This
avoids assigning orchestration overhead to a DSP operation family.

The slice is complete when Release 0/1/4/8-voice captures for both representative
graphs identify the dominant production operation family, tests cover the new
handoff/schema, and output parity plus realtime allocation/locking coverage
remain green.

### Production Release Measurements

The telemetry is present in Release builds and is enabled only during a
measurement window. With telemetry disabled, the renderer receives no
performance-count pointer and performs neither timestamp reads nor counter
updates. The automation endpoint successfully captured all fields from the
Release standalone executable.

Durations below are mean milliseconds per callback. `Other` is total recipe
time minus the measured operation families and covers per-frame setup and final
output copies.

| Graph | Voices | Recipe | Time source | Spectral source | Forward FFT | Inverse FFT | Combining | Other |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Baroque Flute | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Baroque Flute | 1 | 0.301 | 0.052 | 0.186 | 0.016 | 0.012 | 0.030 | 0.005 |
| Baroque Flute | 4 | 1.367 | 0.209 | 0.925 | 0.049 | 0.044 | 0.119 | 0.020 |
| Baroque Flute | 8 | 2.217 | 0.343 | 1.524 | 0.071 | 0.065 | 0.183 | 0.031 |
| African Horn | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| African Horn | 1 | 0.167 | 0.162 | 0 | 0 | 0 | 0.003 | 0.002 |
| African Horn | 4 | 0.706 | 0.684 | 0 | 0 | 0 | 0.013 | 0.009 |
| African Horn | 8 | 1.649 | 1.603 | 0 | 0 | 0 | 0.026 | 0.020 |

At eight voices, Baroque Flute spends 68.8% of recipe time rendering spectral
mesh sources and 15.5% rendering its time-domain source. Graph combining is
8.3%, while both transforms together are only 6.1%. Its topology executes four
spectral source operations, one time source, one forward transform, one inverse
transform, and four combining operations per recipe. The spectral source cost
is therefore both individually material (about 6.0 microseconds per operation)
and repeated four times per generated frame.

African Horn spends 97.2% of recipe time rendering time-domain mesh sources.
It executes two time sources and one combining operation per recipe. At eight
voices, time sources average about 5.8 microseconds per operation while the
combining operation averages about 0.19 microseconds.

Two operation-level Release captures put the eight-voice callback at
2.30--2.52 milliseconds for Baroque Flute and 1.90--1.99 milliseconds for
African Horn. Despite that absolute run-to-run variation, the dominant shares
were stable: spectral source rendering remained about 68% for Baroque Flute
and time source rendering remained about 97% for African Horn. Normal
production playback has no detailed-timing overhead because telemetry is
disabled outside an explicit measurement window.

Artifacts:

- `/private/tmp/cycle-v2-oscillator-spectral-production.json`
- `/private/tmp/cycle-v2-oscillator-chained-production.json`

### Production Optimization Order

1. Profile and optimize spectral mesh rasterization in
   `TrimeshBlockwiseDsp::rasterizePrepared` and harmonic sampling in
   `renderPreparedHarmonicsInto`. This is the largest aggregate spectral cost.
2. Profile and optimize the shared time-domain mesh path used through
   `OscillatorLaneRasterizer`; it dominates the chained graph and remains the
   second-largest spectral-graph family.
3. Leave FFT/IFFT and graph combining alone until rasterization work is
   reduced. Together they are materially smaller than source rendering in the
   representative Release workloads.

## Verification

### Immutable guide sampling design

Prepare the guide downsampling products off-thread alongside each immutable
Cycle V2 guide snapshot. `WaveformBakePolicy` requests lengths `tableSize / r`
for integer resolution ratios 1 through 256. A shared
`PreparedGuideCurveTable` stores those exact products using the existing
`Buffer::downsampleFrom`; runtime performs an O(1) length lookup and contiguous
copy. Unsupported sizes retain the authoritative downsampling path. Preparation
cost and memory are bounded by `tableSize * H(256)` floats per guide (less after
duplicate lengths are removed); mutable noise/phase/vertical offsets are never
cached. The existing `GuideCurveTableDsp` continues to apply them, in the same
order, on every render. Source instances sharing a guide provider share these
immutable products. No lifecycle or random state is moved or skipped.

This removes strided sampling from prepared calls; the asymptotic destination
cost stays O(sample count). Optional work counters distinguish prepared copies
from fallback downsampling. Tests must compare exact samples over all supported
lengths, fallback lengths, seeds, phase/noise/DC combinations, and preparation
replacement. The stable end state is shared-core guide sampling with optional
immutable preparation; no compatibility layer or copied DSP is introduced.

Implemented: two Release captures reduce eight-voice spectral rasterization
from 5.165--5.347 to 4.846--4.859 microseconds/source (about 7.7% using the
two-run means). Recipe time moves from 2.289--2.368 to 2.237--2.251 ms/callback.
The deterministic Baroque WAV is byte-for-byte identical before/after.
Artifacts: `/private/tmp/cycle-v2-source-guide-spectral-{1,2}.json` and
`/private/tmp/cycle-v2-source-spectral-{before,guide}.wav`. Shared guide DSP
tests pass 12,580 assertions in 3 cases, including counters proving zero
runtime downsampling at all 256 supported resolution ratios. The constant
ratio bound is shared with the bake policy to prevent preparation drift.

### Point-sampling derivative design

The chained CPU sample attributes 50 samples to waveform finalization, alongside
151 in curve preparation and 139 in baking. Both representative oscillator
paths use point sampling; segment integral areas are computed but never read.
Add an explicit shared request capability to omit integral preparation while
retaining identical waveform points, slopes, clipping, padding, guide handling,
and sampling. The default remains full waveform preparation for existing
callers. Cycle V2 oscillator sources select point-only preparation. An omitted
area buffer is empty, so integral sampling cannot accidentally consume stale
data. Full renders rebind it; incremental rebuilds honor the same capability.

Optional bake counters report waveform segments and integral segments through
source telemetry. The expected work reduction is from N integral segments per
raster to zero, while waveform and slope work remains O(N). Tests compare
exact point samples and transitions back to full preparation. No topology,
curve-evaluation, noise, or chaining algorithm changes belong in this slice.

### Source-stage baseline (Release)

The first successful eight-voice split, in microseconds per source operation:

| Source | Morph | Rasterization | Sampling | Gain/phase | Stereo copy |
|---|---:|---:|---:|---:|---:|
| Baroque spectral | 0.114 | 5.165 | 0.730 | 0.051 | 0.035 |
| Baroque time | 0.147 | 4.724 | 0.491 | 0.026 | 0.055 |
| African Horn time | 0.108 | 4.811 | 0.885 | 0 (unity) | 0.051 |

Artifacts: `/private/tmp/cycle-v2-source-baseline-spectral-2.json`,
`/private/tmp/cycle-v2-source-baseline-spectral-3.json`, and
`/private/tmp/cycle-v2-source-baseline-chained-1.json`. These are the unchanged
0/1/4/8-voice fixtures. The first launch failed to produce a report; the retry
disabled focus-by-bundle-ID because another worktree has the same bundle ID.

A five-second macOS sample of eight-voice spectral playback, with timing
telemetry disabled, attributes 359 samples to `rasterizePrepared`: 203 beneath
waveform baking, including 97 in guide baking and 68 in derivative/integral
finalization. Approximately 60 are in mesh slicing. Within guide baking,
55 samples are in strided `Buffer::downsampleFrom`. This is evidence to examine
immutable guide resampling and unused integral preparation before introducing
topology caches or direct harmonic rasterization. The sample is statistical,
not an exact duration decomposition. Artifact:
`/private/tmp/cycle-v2-source-spectral-sample.txt`.

Instrumentation validation: Release build passes; the combined performance,
realtime and output-parity selection passes 474 assertions in 19 cases;
shared voice rasterizer tests pass 70 assertions in 9 cases. The source timer
does no timestamp reads or counter updates when its optional pointer is null.

- `CycleV2_tests '[cycle-v2][audio][performance]'`: 313 assertions in 5 cases.
- `CycleV2_tests '[cycle-v2][runtime][realtime]'`: 138 assertions in 13 cases.
- `CycleV2_tests '[cycle-v2][audio-device][realtime][performance]'`: 12
  assertions in 1 case.
- Debug and Release `CycleV2` standalone targets build successfully.
- Both profiling fixtures complete for Debug and Release with 0/1/4/8 voices.
