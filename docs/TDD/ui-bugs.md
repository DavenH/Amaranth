# Cycle V2 UI Bug Notes

## Remaining priority

There are no open deterministic P0 or P1 regressions as of 2026-09-09.
Resolved and no-longer-reproducing entries have been removed from this ledger.

## P2: Cycle 1 default factory preset key no longer resolves

Context:

- The 2026-09-13 expanded Cycle 1 preset export sweep logged
  `FileManager::openFactoryPreset failed to resolve presetName="ooh-aah"` and
  asserted at `FileManager.cpp:174` during asynchronous startup.
- The preset library now contains `OohAah.cyc`; explicit loading and export of
  all 276 files succeeded, so this is limited to the stale default-preset key or
  filename-resolution policy.
- Repro log: `/private/tmp/cycle-preset-migration.log`.

Current status: open; select the intended default and make its persisted key
follow the factory preset filename-resolution contract.

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

## P2: Trimesh preview key scale also changes the default red morph axis

Context:

- After merging `master` on 2026-09-13, the focused
  `Trimesh preview pitch positions whichever morph axis owns key scale` test
  fails independently of the audio-parity and Voice Context conflict paths.
- With preview key scale assigned to Time at MIDI 48, Time reaches the expected
  normalized `0.261682`, but Red also becomes `0.261682` instead of retaining
  its neutral `0.5` value.
- The test and the relevant Trimesh preview behavior arrived from `master`; no
  conflict hunk touched that implementation.

Current status: open; reconcile key-scale preview ownership with the intended
single-axis contract before changing the assertion.

## P2: Envelope purpose rail-spacing assertion no longer matches layout

Context:

- The full Cycle V2 suite on 2026-09-10 failed `Envelope purpose selector
  publishes bipolar pitch presentation` at `TestNodeEditorHost.cpp:2305`.
- The focused test reproduces independently: the second rail begins 39.825 px
  below the first, while the assertion expects 39.1 px within 0.02 px.
- The full test run for audio-parity slices 48–50 on 2026-09-11 retained the
  same failure (`39.825` px versus `39.1 ± 0.02`).
- The viewport-pan performance work does not touch Envelope editor layout,
  shared property rails, or the asserted geometry.
- Full-suite artifact: `/private/tmp/cycle-v2-pan-full-tests.log`.

Current status: open; reconcile the assertion with the current shared Envelope
layout contract without weakening minimum rail travel or hit-target coverage.

## P2: Pan definition and legacy-migration assertions disagree with mode state

Context:

- The full Cycle V2 suite on 2026-09-11 fails `Pan presents as an inline cable
  control`: the current Pan node has two parameters while the test expects one.
- The same run fails `Graph JSON migrates legacy Pan range to its spectral
  Trimesh`: the migrated Pan retains a nonempty `mode` while the test expects
  none.
- These failures are independent of the delay, Output gain, and meter-cache
  paths, whose focused tests pass.

Current status: open; reconcile the Pan mode serialization contract and update
the paired definition/migration expectations together.

## P2: Full-suite Trimesh tests retain order-dependent failures

Context:

- A full randomized Cycle V2 test run on 2026-09-12 failed
  `Trimesh interactor movement policy matches the mature panel` because the
  shared move set contained eight vertices where the fixture expected two.
- The same run later crashed in `Trimesh Panel3D reads node-backed columns
  through lib data retriever` with `SIGSEGV`, followed by a
  `SingletonRepo.h:54` assertion. Focused global-audio graph tests do not touch
  Trimesh panel state and pass independently.

Current status: open; reproduce both tests under their recorded random order
and isolate leaked singleton/panel state before changing the mature movement
policy or test expectations.

## P2: Paired audio automation intermittently cannot focus the applications

Context:

- Organ 2 differential runs on 2026-09-11 repeatedly logged AppleScript error
  `-10006` when System Events attempted to activate either `Cycle` or
  `CycleV2`.
- Each application still completed its scripted offline capture and wrote a
  valid report, raw float sidecar, and WAV, so the error does not invalidate
  the audio comparison.
- Repro artifact: `/tmp/cycle-organ-2-reverb-mix-debug-4/`.

Current status: open; make foreground activation best-effort or synchronize it
with process launch without weakening command/report completion checks.

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

## P2: Complexity regression test omitted Guide noise seed

Context:

- The 2026-09-12 tests-preset build failed while compiling
  `TestInteractionComplexityParity.cpp` because its `GuideCurveResource`
  aggregate still used the field order from before `noiseSeed` was introduced.
- Production Guide behavior was unaffected; the test fixture passed its model
  pointer into the integer seed field and no longer compiled.

Current status: addressed by supplying the default `-1` seed explicitly.

## P2: Legacy Pan migration test expected the removed pre-mode schema

Context:

- The 2026-09-12 graph suite failed after loading a legacy Pan `mode` because
  the test still expected no Pan mode parameter.
- Pan now owns the canonical `auto`/`additive`/`multiplicative` mode parameter;
  loading removes the legacy payload and normalization supplies `auto`.

Current status: addressed by asserting the current canonical `auto` value.

## P2: Full Cycle V2 suite retains cross-test graph and Trimesh failures

Context:

- A full randomized `CycleV2_tests` run on 2026-09-12 completed the focused UI
  polish regressions, but reported 16 unrelated failures before a `SIGSEGV` in
  `Clicking an open Trimesh Guide selector dismisses its popup`.
- Other failures included graph compiler/validator connection expectations and
  an unavailable Impulse Response test stream. None of the failing paths
  overlap the segmented controls, Reverb preview profile, Envelope glyph, or
  Output meter presentation changed by the UI polish batch.
- The focused UI tests and native-size automation fixtures pass independently.

Current status: open; reproduce with the recorded Catch randomness seed
`3643743595` and isolate leaked shared Trimesh/graph fixture state before
changing the individual expectations.

Update 2026-09-12: another full run with seed `1137522538` reported 61
order-dependent failures and ended in `Signal probe detail resolves the
attached Voice Context key value` with `SIGSEGV`. Focused tests for Reverb,
node group movement, Trimesh selection/link highlighting, and cable hit routing
all pass independently; this remains an open suite-isolation defect.

Update 2026-09-12: the UI batch verification run with seed `4133085212`
reported three graph-fixture expectation failures, then the existing
`SingletonRepo.h:54` assertion and `SIGSEGV` in `Trimesh Panel3D reads
node-backed columns through lib data retriever`. The focused marquee and live
Reverb regressions pass independently; this remains the same open randomized
suite-isolation defect.

Update 2026-09-12: the focused `[cycle-v2][nodes][trimesh]` group reached a
`SingletonRepo.h:54` assertion and `SIGSEGV` in `Trimesh Panel3D reads
node-backed columns through lib data retriever` (seed `1390489111`). The same
case also fails in isolation before exercising the morph-selection correction.
The new morph-selection, disabled-control, and pointer-interaction cases pass
independently; this remains open as shared test setup/lifetime work.

## P3: Isolated Delay causal test expects previews from an unconnected graph

Context:

- `Ordinary DSP edits refresh configuration without compiling topology` fails
  in isolation because `previewRenderCount()` remains zero instead of two.
- The fixture contains only an unconnected Delay node, so the current runtime
  produces no previewable execution product. The focused connected Reverb
  causal sequence passes and the failure does not overlap Reverb kernel reuse.

Current status: open; reconcile the Delay fixture topology with its preview
count expectation rather than weakening the runtime product boundary.

## P3: Parallel macOS automation launches contend for CoreMIDI

Context:

- Launching two Cycle V2 automation fixtures concurrently produced CoreMIDI
  error 580 and JUCE assertions at `juce_CoreMidi_mac.mm:595`.
- Both fixtures completed their assertions; sequential launches do not report
  the error.

Current status: open harness constraint; serialize native app fixtures on macOS
unless the audio/MIDI device layer is explicitly disabled for automation.

## P2: Loading Cello emits runaway Intercept dangling-deletion assertions

Context:

- A focused Cycle 1 automation run opening `Cello.cyc` reached
  `Document::open returned`, then emitted repeated `*** Dangling pointer
  deletion! Class: Intercept` and `juce_LeakedObjectDetector.h:80` assertions.
- The assertion stream prevented the agent report from completing within 20
  seconds and grew to hundreds of megabytes before the launched process was
  stopped.
- Repro artifacts are `/private/tmp/cycle-agent-cello-red-guide-logs.txt` and
  `/private/tmp/cycle-agent-cello-red-guide-logs.txt.raw`.

Current status: open; inspect rasterizer snapshot/intercept ownership while
replacing a loaded mesh. This is separate from the repaired Visual DSP
render-only primary-axis selection.
