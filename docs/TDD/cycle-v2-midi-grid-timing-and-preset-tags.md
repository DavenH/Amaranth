# Cycle V2 MIDI grid, playback timing, and preset tags

Status: Implemented.

## Authoritative implementations

`PresetMidiEditor` owns the piano-roll time projection and grid painting.
`RealtimeGraphRenderer` already applies queued MIDI at sample offsets derived
from absolute timestamps. `RealtimeMidiEventQueue` owns thread-safe MIDI
transfer, while `PerformanceKeyboardPanel` owns sequence transport and visual
key state. `PresetPresentationCodec` owns stored preset tags, and the factory
tagging script owns initial taxonomy. Pattern tags remain independent.

## Design

Major grid lines must be classified by absolute step index. Panning changes
their screen position together with notes and minor grid lines, rather than
restarting every fourth line at the left edge.

Preset metadata will no longer contain `Sustained`. Instrument families keep
their family tag; previously primary Sustained presets use the more useful
Texture family unless a more specific family applies. The `Sustained` pattern
category remains because it describes a sequence shape.

Sequence note and discrete CC events will be queued with their absolute
monotonic timestamps, using the renderer's existing sample-offset scheduler.
The UI timer only mirrors playback state and updates the playhead. A dedicated
preview MIDI source and cancellation epoch keep future queued events from
firing after stop or restart, without releasing manually held keyboard notes.
Interpolated automation remains a UI-rate control stream; rhythmic notes and
discrete CC points use the audio timestamp scheduler. No audio callback
allocation or graph publication is allowed.

## Architecture review

`PerformanceKeyboard.cpp` is 911 lines. It owns key interaction, transport,
recording, and panel layout; the new scheduling method only translates the
already ordered playback events to absolute times. `RealtimeMidiEventQueue`
owns cancellation generations, and `RealtimeGraphRenderer` owns sample-offset
execution. The app's `StandaloneAudioEngine` only translates clock time into
the queue API. No layer repeats the note scheduling or cancellation policy.
The queue capacity rises to 2048 to hold the codec's maximum 512 notes
(1024 note events) and 512 controls. Generation-based filtering scans future
events only after cancellation, rather than every audio callback. A future
keyboard panel extraction should move layout and recording into collaborators;
that is separate from the timestamp scheduling boundary here.

## Completion criteria

- Pan regression test proves major lines follow absolute beats.
- No current preset carries Sustained; curated primary replacements are saved.
- Note-on, note-off, and CC timing tests prove intended sample offsets across
  callback boundaries and after a delayed UI update.
- Stop/restart cancels future preview events without muting held manual notes.
- Focused UI and realtime tests, architecture review, style check, app build,
  and coherent commits pass.

## Verification

The focused keyboard, piano-roll image, preset taxonomy, and queue tests pass.
The realtime renderer test verifies silence before sample offset 137 and stale
pattern cancellation while a manual note remains active. The broader realtime
filter currently contains a pre-existing Astral fixture failure because
`content/presets/astral.cyclegraph` is absent in this workspace.
