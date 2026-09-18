# Cycle V2 UI Bug Notes

## Resolved P2: Trimesh spectral backgrounds and expanded surface resolution

Reported 2026-09-18. Spectral Trimesh backgrounds used a fixed 128-position
harmonic ramp, the 3D background ignored the key-scale pitch across columns,
and expanded surfaces used 96 columns regardless of panel width. The preview
pitch resolver also missed the Voice Context on factory graphs with implicit
context routing. The panel now uses the full per-key `LogRegions` ramp and
pitch-dependent 3D harmonic traces; expanded grid resolution follows the
panel width. The Organ 4 native fixture reports 586 columns across a 586-pixel
panel and pitch spanning MIDI 20–127, with no failed commands.

## Remaining priority

There are no open deterministic P0 or P1 regressions as of 2026-09-09.
Resolved and no-longer-reproducing entries have been removed from this ledger.

## Resolved P2: Mod Wheel release published duplicate graph/Spy updates

Reported 2026-09-17. While moving the MIDI keyboard Mod Wheel, the Spy appears
to refresh twice after release, suggesting two graph updates for one movement
or gesture commit. A graph update also appears to cancel a running audition
note. These are observations, not yet an established shared cause; the preview
audio failing to follow the wheel on `filter-saw-2` is tracked separately in
`docs/TDD/audio-bugs.md`.

Current status: focused On Release path resolved 2026-09-17. The Honerism 3
fixture reproduced two preview requests and a canceled held note before the
repair, then one asynchronous request/publication with the held note preserved.
The workspace now skips unchanged audio-plan copies and defers compatible plan
adoption until the current note ends. Live-mode commit deduplication remains in
`docs/TDD/cycle-v2-mod-wheel-refresh-audit.md` and the causal-policy cleanup.

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

## P1: Opening a graph can discard unsaved edits without confirmation

Context:

- Moving a node now publishes the normal dirty-document presentation, exposing
  the same unsaved state as other semantic graph edits.
- Opening another preset while the current graph is dirty replaces the document
  without a save/discard/cancel decision.
- This branch intentionally fixes dirty-state publication only; the document-open
  confirmation is separate window/document-lifecycle work.

Current status: open; prompt to save, discard, or cancel before replacing a dirty
graph, and clear the dirty state only after a successful save or confirmed
discard.

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

## P3: Causal-tag runtime suite misses expected probe previews

On 2026-09-17, `CycleV2_tests '[cycle-v2][runtime][causal]'` reported two
`TestGraphRuntime.cpp:48` failures because `findProbePreview` could not find
the requested probe. One was the Stengah asynchronous Waveshaper test at line
839 (`probe2`); the other occurred earlier in the same tag run. The focused
presentation-session and policy tests pass. Log: `/tmp/causal-all-tests.log`.
Current status: open; identify the second case and determine whether preset
topology or preview planning caused the missing probes.

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

## P3: Baroque preset transition omits the expected probe preview

Context:

- On 2026-09-15, `Preset transitions replace equal-revision Trimesh DSP
  content` failed in isolation at `findProbePreview(..., "probe")` after the
  African Horn to Baroque Flute presentation refresh.
- The failure occurs before the test's audio-capture comparison and reproduces
  with the audio executor's prepared-plan retirement disabled, so it is not
  caused by the realtime dispatch optimization.

Current status: open; inspect preset probe identity and incremental preview
publication across equal-revision configuration transitions.

## P3: Stengah Spy test expects probes absent from the current fixture

Context:

- On 2026-09-17, a full Cycle V2 CTest run passed 1040/1076 tests and failed
  36, predominantly tests still naming root preset files or old Baroque and
  Stengah fixture contents after legacy presets were archived. Examples:
  `Prepared oscillator preset matrix...` cannot load its preset, and
  `Curve panel adapters resynchronize equal-revision models...` expects 55
  Stengah Guide vertices while the current fixture has 73. This is distinct
  from the editor UI repairs, whose focused tests pass.

Current status: open; reconcile test paths and semantic expectations against
the canonical preset set rather than weakening the assertions.

- The grouped `[cycle-v2][runtime][probe][presets]` run on 2026-09-14 fails
  `Stengah spies render the exact output selected by each probe` because the
  freshly migrated `stengah.cyclegraph` contains an empty `probes` array.
- The focused PWM Lead Spy regression uses its checked-in probe and passes.

Current status: open fixture/test synchronization issue; restore the intended
Stengah probes or update the test fixture at its authoring boundary.

- On 2026-09-18, a grouped causal/presentation/Reverb test run again failed
  the Stengah async Waveshaper probe case at `findProbePreview(..., "probe2")`
  immediately after loading the preset; 48 of 50 cases passed. The local
  Reverb preview path is not invoked by this test. Log:
  `/private/tmp/causal-reverb-tests.log`.

- On 2026-09-18, `cycle-v2-agent-spy-detail.json` opened the probe detail on
  archived `old/stengah.cyclegraph`, but its `probeDetailRows` assertion
  expected 129 and observed 256. The capture-path extraction's focused tests
  passed. Report: `/private/tmp/causal-probe-extract-native.json`. Keep the
  fixture assertion until the intended archived graph and resolution contract
  are reconciled.

## P3: Opening an Envelope editor marks the document dirty

The 2026-09-17 `cycle-v2-agent-envelope-link-toggle.json` run opens the saved
`old/vox-1.cyclegraph` clean, but `documentDirty` is already true immediately
after opening the expanded pitch Envelope, before any link click. The focused
link fixture therefore checks session state and reopen behavior, not dirty
state. Report: `/private/tmp/cycle-v2-envelope-link-session2`.

Current status: open; distinguish selection/editor-state publication from a
durable document edit. Envelope link toggles themselves remain session-only.

## P3: With-spies Trimesh morph fixture expects a value absent after load

On 2026-09-18, `cycle-v2-agent-trimesh-morph-selection.json` failed its undo
assertion: it expected `waveMesh.yellow` to return to `0.317`, but the loaded
node reports `0` immediately after `openGraph`, before any editor action. The
source `with-spies.cyclegraph` contains `yellow: 0.317`. Diagnostic reports:
`/private/tmp/causal-morph-initial-report.json` and
`/private/tmp/causal-morph-diagnostic-report.json`.

Current status: open; inspect graph load or parameter normalization. The
fixture's undo value should not be changed until that discrepancy is resolved.

## P3: Reverb preview can remain changed after a second no-op gesture

On 2026-09-18, a diagnostic native sequence dragged Reverb size, committed,
undid, reopened the editor, then dragged damping. The second drag changed the
local spectrogram, but the durable Reverb parameters stayed at their initial
values and a subsequent undo left the changed preview visible. A separate
fresh-graph damping drag/commit/undo fixture passes. Report:
`/private/tmp/causal-reverb-kernel-size-damp.json`.

Current status: open; inspect gesture lifecycle and editor rebind after undo.

## P3: Trimesh expanded control-region count test expects an old layout

On 2026-09-18, the broad Trimesh node test filter passed 64 of 65 cases; one
assertion in `TestTrimeshNodeDsp.cpp` expected 22 control regions and observed
28. The focused vertex delta and guide-gain gesture tests passed. Log:
`/private/tmp/causal-trimesh-delta-tests.log`.

Current status: open; reconcile the control layout contract and its assertion.

## P3: Cycle 1 FileManager assertion during a mismatched automation launch

On 2026-09-18, the Cycle V2 guide-gain audit fixture was accidentally run
through the wrapper's default Cycle 1 app. The commands were unsupported and
the filtered log emitted `JUCE Assertion failure in FileManager.cpp:174`.
The same fixture passed when `CYCLE_APP_PATH` and `CYCLE_PROCESS_NAME` targeted
Cycle V2. The mismatched-run log was replaced by the successful rerun; use
the wrapper defaults with `/tmp/causal-trimesh-guide-audit.json` to reproduce.

Current status: open in Cycle 1; unrelated to the Cycle V2 gesture change.

## Addressed: Unmapped saxophone mod wheel edited Trimesh blue morphs

On 2026-09-18, the Live keyboard wheel on `saxophone.cyclegraph` changed
`timeLayer1.blue` from `0` to `0.897637784` and marked the document dirty,
although the attached Modulation Triple used `inverseVelocity` for blue.
The preview command had applied red/blue values to every Trimesh and Envelope
without checking each compiled input source. Baseline report:
`/private/tmp/cycle-v2-sax-wheel-live-before.json`.

Keyboard morph edits now target only axes sourced from key scale or the mod
wheel. A wheel with no mapped source updates its keyboard position without a
graph edit or preview job. The native fixture passes with blue still `0`, a
clean document, two unchanged spy sums, and zero worker/configuration stages;
the mapped-wheel fixture still passes. Reports:
`/private/tmp/cycle-v2-sax-wheel-live-after.json` and
`/private/tmp/cycle-v2-honerism-wheel-mapping-after.json`.

Current status: addressed; `cycle-v2-agent-saxophone-unmapped-wheel.json`
guards the regression.
