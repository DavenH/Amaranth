# Cycle V2 IR Attack Zoom

Status: Implemented 2026-09-05

## Objective

Give the expanded IR editor one-step controls for framing the padded impulse
onset and restoring the complete response, using legible semantic SVG icons and
without treating view state as a graph edit.

## Authoritative Implementation

Cycle v1 `IrModellerUI::doZoomAction` defines the product behavior: attack zoom
starts at `IrModellerPadding` and reduces the current horizontal span to 20
percent, while full zoom restores the complete IR domain. The mature
`ZoomPanel` remains authoritative for zoom constraints, background/grid refresh,
and repaint notification.

Cycle V2 `ImpulseResponseCurvePanel` already owns the mature hosted panel and
its `ZoomPanel`. The expanded editor owns only the visible actions. A narrow
IR-specific controller interface translates button activation into those panel
operations; it does not expose `ZoomPanel` or reproduce zoom arithmetic in UI
orchestration code.

## Test-First Contract

1. The expanded IR editor exposes keyboard-focusable attack and full-view
   buttons with action-oriented titles, descriptions, and tooltips.
2. Attack zoom sets the horizontal origin to `CycleDsp::irDomainPadding` and
   reduces the current span to 20 percent, while leaving the vertical range
   unchanged.
3. Full view restores the exact declared horizontal and vertical limits.
4. Repeated view actions emit no graph transaction or model publication.
5. Automation reports both action bounds and the resulting panel zoom so a
   production fixture can verify the complete click sequence.
6. The sample ruler omits off-screen grid landmarks while zoomed; it must not
   clamp hidden labels into a stack or paint stray ticks beyond the plot.

## Layout And Icon Contract

- Float the actions as a compact cluster inside the graph panel's upper-right
  corner; do not create a second toolbar above the plot or reduce the OpenGL
  plot or slider rail.
- Use 24 px square controls on the shared spacing rhythm, with attack first and
  full view second.
- Store both outline SVGs under the shared `ui-icons` resource set. Their
  waveform and framing marks must remain recognizable at the production size.
- Use the shared control radius, border, focus ring, and interaction colours.

## Negative Boundaries

- Do not mutate or publish the graph, create undo entries, or change IR DSP.
- Do not let the editor reach into `ZoomPanel` or duplicate domain padding.
- Do not paint the IR curve, spectrum, or filter backdrop through JUCE.
- Do not copy Cycle v1 raster sprite cells.

## Completion Criteria

- Focused editor-host tests and a click-sequence automation fixture pass.
- Attack and reset states receive production-size OpenGL screenshot review.
- Standalone Debug builds; the Cycle V2 suite, `git diff --check`, style review,
  hot-loop review, and production-diff review are complete.

## Implementation Evidence

- `ImpulseResponseCurvePanelContract` owns the two view operations. The attack
  path reuses Cycle v1's padded origin and 20-percent span through the mature
  `ZoomPanel`; reset restores the panel's declared bounds. The widget and editor
  only route these operations, and tests observe repaint without graph begin,
  publish, or commit events.
- `zoomAttack.svg` and `zoomFull.svg` are generated through the shared
  `ui-icons` resource set. Their keyboard-focusable 24 px buttons float as a
  horizontal cluster inside the plot's upper-right corner, following the
  production-review correction that removed the awkward strip above the plot.
- Zoomed IR ruler generation now drops off-screen grid landmarks instead of
  clamping their labels together. The 256-sample attack fixture reports only
  the visible `0` and `32` landmarks.
- The focused editor-host test passes 30 assertions. The production
  attack/full/attack fixture passes every command, and the 3456 x 1924 capture
  confirms the attack framing, in-plot icon placement, and clean ruler. Evidence
  is in `/private/tmp/cycle-v2-ir-attack-zoom-report.json` and
  `/private/tmp/cycle-v2-ir-attack-zoom-final.png`.
- Standalone Debug builds. The full Cycle V2 suite passes 11,151 of 11,152
  assertions across 574 test cases; the sole failure remains the recorded
  edge-hover help assertion in `TestNodeCanvasHitRouter.cpp:66`.
