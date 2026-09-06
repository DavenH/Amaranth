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
