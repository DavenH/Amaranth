# Cycle V2 graph grid performance

## Status

In progress (2026-10-07). Expanded Spy rendering remains too slow for
continuous feedback. The measurements below distinguish it from compact
preview updates.

## Expanded Spy: comparable grid measurement

### Selected-probe capture design

`GraphCompiler` owns compiled probe addresses and the dependency index.
`GraphAudioExecutor` owns diagnostic processing and captured audio results;
`GraphPreviewExecutor` owns conversion of a captured traversal grid into a
probe preview. The expanded Spy request should identify one compiled probe
address. Execution should use its upstream dependency closure and capture only
its source output, then extract only that probe preview. Compact graph
previews retain the full diagnostic path. The default-output resolver already
places its tap before the wet delay/reverb suffix; an explicit Spy after an
effect must still include that effect. No node-kind check is needed in the
executor. The existing full-capture call and redundant expanded preview
rendering were deletion targets for this path. The dependency closure is now
owned by `ProbeExecutionScope`; the executor still owns preparation and
processing decisions, and the preview executor reuses the existing probe-grid
conversion. Exact grid parity and skipped downstream work are tested. Compact
preview calls retain their existing complete execution and extraction policy.

The three-open Baroque Flute fixture at note 48 measured 329.32 ms before
selection and 161.49 ms in the final build for the same displayed 512 by 512
grid. Execution fell from 238.45 to 152.73 ms; extraction fell from 23.46 to
0.77 ms. The selected capture produces one node result and does not prepare
or process `reverb`. A test with a Spy after reverb confirms that it still
processes that effect and matches the full-capture grid. Artifact:
`/private/tmp/cycle-v2-spy-selected-final-report.json`.

`GraphAudioExecutor.cpp` was 1,168 lines before this slice and is 1,217
afterward. Its added code conditions its existing preparation, execution, and
diagnostic capture loops; the dependency-closure algorithm lives in the new
40-line `ProbeExecutionScope.cpp`. `GraphPreviewExecutor.cpp` grew from 600
to 628 lines to share the existing probe conversion with the selected path.
The result still takes about 160 ms, so this TDD remains in progress. The
remaining preparation, upstream processing, and capture costs need separate
measurement before another reduction. Reusing prepared processors requires a
reset contract and parity proof because the earlier reuse experiment changed
grid values.

The expanded Spy is opened through `NodeCanvas::openProbeDetail`, which calls
`PresentationPreviewRenderer::captureProbePreview`. It creates a 512-column
diagnostic grid. Before selected-probe capture, it created a fresh
`GraphAudioExecutor`, executed the entire graph, rendered all node/probe
previews, and then selected one Spy result. These costs were absent from the
compact `previewAudio` telemetry reported earlier. That Baroque Flute capture
visited the compiled 20-step graph even though the selected Spy taps
`volumeMultiply` upstream of the output effects.

Temporary timing in Cycle 1's existing `VisualDsp::GraphicProcessor` measured
the BaroqueFlute expanded visual grid at 435 columns and 512 time rows. Two
settled preset-load grid updates took about 33 ms each across the time,
envelope, FFT, and effects stages. One morph-edit grid update took about 51 ms.
Cycle 2's corresponding Baroque Flute expanded Spy at 512 columns and 512
displayed rows took 334.63 ms mean over three opens with a fresh executor:
242.24 ms graph execution, 23.67 ms preview extraction, and about 69 ms other
capture work. The shapes and preset are close; the triggered work differs, so
these figures establish the observed gap without claiming identical DSP.
The final fixture with permanent telemetry measured 329.32 ms total, 238.45 ms
execution, and 23.46 ms extraction over three opens. `expandedProbeTotal`
measures synchronous capture through row reduction; it does not measure the
subsequent JUCE paint. The fixture's paint counter recorded no paint frame
before inspection, so it cannot establish a displayed-frame latency.

Internal temporary timing split Cycle 2's 512-row graph execution into about
73 ms processor/workspace preparation and 169 ms execution and result capture
per open. An experiment retaining a dedicated executor reduced preparation on
the next two opens to under 0.1 ms and graph execution to 128-134 ms. A
semantic test found different grid values on repeated capture, however:
processors retain state, and no general per-capture reset contract exists.
The experiment was removed. Retaining preparation requires an explicit reset
boundary and numerical parity proof, not an executor cache alone.

Follow-up timing explains why similar node counts do not imply similar
latency. Cycle 1's 51.8 ms morph update comprised about 12.1 ms time, 0.8 ms
envelope, 37.1 ms spectral, and 1.8 ms visual-effects work over 435 columns.
At 512 columns and 512 rows, temporary per-step Cycle 2 timing measured about
91 ms inside node processors and oscillator rendering, plus about 72 ms of
per-node diagnostic result and traversal-grid capture. The approximately
73 ms preparation above precedes those steps. Extraction added about 24 ms.
Destruction after extraction added about 67 ms: roughly 34 ms to release the
full `GraphAudioResult` and 33 ms to release the fresh executor and its
diagnostic cache. These are rounded means from separate three-open Debug runs,
so their sums are an explanation of scale rather than one exact trace.

The Spy taps `volumeMultiply` before the wet effects. Nonetheless the full
Cycle 2 diagnostic traversal spent about 28 ms processing and capturing
`reverb`, and another 16 ms capturing or processing `globalInput` and
`output`. Cycle 1's
visual-effects stage processes its enabled waveshaper, tube model, EQ, and
unison; it does not run the wet delay or reverb processors. Cycle 2 also
materializes result grids for each upstream node and retains two large
diagnostic result sets while selecting this one Spy. The 512 versus 435 column
count and differing spectral row counts increase DSP work, but most of the
observed gap is preparation, broad diagnostic capture, and cleanup.

At a lower preview note, the expanded Spy displayed 512 by 512 values but
processed 512 by 1,024 values before reducing rows. Three opens averaged
606.88 ms total, including 417.66 ms execution and 44.46 ms extraction.
This is a concrete source of the reported roughly 500 ms delay. The 512-row
cap is a candidate for earlier application if signal and display semantics
can be preserved; the current post-processing reduction computes twice as
many rows as the detail displays in this case.

Expanded Spy artifacts: `/private/tmp/cycle-v2-expanded-spy-final-report.json`
(final fresh-executor fixture),
`/private/tmp/cycle-v2-expanded-spy-reuse-report.json` (discarded reuse experiment),
`/private/tmp/cycle-v2-expanded-spy-512-report.json` (1,024 source rows),
`/private/tmp/cycle1-expanded-grid-edits-log.txt.raw` (Cycle 1 timing), and
`/private/tmp/cycle1-expanded-grid-edits-v2-log.txt.raw` (Cycle 1 dimensions).
Follow-up Cycle 2 breakdowns are in
`/private/tmp/cycle-v2-spy-process-log.txt.raw` (per-step processing and capture)
and `/private/tmp/cycle-v2-spy-release-log.txt.raw` (result and executor release).
The temporary timing code was removed from both products.

## Measured behavior

The compact Cycle V2 graph preview path is separate. Its
`GraphPresentationModel` refreshes DSP configurations, then
`PresentationPreviewRenderer` asks `GraphAudioExecutor` to
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

Full compact-preview preset loads also incur substantial work. With telemetry
reset before opening, Baroque Flute took 260.41 ms synchronous refresh,
including 132.14 ms preview audio and 15.39 ms extraction. Organ 4 took
462.59 ms synchronous refresh, including 339.28 ms preview audio and
17.57 ms extraction. The remainder includes compilation and graph setup;
those phases are not separately timed yet. Artifacts:
`/private/tmp/cycle-grid-load-report.json` and
`/private/tmp/cycle-grid-organ-load-report.json`.

## Architecture and next measurements

Cycle 1's `VisualDsp` owns staged time, envelope, FFT, and effects columns,
using `TimeColumnRasterizer` for the time stage. Cycle V2's full diagnostic
executor materializes traversal grids at graph nodes and retains full
`SignalPayload` copies in `nodeOutputs`, `NodeAudioResult::output`, probe
grids, and the diagnostic cache. The selected-probe path now retains only its
source node result; temporary per-node timing measured the earlier complete
path's processing and capture costs. Direct copy counts for the remaining
selected path have not been measured.

Temporary stage timing in an otherwise unchanged Cycle 1 standalone Debug
build measured two settled BaroqueFlute preset-load grid updates at 435 columns:
time 0.04-0.06 ms, envelope 0.03 ms, FFT 28.27-28.93 ms, and effects
4.57-4.69 ms. The sum is approximately 33 ms per staged product. The trace is
`/private/tmp/cycle1-grid-log.txt.raw`; the temporary timing code was removed.
This and the Cycle V2 source-edit measurements are different trigger paths and
product shapes, so 33 versus 86 ms is an indication, not a controlled speed
ratio. A Cycle 1 Organ4 load crashed while collecting the second comparison;
it is recorded in `docs/TDD/ui-bugs.md`.

Next, split the selected path's remaining ~160 ms into preparation, upstream
node processing, selected-result copying, and cleanup. Count raster bakes,
FFTs, and copied grid values; then vary unrelated graph size while holding
the selected Spy fixed. Preparation still uses the full plan's workspace
shape. Preserve the mature rasterizers and FFT implementations. Reuse across
opens needs a processor reset contract and exact grid parity before it can
replace fresh capture.

## Completion criteria

- Equivalent presets have comparable product dimensions and measured stage
  durations in both versions.
- The remaining Cycle V2 hot stage has operation-count and duration evidence.
- A representative expanded Spy capture reaches a continuous-feedback budget
  without losing probe content.
