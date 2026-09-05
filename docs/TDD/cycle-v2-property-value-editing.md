# Cycle V2 Property Value Editing

Status: Implemented 2026-09-05

## Objective

Make shared numeric property readouts visually stable and semantically precise:
no resting input box, no alignment jump on edit, and no unit text inside the
editable selection.

## Authoritative Implementation

`PropertySliderRow` is the shared value/readout owner used by Cycle V2 effect,
Voice Context, EQ, and curve editors. Its existing formatter and parser remain
authoritative for display precision, accepted suffixes, range validation, and
the exact committed slider value.

JUCE `Label` owns the mature click-to-edit, focus, return, escape, and
accessibility lifecycle. This change configures its live `TextEditor`; it does
not replace that interaction.

## Design

Split every formatted readout into numeric text and a trailing unit at the
shared row boundary. Keep the numeric `Label` editable and add a sibling,
non-editable unit label within the existing readout allocation. The combined
automation/readout string remains compatible.

When editing begins, configure JUCE's live editor with the same font,
right-justification, vertical centering, and restrained focus outline. Select
only the numeric label text. When a unit-bearing field receives unitless input,
append its displayed unit only for parser evaluation; this preserves semantics
such as `3.1` beside `kHz` meaning `3100 Hz`. The formatter still determines the
unit after every successful commit, including transitions between `Hz` and
`kHz`.

## Test-First Contract

1. Formatted real, percentage, frequency, and decibel-like strings split into
   numeric and unit text without changing their combined display.
2. The resting numeric label has transparent background and outline.
3. The live editor uses centered-right justification, the display font, and
   numeric-only selected content while the unit remains visible beside it.
4. Unitless edits inherit the displayed unit, preserve the exact parsed slider
   value, and reformat correctly when editing ends.
5. Invalid input remains visible with the stable external unit and invalid
   colour, without changing the slider or emitting a gesture.
6. Existing compact/ordinary row travel and complete keyboard edit behavior
   remain unchanged.

## Negative Boundaries

- Do not create a custom text-editing lifecycle or bypass JUCE accessibility.
- Do not put the unit back into the editable label or selected text.
- Do not round the value committed to the slider to its two-significant-figure
  display representation.
- Do not grow the total readout allocation or reduce slider travel.

## Completion Criteria

- Focused geometry, focus lifecycle, parsing, invalid-input, and precision tests
  pass.
- Representative production fixtures for a percentage and frequency control
  pass through real editor binding.
- Standalone Debug builds; the Cycle V2 suite, `git diff --check`, style review,
  and production-diff review are complete.

## Implementation Evidence

- `PropertySliderRow` now owns a numeric editable label and a sibling unit
  label inside the unchanged readout allocation. Valid resting backgrounds and
  outlines are transparent; invalid input retains the external unit and gains
  the existing red treatment.
- The live JUCE editor inherits the numeric label's font, centered-right
  justification, zero indents, shared focus colour, and numeric-only selection.
  Unitless input is evaluated with the currently displayed unit before the
  existing parser commits the exact value.
- Focused tests pass 98 assertions, including frequency entry from `2.8 kHz`
  to exact `3100 Hz`, selection geometry, invalid input, percentage input,
  keyboard precision, and ordinary/compact track budgets.
- EQ and Reverb production fixtures assert separated `60` / `Hz` and `73` /
  `%` readouts plus transparent resting treatment. Delay and IR compatibility
  fixtures also pass without changing combined strings such as `1 beat`,
  `1×`, or sample/frequency readouts.
- Standalone Debug builds. The complete Cycle V2 suite passes 10,721 of 10,722
  assertions; the sole failure is the pre-existing edge-hover help assertion in
  `TestNodeCanvasHitRouter.cpp:66`.
- Production Retina captures confirm both states. The resting `60 Hz` readout
  has no field rectangle. On focus, the restrained outline encloses only the
  right-aligned numeric `60`; its baseline remains stable and `Hz` stays outside
  the editable selection. The focused fixture and evidence are
  `scripts/fixtures/cycle-v2-agent-property-value-focus.json`,
  `/private/tmp/cycle-v2-property-value-focus-report.json`, and
  `/private/tmp/cycle-v2-property-value-focus-detail.png`.
