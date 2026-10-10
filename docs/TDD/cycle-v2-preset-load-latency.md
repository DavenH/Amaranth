# Cycle V2 preset-load latency telemetry

Status: Measurement and realtime traversal warmup optimization complete (2026-10-08)

## Problem and baseline

The preset sidebar calls the existing graph replacement service, then
`NodeCanvas::loadGraphFromFile`. That path loads the document and calls
`GraphPresentationModel::refresh` synchronously on the message thread when
compilation is required. The first canvas paint occurs after that work returns.
An idle delay in agent automation is a fixed sleep, not a queue-idle signal.

The existing canvas telemetry, reset before each load, measured one warm-app
Baroque Flute load at 192.094 ms synchronous refresh (95.701 ms preview audio,
13.090 ms extraction). Its first canvas paint took 788.704 ms: 652.809 ms
node tiles with 21 cache misses, 75.554 ms Spy tiles with two misses, and
45.739 ms cable bodies. Organ 4 measured 330.244 ms synchronous refresh
(211.263 ms preview audio, 20.793 ms extraction), followed by a 584.233 ms
paint: 494.652 ms node tiles with 31 misses, 51.797 ms Spy tile, and
33.016 ms cables. These are separate stage durations, not yet measured as a
single click-to-visible interval. Baseline report:
`/private/tmp/cycle-v2-preset-load-baseline-report.json`.

## Design

- `CanvasPerformanceMetrics` owns a per-load timeline. `NodeCanvas` supplies
  load-start, synchronous-return, the next posted message turn, first full
  canvas paint, and a posted message turn after that paint. Posted turns are
  reproducible queue markers, not a claim that the entire queue is empty.
  New loads supersede old markers.
- `GraphPresentationPerformanceMetrics` owns timings for compilation, runtime
  trace, cancellation wait, and published presentation facts. These add to
  existing preview audio and extraction stages.
- The existing node-layer cache remains authoritative for drawing. Its
  performance observer records the slowest cold node tiles by node ID; it
  does not change rendering or cache policy.
- A focused agent fixture opens representative presets from an already
  running app, yields the message queue, and exports the actual first-paint
  markers. It asks `openGraph` to skip its normal state snapshot, because that
  snapshot holds the message thread after load return and distorts this
  measurement.

No graph or rasterizer behavior changes in this measurement slice. The
message thread owns graph replacement and first paint; preview execution may
move to the existing presentation worker only after compilation, resource
ownership, cancellation, audio publication, and stale-result behavior are
designed and tested. Moving only Spy extraction would leave the larger cold
node paint cost on the message thread.

`NodeCanvas` remains responsible for the load and paint lifecycle, with only
timestamp calls added. `CanvasPerformanceMetrics` owns timing state and
serialization. `NodeCanvasPresentation` reports cache-miss tile timings to
its existing observer; it does not choose a new cache or rendering policy.
The two high-level C++ files were already above the 1,200-line review trigger
(2,892 and 1,435 lines respectively). Their 19 new lines only forward facts
to existing instrumentation boundaries; no domain behavior or duplicate
decision site was added. A later paint optimization should extract rendering
work from these classes rather than growing them.

## Measured result

Four sequential warm-app loads in Standalone Debug, alternating Baroque Flute
and Organ 4, produced the following per-load timings in milliseconds. Time
zero is entry to `NodeCanvas::loadGraphFromFile`, immediately after the
sidebar's graph-replacement decision. The first-paint endpoint is completion
of the first full JUCE canvas paint, before OS composition.

| Preset | Return | First paint end | Next turn | Refresh | Compile | Preview audio | Node paint | Reverb tile | Spy paint |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Baroque Flute 1 | 303 | 1,467 | 1,503 | 197 | 73 | 93 | 660 | 329 | 82 |
| Organ 4 1 | 395 | 1,959 | 2,002 | 329 | 46 | 211 | 503 | 327 | 52 |
| Baroque Flute 2 | 268 | 1,278 | 1,317 | 212 | 42 | 92 | 578 | 297 | 74 |
| Organ 4 2 | 367 | 1,861 | 1,911 | 324 | 49 | 203 | 467 | 329 | 54 |

The posted callback queued at load return ran after the first canvas paint in
all four runs. It is therefore not an earlier indicator that the UI is ready.
Organ 4 has no explicit `probes` in its preset, but still pays for whole-graph
compact-preview execution and a default output Spy tile. `PreviewAudio` is
the shared graph preview render for compact node previews and probes, not a
Spy-only calculation. The Reverb tile alone costs about 300–330 ms in the
first node paint. Raw telemetry:
`/private/tmp/cycle-v2-preset-load-final-report.json` and filtered log
`/private/tmp/cycle-v2-preset-load-final-log.txt`.

The same four-load fixture with the app focused reported first-paint endpoints
of 1,513/1,236 ms for Baroque Flute and 2,015/1,869 ms for Organ 4, with
Reverb tiles at 314/301 ms and 339/318 ms respectively. Focus did not remove
the cold-paint cost. Its report is
`/private/tmp/cycle-v2-preset-load-focused-report.json`; the filtered log has
no assertion, error, or crash. These are Standalone Debug automation timings;
they should not be treated as Release-build or OS-present measurements.

In Standalone Release with the app focused, repeated Baroque Flute loads
finished their first paint in 286/188 ms; Organ 4 finished in 334/308 ms.
The synchronous refresh was 36–60 ms, including 24–47 ms whole-graph
preview execution. Reverb's first node tile was 32–36 ms and Spy paint was
6–9 ms. The load-return-to-paint-start interval was 55–170 ms. The Release
report is `/private/tmp/cycle-v2-preset-load-release-report.json`.

Two real sidebar-row click paths also passed in Release: `24-h` finished its
first paint 153 ms after load entry, and `30-bmi` in 80 ms. The click
callback reaches the same `loadGraphFromFile` timeline. Three larger graphs
(`thrash-guitar-3`, `altosax-2`, and `muted-trumpet`) finished in 224, 210,
and 207 ms respectively. Reports:
`/private/tmp/cycle-v2-preset-click-latency-report.json` and
`/private/tmp/cycle-v2-large-preset-load-report.json`. No measured case
reproduced the reported subjective ~750 ms. The telemetry starts at load
entry, so it excludes the preceding sidebar selection and any dirty-document
prompt, and the paint endpoint precedes OS composition.

## Worker recommendation

Full-graph preview execution on preset replacement could move to the existing
presentation worker with a generation token, preserving atomic publication
and cancellation on a later load. Compilation and audio resource publication
need an explicit ownership boundary first. In Release, this stage took only
24–47 ms on the representative loads, so moving it is not justified by the
current first-paint measurements. The stage is shared by compact node
previews and probes; an explicit Spy-only worker would save less. Reverb's
first tile also fell from ~300 ms in Debug to ~30 ms in Release. If a
specific slow Release preset is reproduced, use this fixture first and then
target its actual dominating stage. The existing presentation worker, local
Reverb preview worker, and node-layer cache are the authoritative starting
points for such a change.

## Debug preset-load composition measurement

The first-paint timeline is authoritative for user-visible preset replacement.
`CanvasPerformanceMetrics` owns that timeline. `GraphPresentationPerformanceMetrics`
owns nested graph-refresh measurements; `NodeCanvasPresentation` owns paint-stage
measurements. `NodeWorkspace` is the existing owner of keyboard/sidebar setup
and audio-plan publication after `NodeCanvas::loadGraphFromFile` returns.

This measurement adds narrow timing reports from `NodeWorkspace` to the
existing canvas telemetry and records paint stages only for the first complete
canvas paint. It does not move work or copy graph, rendering, or audio behavior.
The timing categories must form non-overlapping intervals from canvas load
entry through first paint. Nested graph-refresh and paint stages are selected
only as children of their containing interval. The temporary fixture and chart
may be deleted after the diagnosis; production telemetry remains at the
existing owner boundary.

Sixteen distinct, unmodified repository presets were loaded twice each in one
warm Standalone Debug app, first in forward order and then in reverse. The
chart and its per-run/mean CSV files are in
`output/preset-load-debug-2026-10-08/`. The chart's segments sum to each
load's first complete canvas paint endpoint; the individual run totals are
shown as white ticks. All 32 automation inspections succeeded. The total
range of preset means was 272–1,914 ms; seven of 16 means exceeded 750 ms.

| Stage | Mean across presets | Share of mean total |
| --- | ---: | ---: |
| Realtime audio graph preparation | 285 ms | 34% |
| Cold node painting | 243 ms | 29% |
| Graph preview audio | 97 ms | 11% |
| Spy rail painting | 68 ms | 8% |
| Remaining graph refresh | 79 ms | 9% |
| Canvas setup, scheduling, cables and other paint | 72 ms | 9% |

The audio graph is prepared synchronously on the message thread during
`StandaloneAudioEngine::publishGraph`. `RealtimeGraphRenderer::prepareGraph`
prepares eight realtime voices and one global execution, including each
processor's preparation. The measured graph-preparation range was 88 ms for
Subbass to 930 ms for Organ 4. Organ 4 also spent 500 ms painting node tiles;
its slowest tile was Reverb at 319 ms. Baroque Flute spent 586 ms painting
nodes, with a 295 ms Reverb tile. These are distinct costs. In contrast, the
residual interval between workspace setup and paint start averaged 17 ms, so
message-queue delay was not the major factor in this Debug sample.

This is a diagnosis, not an optimization. A worker design should account for
the full audio graph publication lifecycle and stale graphs before moving
preparation off the message thread. Cold tile painting also needs attention
for the larger presets. These Debug measurements do not imply the same costs
in Release. The chart starts at canvas load entry and ends after its first
full JUCE paint, before macOS composition; sidebar click dispatch preceding
load entry is outside the interval.

Architecture review: the touched `NodeCanvas.cpp` (2,916 lines) and
`NodeCanvas.h` (379 lines) are already above size review thresholds. Their
added code only forwards timestamps at the existing load and paint lifecycle
boundaries. `NodeWorkspace.cpp` (790 lines) remains the sole owner of audio
publication; the new markers supply timing facts, not a second publication
decision. `CanvasPerformanceMetrics.cpp` (725 lines) owns the measurement
state and JSON export. No existing behavior was copied, no policy site was
added, and there is no deletion target for this instrumentation. Any future
paint optimization should extract rendering responsibility from `NodeCanvas`
instead of expanding it. This change adds fewer than 20 lines to that class
and makes no behavior change.

## Realtime preparation root cause and optimization

`RealtimeGraphRenderer::prepareGraph` prepares eight voice executions and one
global execution before publishing the graph. Each voice prepares its
processors through `GraphAudioExecutor`; a Trimesh processor called
`TrimeshGridwiseDsp::prepare` with the default `prewarmTraversalGrid = true`.
That method rendered every traversal column into a scratch buffer for each
Trimesh node and voice. Organ 4 has nine Trimesh nodes. The rendered columns
were discarded. Realtime execution sets `captureTraversalGrid = false` in
`GraphAudioExecutor`, so it never consumes these prepared grid columns.
Diagnostic execution sets the flag true and already disables warmup before
rendering its requested grid. The Trimesh grid DSP and rasterizer remain the
authoritative implementations; no rendering algorithm is copied.

The realtime renderer now sets `prewarmTraversalGrid = false` on its prepared
spec before preparing any voice or global executor. This boundary expresses
the execution mode once and leaves diagnostic preparation unchanged. The
processor cache signature includes the flag, so a later diagnostic request
cannot accidentally reuse an incompatible preparation. Sampling buffers and
blockwise rasterizer preparation remain in place. Expected preparation cost
falls by the discarded `O(voice count × mesh nodes × traversal columns)`
bakes; audio execution and published graph ownership remain unchanged.

A temporary Debug experiment changed only this flag in the standalone audio
publisher. Its four-load fixture measured graph preparation with and without
warmup: Organ 4 932 → 37 ms, Alto Sax 2 390 → 40 ms, Baroque Flute 277 →
35 ms, and Subbass 83 → 1 ms. The temporary publisher edit was reverted;
the final location is `RealtimeGraphRenderer::prepareGraph`, shared by
standalone and offline realtime rendering. Existing Trimesh parity tests
compare prewarmed and non-prewarmed diagnostic grids exactly. The targeted
realtime audio test now asserts the prepared realtime spec omits warmup while
still rendering an audible graph.

The 16-preset sweep was repeated twice in each build before and after the
change while macOS reported AC power. All 128 load inspections succeeded.
The matched means were:

| Build | Mean audio graph prepare | Mean first paint | Organ 4 prepare | Organ 4 first paint |
| --- | ---: | ---: | ---: | ---: |
| Debug before | 298 ms | 879 ms | 968 ms | 1,992 ms |
| Debug after | 27 ms | 600 ms | 32 ms | 1,020 ms |
| Release before | 42 ms | 135 ms | 154 ms | 316 ms |
| Release after | 16 ms | 108 ms | 24 ms | 187 ms |

The optimization removes 270 ms of mean Debug preparation and 26 ms of mean
Release preparation. Organ 4 preparation fell by 936 ms in Debug and 130 ms
in Release. The first-paint reduction is similar but not identical because
preview and painting costs vary between runs. Mean cold node painting stayed
near 250 ms in Debug and 38 ms in Release. Matching reports, CSV tables, and
interactive charts are under `output/preset-load-ac-2026-10-08/` and
`output/preset-load-ac-optimized-2026-10-08/`. The latter includes a paired
before/after plot. The `workspace-stage-scales.html` chart gives each of the
five workspace stages its own horizontal scale, since plan copy, voice
duration, and layout are too small to see on the full-load scale. The earlier
battery-powered samples remain separate.

This change has no adapter or deletion target. `GraphAudioExecutor` remains the
owner of processor preparation and the sole setter of traversal capture per
execution mode. `RealtimeGraphRenderer` owns only the realtime spec policy.
Cold node painting remains a separate first-paint cost. Reverb preview tiles
create a full-resolution heatmap on a cache miss, through the existing
`NodePreviewRenderer` and `ScalarSurfaceMaterialEvaluator`; any paint change
should preserve that mature surface material behavior and benchmark the
visible tile size before reducing its raster workload.

Architecture review: `RealtimeGraphRenderer.cpp` is 619 lines. The two added
lines state an execution-mode fact at its existing preparation boundary;
voice and global preparation, processor ownership, and graph publication
remain with their existing collaborators. `TrimeshGridwiseDsp` already owns
the warmup decision and its operation counter. The change removes a repeated
render operation without moving a domain algorithm or adding a second policy
site. No extraction is warranted for this narrow change. Before and after
operation count for each realtime Trimesh grid is `traversalColumnsFor(...)`
warmup bakes versus zero; the existing grid DSP test asserts zero bakes when
the flag is false, and the realtime test asserts that the flag is false.
The broader realtime test filter has five existing failures; all five
reproduced when the original warmup setting was restored temporarily. They
are recorded in `docs/TDD/audio-bugs.md` and do not show a new regression.

## Completion criteria

- Telemetry reports load-start to synchronous return, first paint start/end,
  and a later message turn without using a fixed sleep as the result.
- It separates preview execution, graph setup, node tiles, Spy tiles, and
  cable paint using the authoritative existing metrics and identifies the
  slowest cold node tiles.
- Representative repeated loads provide measured intervals and a concrete
  recommendation for moving work to a worker.
- Focused semantic tests and a standalone agent run pass; no user preset
  files are changed.

Verification: `CycleV2_tests '[cycle-v2][canvas][performance]'` passed 262
assertions in 14 cases. Standalone Debug and Release `CycleV2` builds passed
with `--parallel 10`. The focused preset-load agent fixture passed in both
builds; two sidebar-row clicks and three larger Release preset opens passed.
`git diff --check` and the Cycle V2 architecture audit passed. The modified
preset files already present in the worktree were not used or staged.
