---
name: ui-design
description: Design, review, and refine application UI layout and interaction aesthetics using measurable proportions, spacing, control precision, visual hierarchy, and production-size screenshot evidence. Use for panels, controls, inspectors, toolbars, editors, meters, keyboards, and visual consistency work; use the SVG icon-system skill instead when the task is specifically a multi-icon vector family.
---

# UI Design

Design the interface as a coherent spatial and interaction system. Do not judge
quality from component bounds or source code alone: the production-size render
and the complete gesture are the source of truth.

## Establish the Design Contract

Before editing, inspect the current UI at its actual display size and identify:

- the primary user task and the content that deserves the most space;
- the relevant visual archetype, platform convention, or real-world reference;
- the product's existing type, color, radius, stroke, and spacing vocabulary;
- the available width and height, resize behavior, and likely localization;
- the states and gestures each control must support;
- mature repository components that already own equivalent behavior or style.

State the intended hierarchy and the few geometry rules that will make it
visible. When matching a platform or reference, compare directly with current
reference screenshots or native controls; do not approximate it from memory.

## Treat Geometry as Four Separate Contracts

For every important control, distinguish:

1. **Visual footprint** — how large it looks and how much emphasis it receives.
2. **Hit target** — the area that can reliably acquire hover, focus, or a drag.
3. **Manipulation mapping** — how pointer, wheel, and keyboard movement map to
   value changes, including fine adjustment.
4. **Value indication** — how precisely position, fill, text, or markers reveal
   the current value.

Do not enlarge all four merely because one is inadequate. A compact slider can
have a larger invisible hit target and a long relative drag range. A large knob
can remain coarse if its mapping is poor. A large thumb can be easy to grab yet
hide the value it is meant to indicate.

For continuous controls:

- make the indicator's exact reference point unambiguous;
- keep the thumb small relative to the usable track, or add a hairline, notch,
  fill boundary, or nearby numeric readout that shows the exact value;
- calculate usable travel after subtracting the thumb radius and end insets;
- provide enough pointer travel for the meaningful value resolution;
- add a discoverable fine-adjust gesture when ordinary travel is insufficient;
- support keyboard adjustment and show the value during precision work;
- keep the interactive target comfortably larger than thin visible geometry.

Write an adjustment budget for precision-sensitive controls. Let `D` be usable
ordinary drag distance and `R` the number of meaningful increments across the
range. If `D / R` is too small for intentional pointer placement, do not pretend
the visible track provides that precision: use relative dragging with adequate
gain, a fine-adjust modifier, keyboard increments, direct numeric entry, or a
combination. Test exact endpoints and representative values, not only whether
the control moves.

## Allocate Space by Information Value

Treat the panel as a finite budget. Give space to information and manipulation,
not to empty containers.

- Reserve the largest region for content whose shape, comparison, or direct
  manipulation benefits from area.
- Size simple controls by their information content and required precision, not
  by the size of the surrounding panel.
- Remove repeated nested insets. Padding should belong to one clear container
  boundary rather than accumulate at every child.
- Use a small spacing scale consistently. Similar gaps imply grouping; larger
  gaps or separators mark a genuine change of section.
- Avoid stranded space: unexplained central voids, one-label rows, sparse
  columns, and symmetric padding that separates related information.
- Share a row when controls are short, closely related, and still readable.
  Do not compress unrelated controls merely to increase density.
- Align repeated labels, values, tracks, baselines, and section edges. Optical
  alignment can override mathematical centering when shapes demand it.

Run a space audit: annotate the bounds of the panel, each section, its content,
and each gap. Any large region must have a stated job. If removing a gap does
not harm grouping, scanning, pointer safety, or calm, remove or reduce it.

When space is scarce, allocate it in this order: required content and legibility,
minimum usable control geometry, recognizable proportions, group separation,
then flexible breathing room. Leftover space belongs to the component that can
turn it into more information or manipulation precision; it should not be split
equally by default.

## Preserve Recognizable Proportions

When a component depicts or borrows from a familiar object, preserve the
relationships that make that object recognizable. Start from the reference's
aspect ratios, repetition, overlap, and hierarchy, then adapt only as required
by the task.

- Do not independently stretch subparts to fill arbitrary bounds.
- Keep repeated units on one scale and distribute remainder space outside the
  object or into a deliberate scroll, zoom, or overflow strategy.
- Test the extreme available sizes, not only the designer's preferred window.
- If fidelity and legibility conflict, document the specific adaptation and
  preserve the reference's most diagnostic relationships.

For example, a piano keyboard should derive black-key position and both key
widths from one key geometry; it should not make white keys squat merely to fill
the allotted height. A stereo meter should give most of its width to the two
scales and their readable separation, not to an unassigned central gulf.

## Build Hierarchy Without Waste

- Use size, weight, contrast, and spacing before adding boxes, gradients,
  decoration, or extra text.
- Keep typography legible at normal zoom. Use a small, intentional type scale
  and reserve emphatic sizes or weights for meaningful hierarchy.
- Prefer concise labels near their controls. Put transient explanations and
  operational guidance in the product's established console or help surface
  when one exists; retain local text when it is required to understand a risky
  or unfamiliar choice.
- Use color semantically and consistently. Never rely on color alone for state.
- Make enabled, disabled, hover, pressed, selected, focused, warning, and error
  states related but unmistakable.
- Avoid visual novelty that makes a standard operation harder to recognize.

## Interaction and Accessibility

- Make the rendered affordance, hit geometry, cursor, and actual event target
  agree. Test the real routed event, not only a classification helper.
- Keep selection stable while manipulating a selected object.
- Give every completed action immediate, proportional feedback.
- Support expected keyboard traversal and shortcuts. Escape dismisses a modal,
  menu, or transient popup; Return activates a clear default when appropriate.
- Preserve undo for edits and do not trap focus or pointer capture.
- Check light and dark appearance, accent changes, high contrast, reduced motion
  or transparency where relevant, focus visibility, and non-color state cues.
- Do not make an inactive control merely faint; preserve label readability and
  communicate why it is unavailable when the reason is not evident.

## Visual Review Loop

For a nontrivial visual change:

1. Capture the current interface and relevant reference at actual size.
2. Write a compact geometry and state specification before implementation.
3. Implement with shared layout and presentation primitives where appropriate.
4. Capture the result in the same state, size, scale, and appearance.
5. Compare silhouette, density, alignment, proportions, hierarchy, and control
   precision; fix specific discrepancies and repeat.
6. Exercise hover, press, focus, drag, fine adjustment, resize, disabled state,
   and keyboard behavior as applicable.

Add a focused automation fixture or semantic assertion for a UI regression.
Screenshots prove appearance; tests prove interaction and model effects. Use
both when the task changes both.

## Platform-Matching Work

When the request is to design or assess a macOS property panel that should be
indistinguishable from native UI, read
[references/macos-property-panel.md](references/macos-property-panel.md). Treat
its rubric as a pass/fail reference profile, not as the default visual style for
all products.

## Quality Bar

The result is complete only when:

- every large region and strong emphasis has a semantic reason;
- familiar objects retain credible proportions;
- compact controls remain easy to acquire and precise to manipulate;
- indicators reveal values more precisely than their visual mass obscures them;
- repeated elements share alignment, rhythm, and state language;
- important states and complete gestures work through the real event path;
- production-size screenshots show the intended improvement without creating a
  new density, hierarchy, or accessibility problem elsewhere.
