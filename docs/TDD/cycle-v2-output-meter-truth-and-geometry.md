# Cycle V2 Output Meter Truth and Geometry

Status: Implemented

## Objective

Make the Output node's two meters an honest, compact summary of the rendered
audio. The preview must stop implying activity when no audio has been measured,
preserve independent left/right values when the signal is stereo, and spend the
available width on the meters rather than an unexplained central void.

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

The meter has no hover, drag, or value-entry interaction. Its visual footprint,
channel separation, and exact fill boundary are therefore the applicable UI
geometry contracts; no invisible hit target is introduced.

## Architecture and Deletion Targets

- Add a small UI presentation primitive for deterministic left/right bounds.
- Make the existing monitor preview processor consume the captured
  `SignalPayload` directly.
- Register Output with `PreviewModuleRole::OutputMeters`.
- Delete the fabricated static Output levels and the signed-summary averaging
  path.
- Keep `NodePreviewRenderer` responsible only for painting the supplied levels
  into the shared bounds.

Expected production change: one small presentation pair, focused edits to the
monitor processor, Output definition, and renderer. New node-kind switching or
changes to graph execution are out of scope.

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
- `CycleV2_tests` reports 526 passing cases and the one pre-existing
  `TestNodeCanvasHitRouter.cpp:66` failure already tracked in `ui-bugs.md`.
  The standalone CycleV2 target builds successfully. `git diff --check` passes;
  `clang-tidy` was unavailable on this machine.
