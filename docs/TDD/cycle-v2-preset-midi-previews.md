# Cycle V2 Preset MIDI Previews

Status: Complete

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
CC1 keyframes form a linear modulation envelope during playback; other CCs
retain their recorded event timing.
Playback sends note off before note on at shared boundaries. Stop and preset
load release preview notes. Existing presets without a sequence
keep the one-note audition behavior. The keyboard displays the phrase's pitch
range centred within its 24-semitone viewport where possible.

## Complexity and deletion

Loading and committing an edit cost O(events), independent of graph size.
Pointer movement changes one note or CC point without publishing a new document.
Timer dispatch costs O(events due in that tick), plus constant work for CC1
interpolation. The one-note timer path remains a stable fallback for older/user
presets without phrases. No graph model or audio resource is copied while
recording or editing.

## Completion criteria

- All 249 current factory presets contain a musically appropriate phrase.
- Pad, bass, lead, keys, pluck, brass, and percussion templates differ in
  register, duration, rhythm, and velocity, with selected CC1 motion.
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

`PerformanceKeyboard.cpp` grew from 537 to 804 lines, crossing the 800-line
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

Factory changes add only phrase data to 249 files. The generator preserves any
existing hand-edited sequence, and no mature DSP, graph topology, envelope,
or rasterization behavior is copied. There are no new node-kind branches.

## Verification

- All 249 factory files parse, with 3,089 note/CC events in range; 202 presets
  have CC1 motion. The generator is idempotent. A JSON comparison against the
  pre-change commit confirms that only new sequence presentation data was added.
- Focused Catch2 keyboard, editor, and preset serialization tests pass:
  195 assertions across 19 cases. The editor test covers move, resize, CC draw,
  delete, and one publication per complete gesture; recording also recentres the
  keyboard on a new pitch range.
- `cycle-v2-agent-preset-midi-preview.json` passes through the real app:
  play/stop, live note and mod-wheel capture, stop, save to a new file, reload,
  and retention of ten notes and seven CC points.
- Actual-size component captures were reviewed at
  `/private/tmp/cycle-v2-preset-keyboard-component.png` and
  `/private/tmp/cycle-v2-preset-midi-component.png`. The record/play/edit row
  preserves the 25-by-100 px key proportions, and the piano roll shows selected
  notes and a connected CC1 envelope. In-app screenshots were black in the
  agent run and OS region capture did not produce an image in this environment.
- Standalone Debug and tests presets build with `--parallel 10`.
  `scripts/cycle_v2_architecture_audit.py` reports the existing PLAN/REVIEW
  files, `git diff --check` passes, and scalar `std::isfinite` calls are only
  in JSON validation, outside DSP/visualization hot loops. `clang-tidy` is not
  installed locally.
- The broader `[cycle-v2][preset]` tag remains 31/39 green due to the existing
  unrelated fixture and DSP parity failures tracked in `audio-bugs.md`.
