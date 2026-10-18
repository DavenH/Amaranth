# Cycle V2 Contextual Keyboard Shortcuts

## Status

Implemented (2026-10-18).

## Design

The expanded Trimesh editor's existing `TrimeshControlsComponent` and
`NodeEditorCommandService` are authoritative for morph values and link state.
Keyboard endpoint edits must use the existing begin/end morph gesture so they
remain one durable, undoable semantic command. The expanded Envelope editor's
existing curve transaction is authoritative for Red/Blue morph values;
Envelope links remain session-only and must not enter graph serialization.

`NodeCanvas` remains the owner of preview Space and of the unified
Curves/Presets/Patterns/Nodes sidebar. It may route a contextual key to the open
editor, but must not reproduce editor behavior or mutate the graph itself.
`InlinePresetBrowser::setActiveTab` remains the only sidebar-selection path.

The shortcut contract is:

- Trimesh: Y/R/B focuses the matching morph rail; Left/Right sends that rail to
  zero/one; L toggles its link.
- Envelope: R/B focuses the matching morph slider; Left/Right sends it to
  zero/one; L toggles its session-only link. There is no editable Time morph in
  the Envelope model, so Y has no Envelope target.
- Space always bubbles to `NodeCanvas` preview playback, including while an
  expanded morph control owns keyboard focus.
- On the bare canvas, C selects Curves and P selects Presets. Presets retains its
  existing delayed search-field focus.

Keyboard focus belongs to the complete morph tuple: label, rail, Axis control,
and Link control. A subtle neutral outline encloses that row, while the slider
keeps its ordinary value styling. A small keyboard glyph sits in a reserved
trailing slot outside the outline. Its rounded shell, two rows of round keys,
and centered spacebar preserve the diagnostic proportions of the supplied
keyboard reference at an 18 by 13 pixel production size. The outline and glyph
are shared property-control presentation primitives, not a new icon family.
The editor retains the last selected morph tuple when a child control loses
transient JUCE focus, so pointer release and ordinary focus transfer do not
remove the cue or leave Left/Right and L without a target. Selecting a different
morph rail, Axis control, or Link control replaces that editor-local target.

Cycle 1's other keys were reviewed separately. `A` remains the useful follow-on
for selection, with its exact select-all/clear-selection context to be designed
alongside the Cycle 2 selection workflow. The `7`/`8`/`9` link aliases, `E`
extrude, `W`, and `[`/`]` tour navigation are intentionally not carried forward:
their related Cycle 2 workflows are absent or not mature enough yet.

All shortcut dispatch is O(1). A morph endpoint key changes one parameter and
does not scan unrelated graph nodes, copy a graph or mesh, serialize state, or
prepare durable resources. Sidebar keys change presentation state only.

## Completion Criteria

- Focus, pole, link, undo, and Space behavior have focused semantic tests for
  Trimesh and Envelope, including arrow dispatch after component focus moves.
- C/P select the matching sidebar tab only in canvas context.
- The app automation protocol can dispatch the new printable keys, and a
  focused fixture exercises the real component path.
- Production-size captures show one subtle focus outline around the complete
  morph tuple and the keyboard glyph beyond its trailing edge, without a second
  slider-only focus ring.
- Architecture audit, touched-file sizes, diff review, style checks, and
  relevant tests pass.

No compatibility adapter or deletion target is introduced. The change reuses
the existing edit, publication, undo, repaint, playback, and sidebar owners.

## Architecture Review

The production change adds no graph scan, serialization, mesh copy, or durable
resource preparation. `NodeCanvas` grows from 3,403 to 3,427 lines, but its 24
new lines only route keys to the existing editor/sidebar owners and publish the
selected sidebar tab for automation. It owns no morph, link, or tab policy. The
existing `NodeCanvas` extraction work in `docs/TDD/refactors.md` remains the
appropriate deletion plan; creating another shortcut facade here would add a
switchboard without removing a responsibility.

`EnvelopeEditorComponent.cpp` grows from 772 to 881 lines. Its responsibilities
remain the Envelope editor's control composition, curve-transaction adapter,
and session-only presentation state. Keyboard handling translates keys into
those existing controls and does not add graph-command or serialization policy.
`NodeWorkspace.cpp` shrinks from 810 to 807 lines by removing visibility as a
playback eligibility rule: visibility is layout state, while the keyboard still
owns playback. `NodeCanvasAutomationInspector.cpp` grows from 1,138 to 1,139
lines with observation only.

The stable policy owners are:

- `NodeEditorCommandService` for durable Trimesh parameter/link commands and
  undo;
- `PrecisionSlider` plus the existing Envelope binding for curve transactions;
- `EnvelopeEditorComponent` for session-only Envelope links;
- `InlinePresetBrowser::setActiveTab` for sidebar selection; and
- `PerformanceKeyboardPanel` for playback lifecycle.

No policy is duplicated across callers. The canvas supplies only context and
routing, while each editor supplies its own available-axis facts.

The tuple-focus refinement keeps geometry in the existing owners:
`EnvelopeMorphControls` and `TrimeshSidePanelRenderer`. From the preceding
shortcut slice, `EnvelopeEditorComponent.cpp` grows by 16 lines to paint and
publish its row-level focus state, `TrimeshSidePanelRenderer.cpp` grows by 19
lines for the reserved slot and tuple bounds, `TrimeshWidget.cpp` grows by 10
lines as a literal geometry adapter, and `TrimeshControlsComponent.cpp` grows by
14 lines to paint the overlay. The latter two retain interaction routing only;
they do not duplicate layout constants or rendering policy. No touched file
grows by 200 lines, and the existing extraction plans for files above the review
threshold remain unchanged.

The persistent-focus follow-up keeps one editor-local axis string in each morph
editor. `PrecisionSlider` adds only a generic focus-gained callback; it owns no
Envelope policy. `EnvelopeEditorComponent.cpp` grows from 881 to 892 lines and
`TrimeshControlsComponent.cpp` from 532 to 542 lines. The former remains above
the responsibility-review threshold for its pre-existing control-composition
role, but this change adds no new collaborator or cross-subsystem policy.

## Validation

- Standalone Debug build passes with `--parallel 10`.
- The `[keyboard]` Cycle V2 suite passes 491 assertions in 27 cases, including
  real peer-dispatched arrows after focus moves away from the morph control.
- `cycle-v2-agent-contextual-keyboard-shortcuts.json` passes all 48 commands,
  including both editors, undoable poles, links, C/P tabs, pointer release, and
  Space playback while the performance keyboard is occluded.
- Production-size app captures show the neutral tuple outline and trailing 18
  by 13 keyboard glyph in both expanded editors, with no slider-only focus ring.
- The architecture audit, touched-file size review, scalar-math hot-loop check,
  and `git diff --check` pass. Existing scalar math reported by the check is in
  untouched formatting/automation code, not a modified hot loop.
- A native macOS-key sequence is registered in
  `test_cycle_v2_native_edit_smoke.py`. Its local run is externally blocked
  because this process lacks Accessibility permission to send System Events
  keystrokes; deterministic component-path and fixture coverage pass.
