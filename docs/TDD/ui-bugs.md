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

Update 2026-09-27: seed `3643743595` completed without a crash, with 39 failed
test cases and 41 failed assertions. Most failures now reference archived
factory presets or stale fixture topology; current focused causal and shipped
canonical-graph groups pass. Log: `/tmp/cycle-v2-seed-3643743595.log`.

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

## P3: Native authoring smoke inserts a global Delay into the voice graph

Context:

- The 2026-09-13 `authoring` native smoke creates a Delay from the FX palette
  and inserts it between `waveMesh` and `fft` in the per-voice oscillator path.
- The gesture succeeds, but graph validation correctly reports that the global
  Delay is neither reachable from Global Input nor connected to Output, so the
  fixture's `compileSucceeded` assertion fails.
- This is independent of explicit Trimesh signal types and single Voice Context
  inference; the focused Trimesh semantic fixture passes.

Current status: open; change the generic cable-insertion smoke to use a node
whose execution scope is legal in the selected cable, or assert insertion
separately from compilation validity.

## P3: Archived preset tests still target moved fixtures

Context:

- On 2026-09-17, a full Cycle V2 CTest run passed 1040/1076 tests and failed
  36, predominantly tests still naming root preset files or old Baroque and
  Stengah fixture contents after legacy presets were archived. Examples:
  `Prepared oscillator preset matrix...` cannot load its preset. This is
  distinct from the editor UI repairs, whose focused tests pass.

Current status: open; reconcile test paths and semantic expectations against
the canonical preset set rather than weakening the assertions.

- On 2026-09-18, `cycle-v2-agent-spy-detail.json` opened the probe detail on
  archived `old/stengah.cyclegraph`, but its `probeDetailRows` assertion
  expected 129 and observed 256. The capture-path extraction's focused tests
  passed. Report: `/private/tmp/causal-probe-extract-native.json`. Keep the
  fixture assertion until the intended archived graph and resolution contract
  are reconciled.

## P3: Primary-morph editor test still assumes a populated new Trimesh

Context:

- On 2026-09-30, the focused `Trimesh primary morph commits refresh graph
  presentation` case failed before its gesture assertions because it selects
  vertex 2 from a newly created Trimesh, whose correct default is now empty.
- The live primary- and non-primary-morph gesture cases pass independently.

Current status: open fixture maintenance; explicitly author the mesh topology
needed by the selection-preservation assertions rather than restoring implicit
content to newly added Trimesh nodes.
