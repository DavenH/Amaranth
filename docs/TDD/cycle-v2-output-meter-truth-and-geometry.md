# Cycle V2 Output Meter Truth and Geometry

Status: Implemented

## Objective

Make the Output node's two meters an honest, compact summary of the rendered
audio. The preview must stop implying activity when no audio has been measured,
preserve independent left/right values when the signal is stereo, and spend the
available width on the meters rather than an unexplained central void.

The meter-only sections below describe the original completed slice. The later
Master Gain Extension supersedes their layout and interaction limits while
preserving their signal-truth contract.

## Current Failure

- `NodeDefinitionRegistry` marks Output as non-previewable, so the runtime
  preview processor is never used by the graph preview executor.
- `NodePreviewRenderer` paints hard-coded levels of 0.64 and 0.58 for every
  Output node, including disconnected and silent nodes.
- `MeterPreviewProcessor` also fabricates 0.65/0.62 when invoked without input.
- When given an upstream preview, the processor averages signed values. Phase
  cancellation can therefore report silence for a loud signal, and the right
  channel is invented as 95% of the left.
- The renderer removes 20% from each horizontal edge, then gives each bar only
  28% of the remaining width. At the natural Output-node size this strands a
  large central region with no semantic job.

## Authoritative Implementation and Boundary

`GraphAudioExecutor` is authoritative for the diagnostic audio payload. Its
Output-node result already copies the actual input `SignalPayload`, including
`block`, `secondaryBlock`, and `ChannelLayout`. `GraphPreviewExecutor` already
passes each previewable node's captured diagnostic output to
`PreviewProcessContext::capturedOutput`.

The change will expose Output as an `OutputMeters` qualitative preview and
reduce the captured payload to one absolute peak per channel. It will not add a
second audio execution path, inspect the realtime renderer, smooth or hold
levels, or reproduce audio processing in UI code. Mono input is represented by
the same measured peak on both output channels because the host output expands
mono equally; stereo input uses its independent secondary block.

## Presentation Contract

The shared meter layout owns these rules:

- use 14% horizontal and 8% vertical outer insets;
- keep a 4--8 px channel gap, scaled from preview width;
- divide all remaining horizontal space equally between left and right;
- keep the channel order left-to-right and the scale identical;
- at the Output node's natural preview width, the two bars together occupy at
  least 60% of the available width;
- clamp measured peaks to the visible 0--1 range;
- an absent, empty, or silent captured payload lights no segments;
- clip the segmented display at the exact continuous fill boundary so a low
  nonzero signal remains visible without rounding it up to a whole segment;
- keep the existing semantic low/warning/over colours and unlit segment state.

The original meter-only slice had no hover, drag, or value-entry interaction.
Its visual footprint, channel separation, and exact fill boundary were therefore
the applicable UI geometry contracts before the Master Gain Extension.

## Architecture and Deletion Targets

- Add a small UI presentation primitive for deterministic left/right bounds.
- Make the existing monitor preview processor consume the captured
  `SignalPayload` directly.
- Register Output with `PreviewModuleRole::OutputMeters`.
- Delete the fabricated static Output levels and the signed-summary averaging
  path.
- Keep `NodePreviewRenderer` responsible only for painting the supplied levels
  into the shared bounds.

Expected production change for the meter-only slice: one small presentation
pair, focused edits to the monitor processor, Output definition, and renderer.

## Verification

- Processor tests cover no payload, empty payload, mono peak, independent
  stereo peaks, negative samples, and visible-range clamping.
- A graph preview sequence test proves an Output node receives measured audio
  through the real compile/audio/preview path.
- Geometry tests cover natural, compact, and expanded bounds, equal bar widths,
  bounded channel gap, non-overlap, and at least 60% combined width at the
  natural size.
- A focused automation screenshot captures the production-size Output node
  before and after the change; the after image must show compact separation and
  levels derived from the demo graph rather than the old hard-coded pair.
- Run the focused Cycle V2 tests, `git diff --check`, and the standalone build.

## Completion Criteria

- Output preview levels originate only from the captured diagnostic audio.
- Silence and missing input never look active.
- Stereo channels remain independent and mono duplication is explicit.
- Meter geometry satisfies the measurable contract at supported preview sizes.
- The focused runtime, geometry, and end-to-end preview tests pass.
- Production-size visual evidence shows the central void removed without
  crowding the node title, ports, or surrounding canvas.

## Implementation Evidence

- `OutputMeterPresentation` owns the 14% outer inset, 4--8 px channel gap,
  equal channel widths, and continuous fill clipping.
- Output is registered as an `OutputMeters` preview and
  `MeterPreviewProcessor` reads only its captured diagnostic `SignalPayload`.
  Missing and empty payloads report zero; mono duplicates its measured peak;
  stereo preserves independent measured peaks; values above full scale clamp.
- The hard-coded 0.64/0.58 renderer fallback and 0.65/0.62 processor fallback
  were deleted.
- The focused processor, layout, low-level paint, registry, compiler, and
  compile/audio/preview sequence tests pass.
- `scripts/fixtures/cycle-v2-agent-output-meter.json` passes and reports the
  default graph's Output preview at a measured 0.01223 rather than a fabricated
  level. The production-size before/after captures are
  `/private/tmp/cycle-v2-agent-canvas.png` from the baseline run and
  `/private/tmp/cycle-v2-agent-output-meter.png` from the final run.
- At completion of the initial meter-only slice, its 526 focused cases passed;
  the broader run still contained the then-pre-existing
  `TestNodeCanvasHitRouter.cpp:66` failure tracked in `ui-bugs.md`. The later
  Master Gain Extension evidence below supersedes that historical test status.

## Master Gain Extension

### Objective

Add Cycle 1's master-volume contract to the Output node without conflating it
with Cycle V2's fixed post-mix safety headroom. The Output preview becomes a
compact mastering strip: the stereo meters remain the dominant readout and a
vertical fader beside them controls the persisted master gain.

### Authoritative DSP Contract

Cycle 1's `OscControlPanel::scaleVolume()` is authoritative. It maps normalized
slider position `x` to linear gain `exp(6x - 3)`, so `0.5` is unity. Its
`SynthAudioSource` applies a 128-sample half-life smoothed ramp after all effects.
The mapping belongs in shared `CycleDsp` parameter mapping and is consumed by
both Cycle 1 and Cycle V2; it must not be reimplemented in the Output UI or
processor.

Cycle V2 translates the boundary by persisting normalized `gain` on the Output
node and compiling it into mapped linear gain. Diagnostic and preview execution
uses the typed Output processor so its captured payload and traversal grids
reflect the gain. Realtime graph execution deliberately exposes the signal
feeding the sink, so `RealtimeGraphRenderer` owns the authoritative audible
application after voice summation. The existing
`RealtimeGraphRenderer::outputHeadroom` remains a separate fixed multiplication
at that same final stage. It is not folded into the node parameter, display
value, converter, or parity manifest.

The Cycle 1 preset converter transfers oscillator knob 0 unchanged into the
Output node. Older Cycle V2 graphs omit the parameter and therefore normalize to
the definition's `0.5` unity default.

### Presentation and Interaction Contract

- Double the Output node's natural height from 160 px to 320 px.
- Reserve at least 56% of the natural preview width for the two meters and no
  more than 24% for the fader hit column. Centre the fader exactly in the
  component, place one meter on each side, and keep equal visible group gaps.
- Draw a thin vertical track, a small horizontal thumb with an exact centre
  line, and a concise decibel readout. The hit column remains at least 24 px at
  normal zoom even though the visible track is narrower.
- Map upward motion to increasing normalized gain. Ordinary drag uses at least
  120 px for the full range; Shift provides 4x finer adjustment. Double-click
  resets to the `0.5` unity position.
- Hover uses the vertical-resize cursor and reports the mapped decibel value.
- Up/Down adjusts a selected Output node in 0.02 normalized steps; Shift uses
  0.005 steps.
- A drag is one dispatcher-owned transient edit. Multiple updates retain the
  same durable base, commit once on mouse-up, refresh audio/preview, and undo as
  one action. UI code never mutates `NodeGraph` directly.

### Architecture and Deletion Targets

- Extend `OutputMeterPresentation` as the sole owner of meter/fader layout,
  value geometry, and painting.
- Replace the diagnostic Output passthrough processor with a narrowly configured
  gain processor that reuses `SmoothedParameter` and vector buffer operations.
- Carry mapped gain in `GraphExecutionPlan`; smooth and apply it once globally in
  `RealtimeGraphRenderer`, after voice summation and before clipping.
- Reuse `GraphCommandDispatcher` transient editing through
  `NodeCanvasAuthoring`; do not create an Output-specific graph mutation path.
- Delete the converter manifest language that calls master gain an unresolved
  constant-gain discrepancy once the normalized control is emitted.

### Verification

- Shared mapping tests cover endpoints and unity.
- Processor tests cover mono/stereo gain, neutral default, configuration
  replacement without discontinuity, and traversal-grid agreement.
- A compiled graph test proves Output gain changes observable output while the
  renderer's fixed headroom remains unchanged.
- Geometry tests cover compact/natural/expanded bounds, thumb endpoints,
  hit-target size, centred placement, meter allocation, and position mapping.
- A gesture sequence test performs at least two updates, commits, observes the
  persisted value and downstream parameter impact, then undoes.
- Converter tests prove a Cycle 1 volume knob is preserved on Output and the
  equivalence manifest no longer declares that control missing.
- A production-size Cycle V2 screenshot validates the final meter/fader balance.

### Completion Criteria

- Output gain is persisted, compiled, smoothed, audible, preview-visible, and
  exactly mapped from the Cycle 1 normalized control.
- The safety headroom remains independently applied and documented.
- Pointer, reset, fine-drag, keyboard, commit, and undo behavior follow the
  semantic command path.
- The meters remain readable and spatially dominant at the natural Output-node
  size.
- The Output node is 2x its original natural height and the fader is centred
  between the two meters.
- Focused tests, the full Cycle V2 test suite, converter tests, standalone build,
  style checks, and production-size visual review pass.

### Implementation Evidence

- `Output` owns a persisted normalized `gain` with a `0.5` unity default. Shared
  `CycleDsp` mapping is now the one source for Cycle 1 and Cycle V2's
  `exp(6x - 3)` gain law and decibel presentation.
- Diagnostic Output processing scales captured blocks and traversal grids;
  `GraphExecutionPlan` carries the same mapped gain to the realtime renderer,
  which smooths it after voice summation and keeps `outputHeadroom` independent.
- `OutputMeterPresentation` now owns the stereo-meter/fader geometry and paint.
  The 190x320 natural Output node is twice its original height, with equal
  meters flanking a fader aligned exactly to the component centre. The canvas
  routes drag, Shift-fine drag, double-click reset, wheel, and keyboard edits
  through semantic commands. Fader drag uses one transient edit, accepts
  multiple updates, commits once, and undoes once.
- The Cycle 1 converter emits oscillator knob 0 as Output gain and records that
  normalized value separately from Cycle V2's fixed headroom. Canonical checked
  graphs explicitly store the unity default; older graphs normalize missing
  gain to unity.
- The focused production automation fixture exercises semantic fader targeting,
  ordinary and fine two-update drags, commit/undo, reset, keyboard adjustment,
  hover/cursor behavior, and screenshot capture. The final 1728x962 capture is
  `/private/tmp/cycle-v2-agent-output-meter.png`.
- All 899 CTest cases pass, as do all 31 converter tests. Cycle 1 and Cycle V2
  standalone targets build successfully with `--parallel 10`; `git diff
  --check` passes. No new scalar standard-library math appears in a DSP hot
  loop, and `clang-tidy` is unavailable on this machine.
