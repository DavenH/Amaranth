# Cycle V2 Preset MIDI Previews

Status: Complete — piano-roll navigation and performance revision

## Revision design (2026-10-01, follow-up)

The preset sequence's saved `durationSeconds` is the piano-roll timeline and
owns playback length whenever notes exist, including trailing silence. The
keyboard panel remains the sole MIDI transport and note-lifecycle owner. If a
sequence has no notes, it auditions the selected keyboard note for the
configured audition length, even when CC points remain. The popup sends
note-on/off requests through the panel for click audition and reads a transport
position callback for its playhead; it does not route MIDI itself.

The editor owns one viewport with an integer time-step offset and integer
lowest pitch. Wheel fractions accumulate across events before either axis
moves. Notes, curve points, grid, ruler, playhead, and minimap projection all
use that viewport. Wheel and minimap navigation publish no document edits.
The minimap shows the entire timeline and pitch extent, with a draggable
viewport rectangle. A dedicated velocity lane edits each note's saved
velocity; Shift-drag and arrow keys permit one-step precision. The modulation
lane is enlarged and follows the existing canvas palette's dark field,
contrasting curve, point handles, and viewport indication.

At pointer-down, note/velocity audition begins; pointer-up, popup destruction,
and a pitch change release the prior note. A visible right-edge grip and a
`MOVE`/`RESIZE` readout distinguish note gestures even when the note's start is
offscreen. Playback position repaint is UI-only. Editing remains O(1) per
gesture update (excluding local repaint); minimap painting scans sequence
events. No graph clone, audio preparation, or serialization occurs during a
gesture.

Baseline: `PresetMidiEditor.cpp` 395 lines and `PerformanceKeyboard.cpp` 823
lines. Paint and navigation will be split from gesture code if editor growth
crosses the 200-line review trigger. Deletion target: the old independent
horizontal/vertical wheel rounding and fixed grid origin. Complete when
focused gesture tests, playback-duration/fallback tests, actual-size render,
Cycle 2 fixtures, style check, architecture audit, build, and commit pass.

### Revision review and verification

The gesture and transport-facing editor is now 484 lines, with 400 lines of
viewport mapping and painting in `PresetMidiEditorView.cpp`; the original
395-line mixed editor no longer owns rendering. `PerformanceKeyboard.cpp` is
865 lines (from 823). Its responsibilities remain held-key state, MIDI sink,
recording, and audition transport. It delegates saved phrase validation to
`PresetPresentationCodec`, document publication to the workspace/dispatcher,
and all piano-roll viewport decisions to `PresetMidiEditor`. The panel alone
decides whether the phrase has notes and owns the note-off path for editor
audition and playback. No second transport or document-lifecycle policy was
added. The old wheel rounding and fixed horizontal grid origin were removed;
the view reads the editor's single viewport. There are no new graph-kind
branches, copies, serialization, or resource preparation in pointer updates.

Standalone Debug and `CycleV2_tests` build with `--parallel 10`. Focused
keyboard and sequence tests pass: 227 assertions in 25 cases. These cover
fractional trackpad pan, snapped note/CC positioning, minimap navigation,
offscreen resizing, note audition, velocity editing, explicit roll length,
trailing silence, CC-only audition fallback, and playhead/row rendering. The
Cycle 2 editor fixture passes 11/11 commands, including held-note release and
Space transport, and the preset preview record/save/reload fixture passes
24/24. Actual-size populated and empty editor captures were reviewed at
`/tmp/cycle-v2-piano-roll-large-populated.png` and
`/tmp/cycle-v2-piano-roll-large-empty.png`. The architecture audit reports
`PerformanceKeyboard.cpp` at the existing 800-line review threshold, below
the 1,200-line plan threshold. `git diff --check` passes; no DSP or raster
hot loops were changed. `clang-tidy` is unavailable locally. Seventeen
pre-existing modified factory preset files remain user-owned and unstaged.

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
