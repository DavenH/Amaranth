# Cycle V2 Curve Editor Primitives

## Status

Implemented.

Implemented in July 2026. Repeated JUCE presentation and transaction mechanics
now live in small parameter primitives, Envelope morph geometry/rendering lives
in `EnvelopeMorphControls`, and every concrete curve editor has its own
translation unit. The factory is a narrow construction switch and the former
monolithic `CurveNodeEditors.cpp` has been deleted.

## Problem

`CurveNodeEditors.cpp` contains four concrete editors plus styling, parameter
binding, automation export, layout geometry, Envelope interaction, and drawing
helpers. It compresses multiple declarations and operations onto individual
lines, hides repeated control lifecycle behind ad-hoc lambdas, and uses one
translation unit as the ownership boundary for unrelated concrete editors.

## Design

Introduce small presentation primitives around repeated contracts:

- `LabeledParameterSlider`: label, slider style/range, node-value sync, and
  transaction-aware publication hooks;
- `ParameterToggle`: typed Boolean binding and transaction publication;
- `ParameterRail`: repeated rail layout only;
- `EnvelopeMorphControls`: Envelope-specific morph plane, axis/link hit testing,
  and rendering.

Keep domain editors concrete and cohesive:

- `WaveshaperEditorComponent.cpp`
- `ImpulseResponseEditorComponent.cpp`
- `GuideCurveEditorComponent.cpp`
- `EnvelopeEditorComponent.cpp`

Each editor composes primitives, synchronizes its node model, and delegates
curve interaction to the authoritative widget/controller. Do not introduce a
universal editor schema, `NodeKind` switchboard, or generic component containing
Envelope domain behavior.

## Style And Enforcement

- One declaration and one executable statement per line.
- Methods remain under roughly 30 lines or extract a cohesive operation.
- Concrete component state is named and grouped by role rather than stored in
  dense semicolon-separated `Impl` declarations.
- Enable scoped clang-tidy checks for function size and isolated declarations;
  do not mass-edit unrelated legacy sources to silence new warnings.

## Semantic Tests

- Existing end-to-end authoring smoke tests still cover hover, cursor, add,
  drag, sharpen in both directions, parameter editing, and delete.
- Each editor round-trips its typed controls through graph commands.
- A transaction publishes one semantic edit and remains undoable.
- Envelope morph/link controls retain their current hit regions and behavior.
- OS capture verifies expanded editor layout and OpenGL composition.

## Deletion Targets

- Delete the monolithic `CurveNodeEditors.cpp` after concrete implementations
  move to cohesive translation units.
- Remove duplicated style, sync, transaction, and automation-export sequences
  replaced by the primitives.
- Retain the factory only as a narrow registration/construction boundary.

## Completion Criteria

- No concrete editor implementation shares a source file merely for
  convenience.
- Shared primitives own stable repeated mechanics; domain behavior remains in
  its concrete editor.
- The production diff passes the repository engineering loop: design,
  implementation, refactor, style check, semantic verification, and commit.
- Continue through committed slices without user scheduling until every
  deletion target and completion criterion above is satisfied.
