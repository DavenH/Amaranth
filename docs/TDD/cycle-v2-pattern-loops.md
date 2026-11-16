# Note-defined MIDI pattern loops

Status: Complete.

## Contract

Note playback ends each note at its authored note-off, then loops after the end
of the 4/4 measure containing the last note. Stored trailing roll padding and CC
points do not extend a note pattern's loop. Notes exactly on a measure boundary
do not add another measure. CC-only/empty keyboard audition retains its existing
configured duration and stop behavior. Preserve authored pattern data and tempo.

PatternPlaybackSchedule owns effective measure-aligned playback length and event
preparation. PerformanceKeyboard owns timed dispatch and UI mirroring. Schedule
next-cycle events inside existing lookahead so note-on timestamps remain at the
measure boundary, independent of UI timer arrival; explicit stop cancels the
pattern source. Late UI updates skip expired cycles rather than growing an
unbounded queue. No DSP, persisted format or graph editing algorithm changes.

Shift+A in PresetMidiEditor selects every note (including offscreen notes),
clears CC/velocity selection and does not publish a musical edit. Reuse existing
selectedNotes and group move/delete behavior. Keep Shift+arrow interactions.

## Verification

Cover short notes with stale long roll padding, final-note measure boundaries,
changed tempo, exact loop timestamps, rest after note-off, explicit stop and late
updates. Retain CC-only audition behavior. Cover Shift+A then transpose/delete,
controller preservation and selection without publication. Build Debug app/tests
and Release app; use isolated native editor/playback checks, architecture audit,
source responsibility review, code-map and whitespace. Preserve incoming presets.


## Outcome and evidence

Loop length is derived once when the sequence changes, keeping playback-position
queries O(1). Scheduling uses the existing 0.5-second lookahead and original
transport origin for exact next-cycle timestamps; at most the current and next
cycle are prepared after a delayed UI update. Natural note-off/release tails
remain intact across loops; only explicit Stop releases the pattern source.
CC schedulers are capped at the note-defined measure boundary during live note
playback. One-pass export retains its existing declared-duration contract.

The existing trailing-roll transport regression now asserts note-defined looping,
silent rest, prequeued next note-on, skipped expired loops and explicit Stop.
Additional focused coverage checks exact measure boundaries/tempo and Shift+A
with offscreen notes, transpose/delete and preserved controllers. Existing seek,
CC-only audition and scheduler-export checks pass. No graph/preset migration.

Native evidence: .context/pattern-loop/results.json, selected-notes.png and
.context/pattern-loop-native-focused.log. A 30-second declared roll with its final
note ending at 1.2 seconds rests at progress 0.709, then wraps near zero at the
2-second measure boundary. Native Shift+A followed by Delete leaves zero notes.
The older combined curve/transport workflow stopped at an unrelated AX label
lookup (first stale fixed curve count, then missing AX label); its transport end
assertion is updated for loops, and its curve count derives from the loaded
preset. The dedicated pattern workflow avoids that unrelated accessibility query.

Debug app/tests and Release app build. Focused schedule/editing/transport/seek
checks pass, along with whitespace, code-map and architecture audit. Source
responsibility review: PerformanceKeyboard is 693 lines; it retains transport
orchestration and cached timing. PatternPlaybackSchedule (111 lines) owns timing
policy/preparation. PresetMidiEditor dispatches the shortcut and its existing
Selection source owns select-all mechanics. New native workflow is indexed in
code-map.md. No duplicated control interpolation, realtime allocation, graph
publication or unrelated preview rebuild is introduced.
