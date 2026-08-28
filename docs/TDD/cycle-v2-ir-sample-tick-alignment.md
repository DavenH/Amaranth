# Cycle V2 IR Sample Tick Alignment

Status: Implemented (2026-08-28)

## Objective

Align the IR modeller's sample ticks with the plotted impulse domain. Establish
one authoritative IR domain-padding constant so UI axes, preview framing,
default models, imported models, and DSP rasterization cannot silently drift.

## Current Failure

The IR curve panel reserves the leftmost 6.25 percent of its width as a
pre-impulse domain. Its boundary and grid therefore begin at normalized x
`0.0625`. The sample ticks currently distribute 0 through the impulse length
across normalized x `0` through `1`, causing every tick except the final one to
sit left of its corresponding plotted sample position. The first tick visibly
ignores the panel's left padding. Edge-label rectangles are also left
unclamped: the first label remains at the outer panel edge after moving its
tick, and the final label can extend beyond the panel.

The padding literal is independently repeated by panel rendering, default-model
construction, panel adaptation, imported-audio modelling, DSP configuration,
and compact preview framing. That duplication is the source of truth failure.

The current visual reference is `/private/tmp/cycle-v2-ir-hertz-readout.png`.

## Authoritative Implementation And Boundaries

- `CycleDsp::rasterizeIrImpulse` is the mature curve-to-impulse implementation.
  Its caller-supplied padding defines where sample zero begins in curve domain.
- `CycleDsp::IrModel` becomes the stable owner of that shared domain padding and
  the normalized sample-fraction mapping.
- Existing curve panels remain authoritative for OpenGL drawing and
  interaction. The editor only maps labels and tick marks to their domain.
- Sample fraction `0` maps to domain x `0.0625`; fraction `1` maps to x `1`.
  Intermediate samples interpolate linearly over the remaining 93.75 percent.

## Implementation Slice

1. Add a domain contract test for endpoints and quarter positions.
2. Add an editor automation assertion proving the first and last sample ticks
   align with the padded plot bounds.
3. Add the padding and mapping to `CycleDsp::IrModel`.
4. Replace production copies in panel rendering, model construction, resource
   preparation, DSP configuration, compact preview framing, and panel adapter.
5. Centre labels on interior ticks and clamp edge labels inside the panel, then
   compare a native IR editor capture before and after.

## Negative Boundaries

- Do not change the curve, impulse samples, interpolation, rasterizer,
  oversampling, convolution, model topology, or audio-resource workflow.
- Do not compensate with a pixel offset or infer padding from current bounds.
- Do not change vertical geometry, tick count, labels, sample values, control
  layout, plot size, or interaction hit bounds.
- Do not introduce a UI-owned copy of DSP domain geometry.

## Verification

- Domain tests prove exact endpoint and quarter-position mapping.
- Editor automation proves tick zero equals the padded plot boundary, its label
  is centred there, and the final tick and label end at the panel right edge.
- Existing IR model, resource, processor, editor, and curve tests remain green.
- The IR resource fixture and native screenshot show visible alignment.
- Standalone Debug, the full suite, `git diff --check`, hot-loop review, style
  review, and production-diff review pass before commit.

## Deletion Targets

- Production `0.0625f` and local `kIrPadding` copies representing IR domain
  padding.
- Tick placement over the full unpadded panel width.

## Completion Criteria

- Sample ticks and plot boundaries consume one shared mapping.
- All production IR domain-padding copies are removed.
- Audio and curve behavior remain unchanged.
- Focused tests, automation, screenshot review, standalone build, full suite,
  and style checks complete with no new regression.

## Implementation Evidence

- `CycleDsp::IrModel` now owns `irDomainPadding` and the sample-fraction mapping.
  Sample zero maps to `0.0625`, quarter positions map over the remaining domain,
  and the final sample maps to `1`.
- Panel drawing, default-model construction, panel adaptation, imported-audio
  modelling, DSP configuration, and compact preview framing consume that owner.
  No production `kIrPadding` or independent `0.0625f` copy remains.
- The expanded editor maps tick marks with the shared domain function. Interior
  labels are centred on their ticks; edge labels are clamped inside the panel
  and aligned to the corresponding boundary.
- The DSP domain contract passes 354 assertions across six IR cases. The IR
  editor contract passes 66 assertions, including exact first/last tick and
  label geometry at 900 by 430 pixels. Effect paths pass 114 assertions across
  14 cases.
- The updated IR fixture passes all six commands and now reports canvas bounds
  for reliable OS capture. At panel x 24 with width 504, sample zero resolves to
  x 55.5 and the last sample to x 528. The reviewed native capture is
  `/private/tmp/cycle-v2-ir-ticks-aligned.png`.
- Standalone Debug builds successfully with `--parallel 10`. The complete Cycle
  V2 executable runs 538 cases: 537 pass, and the sole failure remains the
  pre-existing `TestNodeCanvasHitRouter.cpp:66` hover-help assertion.
- The complete library run reports two unrelated, suite-order-dependent
  rasterizer `curveRes` failures. Both cases pass independently with 262 and
  1252 assertions and are recorded in `audio-bugs.md`; no rasterizer algorithm
  changed in this slice.
- `git diff --check`, line-length review, include review, hot-loop review, and
  production-diff review pass. `clang-tidy` is unavailable. Existing scalar
  finiteness checks in touched files remain outside per-sample hot loops.
