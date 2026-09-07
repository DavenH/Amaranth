# Cycle V2 UI Bug Notes

## Remaining priority

There are no open deterministic P0 or P1 UI regressions as of 2026-09-06.
Resolved and no-longer-reproducing entries have been removed from this ledger.

1. **P2 — Intermittent CoreMIDI endpoint assertion during automation startup.**
   Keep this behind reproducible product and automation failures because it has
   not affected fixture results and does not currently reproduce.

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
