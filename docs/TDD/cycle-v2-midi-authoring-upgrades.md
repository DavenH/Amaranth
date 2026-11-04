# MIDI authoring upgrades

Status: Complete (2026-11-04).

PresetMidiEditor owns note/CC gestures, snapping and viewport projection. The
existing V2 piano roll and its gesture/save path are authoritative; no separate
Cycle 1 piano roll was found. PerformanceKeyboardPanel remains audition and
transport owner; GraphCommandDispatcher/PatternLibrary retain publication/save
ownership. Reuse JUCE DialogWindow for a centred, resizable popup.

- Shift-Up/Down transposes the entire note set one semitone. Preserve intervals,
  durations, velocity and automation; reject the whole operation at MIDI bounds.
- Start at 1120x820, grow for mapped CC lanes, and constrain/centre on the current
  display around the application window. Escape and close release audition.
- Extend sequence duration when created/moved/resized notes exceed it, up to the
  existing 120-second cap. Keep explicit trailing silence and manual length.
  Freeze viewport scale across note gestures; automatic growth must not move the
  grid under the pointer. Extend duration in O(1) on movement; rescan only at commit.
- Creating a note starts a move gesture. Existing right-edge grips still resize.
  Publish once per gesture, including creation plus drag; no graph copy/serialize
  or audio-plan preparation on movement.
- A Snap ON/OFF pill controls note and CC time snapping; pitch remains chromatic.
  Snap-off permits continuous positioning and resizing (minimum 10 ms).
- Add a bounded EditorAudition MIDI source. The panel sends AllSoundOff for that
  source before each new audition pitch, then NoteOn; mouse-up uses NoteOff.
  Reuse the real renderer's Reset path, isolating manual/hardware/pattern voices.
  Keyboard highlighting uses the existing visual-only mirror. No copied voice
  allocator/envelope state, new DSP or audio-thread allocation.

Delete the new-note-as-resize path, duration-end drag clamps, anchored callout
placement and shared manual-keyboard routing of editor audition. Keep existing
manual length, velocity editing, CC lanes, minimap and transport semantics.

Verify transpose bounds/intervals, free and snapped drag/resize, automatic length
beyond the prior end with stable scale, one commit per gesture, drag audition
handoff and real source reset isolation. Build exact app/test targets with
--parallel 10; inspect a centred popup at production size through an isolated
agent instance. Run focused tests, code-map/architecture and diff checks.

Baseline: PresetMidiEditor.cpp 513 lines (gestures), PresetMidiEditorView.cpp
385 (painting/projection), PerformanceKeyboard.cpp ~1000 (keyboard/transport
orchestration). Keep new editing behavior inside the editor, MIDI voice resets
inside the renderer, and use a concrete nested Snap pill rather than introducing
an application-wide toggle framework for a single control.


Acceptance evidence

- Built CycleV2 (standalone-debug) and CycleV2_tests (tests) with --parallel 10.
- Focused run: 49 cases / 8970 assertions pass. Covers whole-phrase transpose
  and MIDI bounds; snapped and continuous movement/resizing and CC timing;
  creation/move/resize beyond the old endpoint; fixed drag projection; one
  publication per gesture; audition handoff and immediate Escape release.
- The production renderer retires only EditorAudition on AllSoundOff while
  manual and hardware voices keep sounding. Overflow recovery covers all four
  sources. The reset is queued before preview preparation for the next pitch.
- Isolated actual-app fixture verifies popup capture, Space transport and Escape:
  `.context/midi-authoring-report.json`; screenshot `.context/midi-authoring.png`.
  The dialog is centred over the app and constrained to the display; the default
  plot retains the overview, velocity and CC lanes with Snap next to Length.
  Use the explicit presetMidiEditor area in inactive automation instances;
  `focused` is intentionally unavailable until the app acquires keyboard focus.
- A broader editor run found three unrelated Trimesh fixture failures, recorded
  in ui-bugs.md; no changes to those production paths. Replaced an old hard-coded
  green-cursor pixel expectation with visible cursor contrast across themes.
- Code-map validation, architecture audit and diff checks pass. The audit reports
  the repository's existing size triggers. PerformanceKeyboard.cpp is now 1024
  lines: keyboard/transport orchestrator, with audition routing and concrete JUCE
  dialog placement. PresetMidiEditor owns editing policy; DialogWindow owns modal
  lifecycle; the renderer retains voice Reset semantics. No duplicate allocator,
  scheduler, graph transaction or envelope rules were introduced.
- PresetMidiEditor.cpp: 513 -> 566 lines; view: 385 -> 399;
  PerformanceKeyboard.cpp: 1001 -> 1024; RealtimeGraphRenderer.cpp: 822 -> 819.
  All named deletion targets are removed. Publication and PatternLibrary save
  behavior remain on their existing commit callbacks; drags only edit local
  events, update length in O(1), audition and repaint. Saved presets were not edited.
