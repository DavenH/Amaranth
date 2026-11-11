# Daily usability

Status: Implemented

## Ownership and implementation

- Runtime: fix Trombone through the existing prepared spectral oscillator, not a
  preset-specific DSP path. Compare with ordinary IFFT optional-phase semantics.
- Pattern library: retain PatternLibrary persistence and PerformanceKeyboardPanel
  audition. Library edits remain ephemeral until Save or close requests a named
  copy; never overwrite the source ID. Cancel leaves the library unchanged.
- MIDI editor: extend its local sequence selection and gesture owner. Shift-drag
  selects intersecting notes; Shift-arrows move selected notes by beat/semitone.
  Reuse JUCE rectangle intersection and existing timing conversion. No graph work
  during movement; sequence publication occurs once at gesture completion.
- Canvas: use NodePalette's catalogue and NodeCanvasAuthoring's dispatcher path;
  preserve existing object context menus and anchor empty-space creation at the
  clicked world coordinate. Do not add a second node catalogue.
- CC controls: retain the existing preview controller routing; shorten drag travel.

## Boundaries and size

No new DSP algorithm, graph mutation path, serialization format, or dependency.
Remove immediate shared-pattern writes from library-editor callbacks. Keep new
selection mechanics in a focused editor translation unit and pattern persistence
workflow in its workspace collaborator. Existing large canvas/keyboard files get
only delegation and lifecycle wiring; their existing scene, authoring and playback
collaborators retain policy ownership. Expected production change: a few hundred
lines; no audio-thread allocations or aggregate graph copies.

## Acceptance

- Trombone prepares and tracks MIDI pitch independently of callback size; missing
  IFFT phase behaves like the ordinary IFFT path.
- Changed library drafts prompt on Save/Ctrl-S/Cmd-S and dismissal; unchanged
  drafts do not. Saving uses a fresh ID, preserves source bytes and other consumers,
  and assigns the saved copy. Cancellation and failures preserve the original.
- Selection is visible, supports additive Shift-click/box, group drag and deletion;
  one-beat horizontal and one-semitone vertical nudges preserve relative spacing
  and respect MIDI/time bounds. Empty selection does not transpose the phrase.
- Empty-space menu appears at a non-origin click and creates through the dispatcher
  with undo; node/cable/group menus retain precedence.
- Build exact app/tests, run focused behavioral tests and sequential app automation,
  inspect diff/style, run architecture audit and code-map check.

## Deferred

Visual-reference tray, browser polish, cursors, plugins, Qt and global restyling.

## Verification and architecture review

Built CycleV2 and CycleV2_tests with standalone-debug and parallelism 10.
Focused tests cover MIDI selection, two group-drag updates with one publication,
keyboard bounds/deletion, pattern draft cancellation, library management, CC
sensitivity, factory preparation and Trombone audio. Native automation verifies
non-origin menu placement, node creation/undo, Cmd-S/Ctrl-S, cancellation,
copy-on-close, and preservation of both factory and user-pattern consumers.
The pattern fixture checks unchanged source ID during audition, a new saved ID,
and unchanged source notes after reopening. Live Trombone pitch evidence is
recorded in audio-bugs.md and `.context/trombone-live-pitch.json`.

Production diff: 606 lines added and 210 removed across runtime/UI C++ (including
the 138-line pattern workflow extraction). All 42 focused tests passed with 1909
assertions; the copy-edit fixture passed 21 commands.

Size review: NodeWorkspace shrank from 1065 to 935 lines; its pattern persistence
boundary is now 195 lines. NodeCanvas remains 3750 lines with only context-menu
routing added; the 33-line catalogue menu delegates all edits to existing
NodeCanvasAuthoring/GraphCommandDispatcher. PerformanceKeyboard remains 1066 lines
with playback and controller policy unchanged except the shorter drag; the
101-line window owns draft/save/dismissal lifecycle. Selection mechanics occupy
78 lines alongside the existing editor, with local O(notes) box queries and
O(selected notes) movement. No graph copies, serialization or publication occur
on selection movement. New branches concern interaction/optional phase only;
there is no new node-kind DSP dispatch or compatibility path to delete.

Reused the ordinary IFFT zero-phase policy, Buffer scratch storage, palette
catalogue, authoring commands, PatternLibrary atomic persistence, JUCE menus,
rectangles and native dialog lifecycle. Architecture audit, code-map validation
and diff whitespace checks completed. Deferred tasks above remain separate.

## Master integration (2026-11-11)

Merged `origin/master` at `69def2ef`. Retained the compact editor, three-octave
pitch grid, scrollable controller lanes, notes-only overview and native dialog
title. The copy-save button sits beside Clear without restoring the removed
Length widgets or shortcut footer. Selection hit testing now reuses `pitchRow`
instead of the former 24-row calculation. Plain arrows retain measure seeking;
Backspace retains return-to-start, Delete removes selected events, and
Shift-arrows move the selected notes. Updated transpose/boundary tests select
notes explicitly; gesture tests derive positions from pitch rows.

Adopted master's extracted JUCE rotary controls and removed the obsolete wheel
implementation during conflict resolution. Ordinary travel remains 32 px;
master's fine Shift travel and endpoint/key behavior remain with JUCE Slider.
The merged focused suite passes 3,695 assertions in 72 test cases.

Merged Debug app and test targets build. The pattern-copy (21 commands), compact
editor layout (9), and playhead (21) app fixtures pass sequentially. Native
node-menu creation/undo, Cmd-S/Ctrl-S, cancellation and preservation of factory
and user-pattern consumers pass. Reviewed `.context/midi-pattern-selection.png`
for the integrated compact editor and Save/Clear alignment. Code-map,
architecture audit and whitespace checks pass; the panel/editor/view/selection
owners are 647/656/389/76 lines after integration.
