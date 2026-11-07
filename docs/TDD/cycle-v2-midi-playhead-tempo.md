# MIDI playhead, tempo grid and note templates

Status: Complete (2026-11-04).

Follow-up to cycle-v2-midi-authoring-upgrades.md. Replace overview viewport
navigation with playhead positioning; replace duration-derived grid spacing.
No Cycle 1 piano-roll seek owner exists; reuse V2 PatternPlaybackSchedule,
PatternControlScheduler and PatternPlayback source cancellation/reset.

Ownership and contracts

- PresetMidiSequence stores grid tempo (20..300 BPM, legacy default 120).
  Codec validates optional tempo; existing seconds-authored events retain their
  times when grid tempo or pattern Length changes. No implicit time stretching.
- PresetMidiEditor owns a four-beat viewport, beat/quarter-beat lines, 1/16-note
  snap and a compact BPM control beside the title. Length never rescales it.
  Horizontal/vertical wheel navigation remains; overview has no viewport box.
- The last touched note is the creation template for duration and velocity,
  including resize, velocity-lane and keyboard velocity edits. A fresh editor
  uses one grid step / velocity100 until a note is touched; template is ephemeral.
  Newly added notes still use the clicked pitch/time, respect the duration cap,
  move during their initial drag and publish once on release.
- Always draw a precise playhead and triangular ruler handle, including stopped.
  Click/drag the 24px ruler or 42px overview to seek continuously, independently
  of note snap. Overview seeking never changes pitch. Reveal offscreen playhead
  horizontally; overview has no independent viewport gesture.
- PerformanceKeyboardPanel owns stopped cursor position and playback restart.
  Space pauses/resumes there; end-of-pattern restarts at zero on next Play.
  Note audition and edit publication retain the stopped position, including a
  downstream sequence refresh after the save callback.
  Seeking while stopped is silent. Seeking while playing cancels scheduled old
  PatternPlayback events, resets only that source and schedules from the target.
  Explicit sequence seeking uses its full timeline for controller-only patterns;
  ordinary no-note keyboard audition retains its configured audition duration.
- PatternPlaybackSchedule clips expired notes and retriggers notes spanning the
  seek time with their remaining duration. PatternControlScheduler seeks directly
  to that time and uses its existing interpolation, without replaying old CCs.
- Seek is ephemeral: no pattern save, graph copy/serialization, undo/history or
  audio-plan preparation. Work is bounded by the pattern's note/controller lanes;
  no audio-thread allocations. Do not rebuild from each intermediate old CC step.

Delete navigateMiniMap, viewport rectangle/dragging policy and all duration-based
beat/snap scaling. Verify stopped/playing seek, exact boundaries, overlapping
notes, multiple automation lanes, old-event cancellation, pause/resume, no notes
and end behavior. Preserve the no-note fixed-key audition duration contract.
Verify legacy/new tempo round trips, malformed tempo rejection, stable grid/snap
on length edit/reopen and actual-app ruler/overview scrub and screenshots.

Baseline: editor 566 + view399 lines; PerformanceKeyboard.cpp1024 (transport and
keyboard orchestration), schedulers 45/65. Keep timing and clipping decisions in
existing schedulers and MIDI transport in the panel, separate from grid painting.


Verification

- Exact CycleV2 standalone-debug and CycleV2_tests targets built with --parallel10.
  Focused suite: 55 cases /1073 assertions, including existing MIDI authoring,
  preview transport, serialization and export scheduler contracts.
- Tests cover continuous stopped/playing seeks, exact end/start boundaries,
  remaining overlapping notes, multiple CC lanes at arbitrary fractional times,
  pause/resume/end restart, note-only and controller-only patterns. A real engine
  callback verifies old future notes are gone (zero scheduled events) and a
  hardware voice survives the pattern reset. No audio device is opened for it.
- Grid tests compare line positions before/after Length changes and after reopen
  at a non-default tempo; creation retains the same time/snap units. Optional
  legacy tempo defaults to120, non-default tempo round-trips, malformed values
  are rejected. Tempo limits belong to PresetMidiSequence for UI and codec reuse.
- Note-template sections cover touch-only selection, resize plus velocity-lane
  editing, and keyboard velocity changes. New note plus drag publishes once and
  inherits both shape values while retaining its clicked pitch/time.
- Dialog integration checks cursor retention through audition and downstream
  sequence refresh/save, and immediate Escape note release.
- scripts/fixtures/cycle-v2-agent-midi-playhead.json exercises real ruler and
  overview down/drag/up, stopped positioning and Space pause/resume. The isolated
  capture/report are .context/midi-playhead.png and .context/midi-seek-report.json.
  Document/graph revisions, pattern ID and event counts stay unchanged by scrubs.
- Code-map, architecture audit and diff checks pass. Existing repository size
  triggers remain. PerformanceKeyboard.cpp1048 owns keyboard and transport
  orchestration; actual note clipping and CC interpolation stay in its stable
  scheduler collaborators. No allocator, graph mutation or serialization logic
  moved into the editor. The added lifetime state is per-dialog note shape and
  per-panel playback position/timeline choice, independent of audio resources.
- Sizes: editor566->609, view399->388, panel1024->1048, note scheduler45->50,
  controller scheduler65->73. Deletion targets are complete: no overview viewport
  rectangle/navigation or duration-derived beat/snap mapping remains.

## Two-measure authoring buffer (2026-11-07)

Add two measures of empty horizontal scroll range after the last note end or
controller event at the current saved tempo. Keep longer explicit saved timelines
reachable and preserve the existing duration cap. Scrolling is ephemeral; only
authored events extend saved duration through the existing gesture/commit path.

Reuse the content scan already performed by updateDurationRange, cache its actual
endpoint, and update that bound in O(1) during extension gestures. Wheel updates
stay O(1), independent of note count and unrelated graph content. Keep playback
cursor framing within the authored timeline so overview/ruler seek behavior does
not change merely because free scroll now has extra space. Test at multiple
tempos, after new content, with later automation, empty data and the duration cap.
This interaction change is separate from the proposed visual editor redesign.

Completed: exact Debug/Release apps and tests build. Focused authoring, keyboard
and scheduler tests pass 1,151 assertions in 56 cases. Added cases cover 60/120/240
BPM, repeated extension with refreshed padding, automation after notes, empty
patterns, one publication per gesture and the 120-second cap. Existing overview
seek and authoring projection tests retain their behavior. The editor is 612
lines, its view 394, and header 127; policy stays in the existing editor owners.

The 12-command cycle-v2-agent-midi-scroll-tail.json app fixture passes. Its last
note ends at 3 seconds, and the scrolled ruler reaches 7 seconds at 120 BPM;
before/after exports are byte-identical. Software captures of this JUCE 2D
editor were inspected. Evidence: .context/midi-scroll-tail-tests.log,
.context/midi-scroll-tail-native.log, .context/midi-scroll-tail-after.png and
.context/midi-scroll-tail-before/after.cyclegraph. Code-map, architecture audit,
production style/diff and whitespace checks pass; no new incidental crashes or
runtime assertions were observed.

## Backspace returns to start (2026-11-07)

Backspace seeks to 0 and reveals the first measure without modifying notes,
automation or selection. Delete remains the event-removal key. Reuse the same
editor seek callback as ruler/overview scrubbing, so the panel preserves paused
or playing state and its existing source-reset/scheduler behavior. No new
transport owner, document publication or undo entry. Verify selected notes and
controls survive Backspace and still respond to Delete; verify return from the
scroll tail and actual app behavior while stopped and playing.

Completed: the editor and its shared seek operation stay at 621 lines; header
128. The automation input adapter adds only a Backspace key alias. Debug/Release
builds and 1,167 assertions in 57 authoring/keyboard/scheduler cases pass. Selected
notes and controls survive Backspace, remain deletable with Delete, and return
from the scroll tail reveals time zero. The 25-command app fixture verifies
paused return, playing restart without pause, and unchanged event counts;
before/after exported graph/pattern JSON is byte-identical. The captured ruler
starts at 1.1 with its playhead at zero. Evidence: .context/midi-backspace-tests.log,
.context/midi-backspace-native.log and .context/midi-backspace-start.png.
Code-map, architecture, production style/diff and whitespace checks pass.
