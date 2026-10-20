# Cycle V2 Repeatable Performance Suites

## Status

Implemented (2026-10-20). Implements roadmap PR 5 independently of annotation
and capability UI.

## Contract and ownership

A host-side JSON suite describes bounded scheduled MIDI performances and explicit
render settings. One coordinated session renders each case and its repetitions to
WAV, retaining exact preset bytes, the effective exported graph, normalized
performances, executable identity, metrics and content hashes. Repetition compares
PCM bytes, independently of WAV headers. Failures retain a partial results manifest
and stop the suite. An existing output directory is never overwritten.

`scripts/cycle_automation.py exec` owns exact-instance lifecycle and its lease.
`scripts/cycle_automation_support/session.py` remains the only IPC transport.
`OfflineAudioCaptureAutomation` owns MIDI parsing and `OfflineGraphAudioRenderer`
owns preparation/execution. Every capture constructs a fresh renderer, MIDI queue,
voice state and processor graph; no host-side audio reset or second renderer is
needed. The runner loads a byte-for-byte preset snapshot, then exports the graph
through `GraphSerializer` before and after the run to detect document changes.
Preset audio resources are embedded by the existing graph serializer.

The shared `.cyclepattern` format is owned by `PatternLibrary` and
`PresetPresentationCodec`. `exportPatternSchedule` validates the retained sequence
with that codec, expands its controls through the mature `PatternControlScheduler`,
and translates MIDI messages through a bounded `MidiEventSink`. Python does not
implement pattern interpolation or note scheduling. The normalized schedule and
source pattern identity are retained in results. `PatternPlaybackSchedule` extracts
note event creation, note boundary ordering and control-lane preparation unchanged
from `PerformanceKeyboardPanel::rebuildPlaybackEvents`; the old body is deleted and
both callers use this core. Exported equal-time events use its existing note-off,
control, note-on priority. This is a deterministic offline ordering; realtime timer
lookahead and ambient wheel values are not synthesized by a preset suite.

## Files, size and lifecycle

- `scripts/cycle_performance_suite.py`: pure manifest validation/source retention,
  roughly 200 lines; no app, socket or DSP dependencies.
- `scripts/render_cycle_v2_performance_suite.py`: coordinator delegation and
  artifact orchestration, roughly 250 lines; no domain algorithms or direct IPC.
- `scripts/fixtures/cycle-v2-performance-suite.json`: small authored acceptance
  suite, including a shared library pattern.
- `scripts/tests/test_cycle_performance_suite.py`: semantic contract tests.
- `UI/PatternPlaybackSchedule.{h,cpp}`: extracted preparation, roughly 65 lines.
- `App/PatternScheduleAutomation.{h,cpp}`: bounded MIDI/JSON adapter, roughly 115
  lines; command dispatch/registry and CMake only wire these existing boundaries.
- `tests/TestPatternPlaybackSchedule.cpp`: interpolation and boundary-order
  behavior plus invalid/overflow failure; existing keyboard tests protect UI reuse.

All planning and I/O occur outside audio callbacks. Planning is O(events + cases),
plus stable event sorting; hashing is linear in artifact bytes. The host explicitly
sets defaults rather than inheriting mutable renderer defaults. Hard bounds cover
case count, repetitions, MIDI queue capacity, supported rate/block settings,
per-case duration and total rendered audio time. A wall budget bounds dispatch and
socket waits; the existing protocol cannot cancel an already executing capture.
No GUI gestures, presentation refresh policy, synthesis code, competing transport,
pattern persistence or temporary compatibility path is introduced. The extraction's
deletion target is the old keyboard-local event construction and comparator.

## Acceptance

- Notes, velocity, release tails, repeated/overlapping notes, timed CC and channel
  pressure use one explicit schedule. Cases can override rates and render settings.
- Invalid fields, nonfinite/out-of-range numbers, out-of-window explicit events,
  unsupported event kinds and excessive aggregate work fail before any app command.
- Pattern files retain their identity and use the mature interpolated MIDI data;
  unsupported/too-long patterns fail explicitly rather than silently truncating.
  Sequence validation and expanded event bounds use the app before audio capture.
- Two repeated real suite runs identify the same preset/performance, produce
  identical PCM for controlled cases and demonstrate fresh runtime state.
- Failed commands, mismatched audio, exhausted budgets and changed graph content
  produce failed results with usable evidence; incomplete output never reports pass.
- Unit contracts exercise normalization, timing/order, fingerprints, limits and
  artifact/failure behavior without substituting for the real audio acceptance.

## Evidence

The 18 focused Python contracts pass, including exact artifact identity, expanded
schedule delegation, interleaved repetitions, partial failures, graph changes,
wall-budget dispatch and source-library retention. Existing coordinator and audio
comparison tests also pass (16 and 15 cases). The code-map checker passes.

The architecture audit reports only pre-existing large coordinators. The changed
`PerformanceKeyboard.cpp` shrinks from 1010 to 983 lines; its playback preparation
is now owned by `PatternPlaybackSchedule`, while event delivery, display and
keyboard lifecycle stay in the panel. `CycleV2Automation.cpp` grows from 1025 to
1028 lines and remains command/lifecycle routing; its new branch delegates all
pattern work to a 89-line adapter, with no interpolation in that adapter. Shared
preparation is 45 lines; no node-kind branch or realtime work is added. The
keyboard-local preparation/comparator deletion target is complete.

The exact standalone app and test target build with `--parallel 10` in the root
workspace's existing cache at implementation commit `d14c2821`. The focused
pattern-schedule/keyboard set passes 29 cases and 677 assertions. Production C++
adds 173 lines and removes 37; the two host modules add 402 lines. There is one
command-kind dispatch addition and no node-kind, DSP, thread or publication branch.
The code-map route stays within its source-entry budget.

Two complete runs of the checked-in suite against `old/filter-saw.cyclegraph`
pass all eight captures each. Per-case repetitions are interleaved, and all eight
corresponding WAV PCM hashes also agree across runs. Both source preset bytes and
effective graph hashes agree. Cases cover soft attack/release, a hard 48 kHz note
with block size 127, repeated notes with timed CC/pressure, and the shared factory
keys pattern. Its three saved control points expand to 71 controller events and
83 total MIDI events through the mature scheduler; they are not rendered as steps.
Every capture has nonzero measured signal. Reports are under
`.context/pr5-performance-suites/.context/e2e-first/` and `e2e-second/` in the root
workspace. Coordinator runs are `fe7ccd65abf9` and `e40fe424b932`.

A separate four-capture `old/saxophone.cyclegraph` suite compares constant CC1 with
a change from 0 to 127 at 350 ms in a held note. Each version repeats exactly.
The 50–300 ms windows are byte-identical, while the 450–850 ms normalized RMS
residual is `0.1608849671`, proving that the timed control reaches audible output.
The report is under `.context/pr5-performance-suites/.context/sax-cc-sensitivity/`;
the comparison summary is `.context/pr5-performance-suites/.context/performance-verification.json`.
Its coordinator run is `c2067d158279`. All runs use the fresh exact root PID 38398;
the root-owned instance was stopped afterward. Filtered logs contain no product
assertion, crash or warning. No new runtime incident required a ledger entry.

Limits remain explicit product boundaries: pattern planning requires the app's
codec/scheduler before capture, a wall timeout cannot cancel an already executing
command, and a suite measures repeatability rather than perceptual quality.
