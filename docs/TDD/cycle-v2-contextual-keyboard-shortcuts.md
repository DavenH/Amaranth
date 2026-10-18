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

The focused morph control shows a small keyboard glyph as a shape cue in
addition to the existing focus colour. Its rounded shell, two rows of round
keys, and centered spacebar preserve the diagnostic proportions of the supplied
keyboard reference at an 18 by 13 pixel production size. The glyph is a shared
property-control presentation primitive, not a new icon family.

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
  Trimesh and Envelope.
- C/P select the matching sidebar tab only in canvas context.
- The app automation protocol can dispatch the new printable keys, and a
  focused fixture exercises the real component path.
- Production-size captures show the focused keyboard glyph without obscuring
  the rail, label, Axis button, or Link button.
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

`EnvelopeEditorComponent.cpp` grows from 772 to 865 lines. Its responsibilities
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

## Validation

- Standalone Debug build passes with `--parallel 10`.
- The `[keyboard]` Cycle V2 suite passes 455 assertions in 26 cases.
- `cycle-v2-agent-contextual-keyboard-shortcuts.json` passes all 45 commands,
  including both editors, undoable poles, links, C/P tabs, and Space playback
  while the performance keyboard is occluded.
- Production-size app captures show the 18 by 13 keyboard glyph within the
  focused Blue rail in both expanded editors.
- The architecture audit, touched-file size review, scalar-math hot-loop check,
  and `git diff --check` pass. Existing scalar math reported by the check is in
  untouched formatting/automation code, not a modified hot loop.
- A native macOS-key sequence is registered in
  `test_cycle_v2_native_edit_smoke.py`. Its local run is externally blocked
  because this process lacks Accessibility permission to send System Events
  keystrokes; deterministic component-path and fixture coverage pass.
