# Cycle V2 UI Bug Notes

## Remaining priority

There are no open deterministic P0 or P1 regressions as of 2026-09-09.
Resolved and no-longer-reproducing entries have been removed from this ledger.

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

## P2: African Horn factory graph is not canonical JSON

Status: Open

The full Cycle V2 suite currently fails `Every shipped graph is canonical JSON
and compiles` on `african-horn.cyclegraph`. The graph compiles, but a
deserialize/serialize pass changes its JSON representation. This predates and
is independent of the document-declick changes; regenerate that preset through
the canonical serializer without expanding unrelated preset diffs.

## P2: Envelope purpose rail-spacing assertion no longer matches layout

Context:

- The full Cycle V2 suite on 2026-09-10 failed `Envelope purpose selector
  publishes bipolar pitch presentation` at `TestNodeEditorHost.cpp:2305`.
- The focused test reproduces independently: the second rail begins 39.825 px
  below the first, while the assertion expects 39.1 px within 0.02 px.
- The viewport-pan performance work does not touch Envelope editor layout,
  shared property rails, or the asserted geometry.
- Full-suite artifact: `/private/tmp/cycle-v2-pan-full-tests.log`.

Current status: open; reconcile the assertion with the current shared Envelope
layout contract without weakening minimum rail travel or hit-target coverage.

## P2: Broad native Trimesh sequence has intermittent late-step assertions

Context:

- Two native runs on 2026-09-11 completed the focused curve reshape and long
  point-drag checks, then failed at unrelated later steps: once when native
  delete did not reduce the vertex count, and once when graph-edge undo did not
  restore the expected edge count.
- The isolated `trimesh-point-drag` and `trimesh-curve-drag` native sequences
  both pass. No application assertion or crash was logged in these runs.
- The latest shared launch log is
  `/private/var/folders/zx/hdzf3v1s6vvdz7chbz40bbtc0000gn/T/cycle-v2-native-edit-smoke.log`.

Current status: open as broader native-fixture stability work; investigate the
delete targeting and graph-state polling independently of Trimesh drag pointer
lifetime.

## P2: Broad Waveshaper native sequence can drag the wrong vertex

Context:

- A focused run on 2026-09-11 inserted a Waveshaper vertex, then attempted to
  drag that exact intercept. The inserted vertex remained unchanged while the
  panel's current vertex moved to the requested destination.
- The failure occurs before the curve-reshape portion of the sequence and is
  independent of curve-pole or hidden-axis gesture scaling.
- The latest shared launch log is
  `/private/var/folders/zx/hdzf3v1s6vvdz7chbz40bbtc0000gn/T/cycle-v2-native-edit-smoke.log`.

Current status: open; make the fixture assert the hovered vertex identity before
mouse-down and investigate why the new intercept is not the drag target.

## P2: Envelope release native edit does not restore exact mesh on undo

Context:

- After native automation was corrected to target the exact launched process,
  the focused `envelope-release` sequence edited the release region but its
  final undo did not restore the initial serialized mesh exactly.
- The failure occurred after the hover-entry assertion passed and is independent
  of the Trimesh hover-proximity correction.
- The latest shared launch log is
  `/private/var/folders/zx/hdzf3v1s6vvdz7chbz40bbtc0000gn/T/cycle-v2-native-edit-smoke.log`.

Current status: open; compare the forward/inverse Envelope vertex delta after a
real routed release drag without folding that investigation into hover behavior.
