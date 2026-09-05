# Cycle V2 Waveshaper Centre Gutter

Status: Implemented (2026-09-05)

## Objective

Remove the unexplained extra space between the Waveshaper transfer plot and
its property controls. The centre gutter should match the IR modeller's single
24-pixel visual spacing unit while preserving the Waveshaper's square plot and
fixed-width control rail.

## Authoritative Layout

- `CurveExpandedEditorComponent::contentBounds` owns the shared outer inset.
- `WaveshaperEditorComponent` owns the 382-pixel production plot and 336-pixel
  property rail.
- `NodeViewModuleRegistry` owns the expanded editor's preferred size and
  viewport clamping.
- The IR modeller's 24-pixel plot-to-control-content gap is the established
  reference for adjacent expanded-editor regions.

At 824 by 464 pixels, the fixed plot and rail leave 52 pixels with no purpose
in the centre, producing a 76-pixel visual gap. Matching the IR panel's 12-pixel
horizontal inset and reducing the preferred width to 766 removes that stranded
space while preserving the plot size, rail size, and control geometry.

## Test-First Contract

1. The preferred Waveshaper editor size is 766 by 464 pixels.
2. At preferred size, the square transfer plot remains at least 380 pixels.
3. The horizontal distance from the plot's right edge to the property group's
   left edge is exactly 24 pixels, matching the IR reference.
4. The property rail remains 336 pixels wide and its controls retain their
   current usable track width.
5. Smaller viewports continue to use the shared clamping behavior.

## Negative Boundaries

- Do not distort or shrink the transfer plot.
- Do not widen, shift within, or rescale the property controls.
- Do not add compensating node-specific offsets to either child region.
- Do not alter curve interaction, parameter mappings, or OpenGL rendering.

## Completion Criteria

- Registry and hosted-editor geometry regressions pass.
- A production-size Waveshaper capture shows one calm centre gutter with no
  new imbalance at the outer edges.
- Standalone Debug and relevant tests build; `git diff --check`, style review,
  and production-diff review pass before commit.

## Implementation Evidence

- The preferred editor is now 766 by 464 pixels, with a 382-pixel square plot
  and unchanged 336-pixel control rail.
- The Waveshaper panel now shares the IR panel's 12-pixel horizontal inset.
  Automation reports the plot ending at x=406 and the control group beginning
  at x=430, giving the required 24-pixel visual gutter.
- Focused registry and hosted-editor tests pass. The production fixture passed
  in `/private/tmp/cycle-v2-waveshaper-gutter-report.json`; its reviewed capture
  is `/private/tmp/cycle-v2-waveshaper-gutter.png`.
