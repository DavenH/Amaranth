# Cycle V2 Preset MIDI Previews

Status: Complete

## Revision design (2026-10-01)

`ModulationSource` maps CC1 to `modWheel`, arbitrary CC to `midiCC`, and
note-on velocity to `velocity`/`inverseVelocity`. The factory generator will
inspect attached `modulationTriple` nodes and emit only the CC number used by
an attached source. The attached triple is the authority; a source string in
an unconnected node is insufficient. Recorded phrases remain user-owned and
must not be regenerated. Factory phrases that still match committed generated
data may be refreshed once; locally edited sequences stay intact.

The keyboard transport moves into a narrow side rail, returning the dock to
112 px tall. The popup owns one viewport transformation for note and CC lanes;
trackpad wheel deltas pan that viewport. CC points have the same select, drag,
right-click delete, and Delete-key interaction as notes. Gestures publish once
at release. These changes do not move graph, DSP, or MIDI lifecycle policy.

Revision completion adds route-aware factory verification, distinct phrases
and velocity accents, side-rail geometry, trackpad pan and CC deletion tests,
an actual-size render review, build, audit, and commit.

## Contract and ownership

Each `.cyclegraph` may carry a bounded, time based MIDI phrase in
`presetPresentation.sequence`. The presentation codec owns validation and
serialization. The graph, its audio plan, and graph undo revisions do not change
when a phrase changes. Saving the preset persists the phrase.

The performance keyboard owns transport and the small piano roll editor. It
routes playback through the existing `MidiEventSink`; the audio engine remains
the authority for hardware MIDI capture and its realtime queue. Recording may
mirror incoming messages into a separate bounded queue and drain them on the
message thread; no allocation or document mutation occurs on the MIDI thread.
The workspace translates edited phrases to `NodeCanvas::setPresetSequence`,
which updates only preset presentation and dirty state.

Events include note start, duration and velocity, and time stamped CC values.
The first configured CC lane (or CC1 when present) forms a linear modulation
envelope during playback; other CCs retain their recorded event timing.
Playback sends note off before note on at shared boundaries. Stop and preset
load release preview notes. Existing presets without a sequence
keep the one-note audition behavior. The keyboard displays the phrase's pitch
range centred within its 24-semitone viewport where possible.

## Complexity and deletion

Loading and committing an edit cost O(events), independent of graph size.
Pointer movement changes one note or CC point without publishing a new document.
Timer dispatch costs O(events due in that tick), plus constant work for CC
interpolation. The one-note timer path remains a stable fallback for older/user
presets without phrases. No graph model or audio resource is copied while
recording or editing.

## Completion criteria

- All 249 current factory presets contain a musically appropriate phrase.
- Pad, bass, lead, keys, pluck, brass, and percussion templates differ in
  register, duration, rhythm, and velocity, with controller motion only when
  an attached modulation source reads that controller.
- Playback, stop, loop/finish, hardware recording, note editing, and CC editing
  work through the real UI and save/load paths.
- Round trip and interaction tests cover notes, CC, malformed data, transport,
  and recording. An actual-size screenshot verifies the popup and keyboard.
- Refactor, style, architecture audit, file sizes, and build/test checks pass.

## Architecture review

`GraphSerializer` remains the authority for the outer graph format; the existing
`PresetPresentationCodec` serializes the new phrase. `GraphCommandDispatcher`
owns the only command that changes its saved state. `NodeCanvas` grew by ten
lines, from 2,778 to 2,788, and only delegates this command and dirty-state
notification. `NodeWorkspace` grew from 552 to 591 lines and translates the
keyboard's callback into the command and mirrors recorded events from the
audio engine. It does not schedule notes or inspect graph nodes.

`PerformanceKeyboard.cpp` grew from 537 to 823 lines, crossing the 800-line
responsibility review point. It remains one
cohesive owner of the performance keyboard's held notes, audition transport,
record control, and MIDI sink: all these actions must agree on note release,
playback state, and MIDI routing. The popup's grid painting and gestures are in
`PresetMidiEditor`; validation and JSON are in `PresetPresentationCodec`;
hardware capture is in `StandaloneAudioEngine`. These boundaries avoid a second
note lifecycle policy. If the panel needs more transport modes, extract the
event scheduling and recording state into a keyboard-owned phrase transport
before adding another transport responsibility. The existing `revealNote` and
new `revealRange` make one pitch-visibility decision inside the keyboard;
neither the workspace nor the graph layer chooses a range.

Factory changes add only phrase data to 249 files. The revision refreshes 247
committed generated sequences and preserves local edits in `alkali-3` and
`altosax-2`. The generator preserves any existing hand-edited sequence, and no
mature DSP, graph topology, envelope,
or rasterization behavior is copied. There are no new node-kind branches.

## Verification

- All 249 factory files parse. Every CC lane matches a controller on an attached
  modulation source; 200 distinct automation curves are present. All 46 presets
  with velocity modulation use at least a 20-step velocity range. The generator
  matches all 247 refreshed sequences and leaves both local edits intact.
- Focused Catch2 keyboard, editor, and preset serialization tests pass:
  198 assertions across 19 cases. Editor tests cover CC point drag, right-click
  and Delete removal, two-finger pan, and Space transport. A separate transport
  test proves non-CC1 envelope interpolation.
- `cycle-v2-agent-preset-midi-preview.json` passes through the real app:
  play/stop, live note and mod-wheel capture, stop, save to a new file, reload,
  and retention of ten notes and six CC points. The popup Space fixture passes
  7/7 commands with playback starting and stopping while the editor has focus.
- Actual-size component captures were reviewed at
  `/tmp/cycle-v2-keyboard-revision.png` and
  `/tmp/cycle-v2-piano-roll-revision.png`. The vertical transport rail returns
  the keyboard dock to 112 px while preserving 25-by-100 px keys. The piano
  roll viewport and CC lane remain aligned. The in-app screenshot was black
  in the agent run, as in the previous revision.
- Standalone Debug and tests presets build with `--parallel 10`.
  `scripts/cycle_v2_architecture_audit.py` reports existing PLAN/REVIEW files;
  `PerformanceKeyboard.cpp` is a cohesive 823-line transport and MIDI panel.
  `CycleV2Automation.cpp` only adds a focused-component area resolver for the
  Space regression fixture. `git diff --check` passes. The only scalar
  `std::abs` calls added are CC point hit tests on pointer-down, outside any
  DSP or per-pixel loop. `clang-tidy` is not installed locally.
- The broader `[cycle-v2][preset]` tag remains 31/39 green due to the existing
  unrelated fixture and DSP parity failures tracked in `audio-bugs.md`.
