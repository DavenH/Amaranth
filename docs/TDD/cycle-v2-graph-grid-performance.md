# Cycle V2 graph grid performance

## Status

In progress (2026-10-07). Duplicate diagnostic preparation has been removed;
the remaining preview computation is still too slow for continuous feedback.

## Measured behavior

Cycle V2's `GraphPresentationModel` is the update owner. It refreshes DSP
configurations, then `PresentationPreviewRenderer` asks `GraphAudioExecutor` to
execute a 256-column diagnostic traversal grid and `GraphPreviewExecutor` to
extract node and probe previews. `GraphAudioExecutor::processInternal` already
prepares the diagnostic executor for the requested frame and column shape.

Five `timeLayer1.red` edits on Baroque Flute in the macOS standalone Debug
build, measured by `resetCanvasPerformance` / `inspectCanvasPerformance`:

| Stage, mean ms | Before | After duplicate-preparation removal |
| --- | ---: | ---: |
| Synchronous refresh | 138.53 | 85.96 |
| Configuration | 25.03 | 28.12 |
| Extra preview preparation | 36.57 | 0 |
| Preview audio, including executor preparation | 64.08 | 44.29 |
| Preview extraction | 6.38 | 6.72 |

The extra preparation used zero requested traversal columns, then processing
prepared the same processors again for 256 columns. The two different
preparation signatures prevented reuse. Removing the extra call preserved all
five successful graph edits and reduced measured refresh time by 38%.

Five equivalent `timeLayer1.red` edits on Organ 4 after the change averaged
119.12 ms refresh, including 5.27 ms configuration, 89.68 ms preview audio,
and 16.01 ms extraction. This confirms that diagnostic graph execution is now
the dominant remaining cost in a larger graph. Raw telemetry:
`/private/tmp/cycle-grid-telemetry-detailed-report.json`,
`/private/tmp/cycle-grid-telemetry-after-report.json`, and
`/private/tmp/cycle-grid-organ-report.json`.

On Baroque Flute, five midgraph `magnitudeLayer1.red` edits averaged 65.01 ms
refresh, with 27.50 ms preview audio. Five downstream `reverb.wet` edits
averaged 26.89 ms refresh and did not run preview audio in this workflow.
The difference tracks the affected downstream graph path. A state snapshot
after the Baroque Flute source edit reported 21 graph nodes, 10 preview nodes,
and 1,181,096 preview sample values (4.51 MiB of float data); most preview
grids were 256 by 512 or 256 by 257. Artifacts:
`/private/tmp/cycle-grid-magnitude-report.json`,
`/private/tmp/cycle-grid-reverb-report.json`, and
`/private/tmp/cycle-grid-shape-report.json`.

## Architecture and next measurements

Cycle 1's `VisualDsp` owns staged time, envelope, FFT, and effects columns,
using `TimeColumnRasterizer` for the time stage. Cycle V2's diagnostic executor
materializes traversal grids at graph nodes and retains full `SignalPayload`
copies in `nodeOutputs`, `NodeAudioResult::output`, probe grids, and the
diagnostic cache. The latter is a likely copy cost, but no per-node timing or
copy-count telemetry has isolated its share yet. Cycle 1 has no equivalent
stage-timing telemetry, so a direct measured speed ratio is still open.

Next, measure executor preparation, node processing by domain, and grid value
copies separately on equivalent Cycle 1 and Cycle V2 presets. Count columns,
rows, raster bakes, FFTs, and copied grid values, then vary unrelated graph
size while holding the edited delta fixed. Preserve the mature rasterizers and
FFT implementations. Any optimization must keep complete diagnostic results,
probe previews, cancellation, and incremental cache semantics unchanged.

## Completion criteria

- Equivalent presets have comparable product dimensions and measured stage
  durations in both versions.
- The remaining Cycle V2 hot stage has operation-count and duration evidence.
- A representative graphwise update reaches a continuous-feedback budget
  without losing probe or preview content.
