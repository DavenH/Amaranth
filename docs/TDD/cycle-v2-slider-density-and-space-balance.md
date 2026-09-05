# Cycle V2 Slider Density and Space Balance

Status: Implemented 2026-09-05

## Objective

Adopt the minimal morph-slider value indication across ordinary property
sliders, tighten the relationship between compact labels and their rails, and
rebalance the expanded Waveshaper editor so its square transfer view and
property group use the available space deliberately.

## Authoritative Implementations

- `PropertyControlLookAndFeel` owns shared property-slider track, fill, exact
  value indication, focus, hover, and disabled presentation.
- `propertySliderLayout` and `PropertyControlMetrics` own compact label, value,
  slider, and track geometry for IR, Waveshaper, Delay, Reverb, and Equalizer.
- `WaveshaperEditorComponent` owns its domain layout. `NodeViewModuleRegistry`
  owns the preferred expanded-editor size.
- Existing curve editor controls and `GraphCommandDispatcher` publication remain
  authoritative for gestures, values, DSP updates, and undo. This slice changes
  presentation geometry only.

## Design Contract

- Ordinary property sliders use the same bare exact hairline marker as morph
  sliders, without the hollow capsule that visually obscures the value. Morph
  sliders retain their semantic axis colour.
- In a compact property row, the vertical gap from the label box to the visible
  slider track is at most 10 px. The label/value line and the slider hit target
  may overlap geometrically only where their visible content and pointer roles
  remain distinct.
- At the preferred Waveshaper size, the transfer view remains exactly square and
  grows by approximately 20% from the current 318 px production result.
- The complete Waveshaper property group is vertically centred in its available
  control column. Unassigned space above and below differs by no more than 2 px.
- Small-window fallback keeps the plot and controls contained; it may reduce the
  plot below the preferred size but may not distort its aspect ratio.

## Space-Audit Rule

Every panel review compares the occupied content bounds with its available
region on both axes. A large residual area is acceptable only when it has a
stated purpose. If repeated controls are pinned to one edge while an unexplained
void remains on the opposite edge, flag the imbalance and propose one or more
of: enlarge the information-bearing view, centre the control group, redistribute
peer groups, or reduce the containing panel. Do not call mathematically valid
bounds balanced without this residual-space check.

## Test-First Contract

1. Shared geometry tests require the compact label-to-track gap to be at most
   10 px and the ordinary indicator to be a centred hairline.
2. Waveshaper hosted-editor tests require a square plot of at least 380 px at
   preferred size and a control group centred within its rail.
3. View-module tests require the new preferred editor size explicitly.
4. Existing complete slider gestures, semantic entry, focus, reset, publication,
   and undo tests continue to pass.
5. Production-size IR and Waveshaper screenshots verify density, alignment,
   focus, endpoints, and disabled presentation.

## Negative Boundaries

- Do not change parameter ranges, mappings, readout precision, DSP behavior,
  publication timing, or undo ownership.
- Do not reduce slider hit targets to the painted hairline.
- Do not distort the Waveshaper plot or enlarge its property controls merely to
  fill space.
- Do not add node-specific layout policy to shared property controls.

## Completion Criteria

- Geometry, hosted interaction, automation, and production screenshot criteria
  pass for both editors.
- Standalone Debug builds; `git diff --check`, style review, hot-loop review,
  and production-diff review are complete.
- The corresponding `ui-bugs.md` entry and this TDD are marked implemented only
  after the complete visual review.

## Implementation Evidence

- Shared property sliders and morph sliders now use the same 1.5 x 17 px exact
  hairline indicator. The painted shape is independent of the full slider hit
  target.
- Compact property rows are 50 px high, with an 18 px heading and a visible
  label-to-track gap no greater than 10 px. The IR production capture confirms
  the tighter label/rail relationship without crowding the readout.
- The preferred Waveshaper editor is 766 x 464 px. Its square transfer view is
  approximately 382 px, up from approximately 318 px, and its complete 152 px
  property group is centred in the control rail. The later centre-gutter pass
  removed stranded width without changing those content dimensions.
- Focused property-control, hosted-editor, and view-module tests pass. The
  Waveshaper and IR automation fixtures pass with no failed commands:
  `/private/tmp/cycle-v2-waveshaper-layout-os-report.json` and
  `/private/tmp/cycle-v2-ir-density-os-report.json`.
- Production captures were reviewed at
  `/private/tmp/cycle-v2-waveshaper-layout-os.png` and
  `/private/tmp/cycle-v2-ir-density-os.png`.
- Standalone Debug builds successfully with `--parallel 10`;
  `git diff --check`, production-diff review, style review, and hot-loop review
  pass. The complete Cycle V2 suite remains at 555 of 556 passing due to the
  unrelated, existing hover-help failure at
  `TestNodeCanvasHitRouter.cpp:66`.
