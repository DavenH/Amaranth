# Cycle V2 UI Bug Notes

## Remaining priority

There are no open deterministic P0 regressions as of 2026-09-09.
Resolved and no-longer-reproducing entries have been removed from this ledger.

1. **P1 — Cycle 1 Calming Keys preset crashes during visual refresh.**
   Reproduce normal interactive preset replacement, then correct the Envelope
   or Unison visualization lifetime boundary without coupling migration back to
   the live document.
2. **P2 — Drunkard pitch render logs VisualDsp column-size assertions.**
   Reproduce the visual update and establish the authoritative column
   resolution before adapting the copy boundary.

## P2: Intermittent CoreMIDI endpoint assertion during automation startup

Context:

- A sequential property-control fixture run on 2026-08-27 completed every
  Reverb command successfully but logged CoreMIDI error `580` and three JUCE
  assertions at `juce_CoreMidi_mac.mm:595` during application startup.
- The five adjacent Cycle V2 fixture launches did not report the assertion,
  and the issue was independent of property value formatting or slider paint.
- Original repro artifacts were
  `/private/tmp/cycle-v2-reverb-property-controls-precision-logs.txt` and its
  `.raw` companion.

Current status: open but not reproduced on 2026-09-06 across the native
Envelope smoke, focused Cycle V2 hover-help and Guide-dock launches, or five
consecutive launches of the Reverb property-control fixture. All five Reverb
runs completed with zero failed commands. Capture the initialized endpoint
names and launch sequence if it recurs before changing MIDI initialization or
teardown.

## P2: Cycle 1 menu smoke requests the removed Graphics menu

Context:

- An incidental smoke run on 2026-09-06 listed the current menus successfully,
  then `cycle-agent-menu-commands.json` failed with `Menu not found: Graphics`.
- The application now exposes the related display commands under `View`; the
  audio-pipeline changes do not alter menus or the fixture.
- Repro report:
  `/private/tmp/cycle-agent-audio-parity/cycle-agent-smokes-report.json`.

Current status: open; update the menu fixture against the intended current menu
contract in a focused UI slice.

## P2: Drunkard pitch render logs VisualDsp column-size assertions

Context:

- The focused pitch-envelope audio render passes its signal thresholds but logs
  three assertions at `VisualDsp.cpp:367` because source and destination column
  sizes differ.
- The assertion is in visualization column copying, outside the audio voice and
  envelope paths changed by the parity fix.
- Repro log: `/private/tmp/cycle-agent-pitch-envelope-log.txt`.

Current status: open; reproduce through the visual update path and decide which
column resolution owns resampling before changing the assertion.

## P2: Graph document save test cannot use JUCE's default temporary directory

Context:

- The full Cycle V2 suite and a focused rerun on 2026-09-07 fail
  `Graph documents save canonical JSON with stable line endings` at
  `TestGraphSerializer.cpp:111`.
- JUCE first asserts at `juce_TemporaryFile.cpp:126`; the destination is chosen
  from `File::tempDirectory`, and `GraphDocument::save()` then returns false.
- Preset serialization itself passes in the workspace-backed native migration
  runs, including 221 load/save/reopen cycles.
- Full-suite log: `/private/tmp/cycle-v2-full-tests.log`.

Current status: open as a sandbox/test-fixture path issue. Give temporary-file
fixtures an explicitly writable test root rather than weakening document-save
behavior.

## P1: Cycle 1 Calming Keys preset crashes during visual refresh

Context:

- The Cycle 1 factory-library migration sweep opened and exported 28 presets,
  then crashed while opening `cycle/content/presets/calming-keys.cyc`.
- The crash is an invalid `dynamic_cast` read in
  `EnvRasterizer::renderWaveformOnly()` reached from
  `UnisonPhaseColumnRenderer`, `VisualDsp::processFrequency()`, and the pending
  UI update graph. It occurs after the document load starts scheduling visual
  work; the preset's canonical migration itself is not yet implicated.
- Repro artifacts are `/private/tmp/cycle-v1-preset-migration-session.log` and
  `/Users/daven/Library/Logs/DiagnosticReports/Cycle-2026-09-07-111853.ips`.

Current status: open. Update suppression was insufficient because live document
application still touched UI-owned state, and `cluck-2.cyc` exposed the same
class of failure. The migration exporter now decodes and migrates the source
without applying it to the live document, isolating canonical export from
editor, updater, rasterizer, and audio lifecycles. Reproduce normal interactive
loads separately before changing Envelope or Unison rasterization ownership.

## P2: African Horn factory graph is not canonical JSON

Status: Open

The full Cycle V2 suite currently fails `Every shipped graph is canonical JSON
and compiles` on `african-horn.cyclegraph`. The graph compiles, but a
deserialize/serialize pass changes its JSON representation. This predates and
is independent of the document-declick changes; regenerate that preset through
the canonical serializer without expanding unrelated preset diffs.
