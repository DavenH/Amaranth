# Cycle V2 UI Bug Notes

## P2: Broader Trimesh tests retain a stale control-region expectation

The 2026-09-18 `CycleV2_tests '[trimesh]'` run passed the mapped pitch tests but
failed the control-region count (`28` versus `22`). The compact versus expanded
column equality (`96` versus `450`) was also stale after expanded pixel-width
sampling; its test now checks shared source data at their respective resolutions.
The control-region expectation remains open; log:
`/tmp/cycle-v2-trimesh-tests.txt`.

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

## P1: Empty trimesh vertex problem

Adding vertices to an empty trimesh node adds them at phase=0 regardless of where is clicked

## P1: the preset page overlays the trilinear mesh 2d editor

let's just hide it along with the rest of the elements that get hidden/dimmed on expand

## P2: The spy nodes get deeply dimmed when clicked for some reason

## P2: the 'out' spy node cannot be expanded

## P3: vertex selection rect doesn't have appropriate hover cursors on edges or center

## P2: Spy node preview content often doesn't agree with its expanded content

## P3: envelope nodes with a component curve do not render that componetn curve on preset load until expanded

## P2: envelope component curves with randomness do not get randomness reseeded based on unison voice

## P2: a graph update causes the playing audio note to stop

## P3: Cycle2's expanded 'out' spy node at C1 takes about 20x longer to render than Cycle 1's comparable DSP grids

## P2: Moving the view-axis' morph slider on trimesh invalidates and repaints the whole grid

But the grid content cannot change moving this slider - only the 2d editor should be updated.

##  P3: An 'empty' trimesh node starts with a default mesh.

## P3: Cycle 1 emits leaked-object assertions after preset-churn shutdown

The 2026-09-20 scalar-surface preset-churn fixture completed all seven
explicit preset loads, then emitted three `juce_LeakedObjectDetector.h:104`
assertions during application shutdown. No crash report was produced. Log:
`/private/tmp/cycle-surface-preset-churn-v1-logs.txt`.

Current status: open; the shutdown assertions are separate from
the repaired unison scratch-buffer overrun during preset rasterization.

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

## P3: Hosted Trimesh point-drag test retains two selected vertices

On 2026-09-22, the broad `[nodes][trimesh]` test filter failed in
`Hosted Trimesh point drag defers mesh replacement across publications`: the
post-gesture selection assertion expected one selected vertex and observed
two. This occurred while verifying the scalar-surface material; that change
does not touch Trimesh selection or graph publication. The focused surface
renderer test passed in the same build.

Current status: open; inspect selection reset across the hosted drag sequence.

## P3: Cycle 1 FileManager assertion during a mismatched automation launch

On 2026-09-18, the Cycle V2 guide-gain audit fixture was accidentally run
through the wrapper's default Cycle 1 app. The commands were unsupported and
the filtered log emitted `JUCE Assertion failure in FileManager.cpp:174`.
The same fixture passed when `CYCLE_APP_PATH` and `CYCLE_PROCESS_NAME` targeted
Cycle V2. The mismatched-run log was replaced by the successful rerun; use
the wrapper defaults with `/tmp/causal-trimesh-guide-audit.json` to reproduce.

Current status: open in Cycle 1; unrelated to the Cycle V2 gesture change.
It recurred during the 2026-09-20 scalar-surface visual audit because the saved
default preset name `ooh-aah` did not resolve before the fixture opened
`CalmingKeys`; the fixture itself completed and the OpenGL panel reported no
error. Log: `/private/tmp/cycle-scalar-v1-logs.txt`.
It also recurred during the 2026-09-29 Icy-hot program automation; both View menu
switches and GL validation passed afterward. Log: `/tmp/cycle-icy-hot.log`.


## P3: Organ graph load emits legacy Curve endpoint assertions

On 2026-09-20, opening `organ.cyclegraph` and the `timeLayer1` expanded
Trimesh editor emitted repeated `JUCE Assertion failure in Curve.cpp:56` and
`:57` messages. The automation commands completed, the shared OpenGL canvas
remained attached, and the expanded surface rendered. Log:
`/private/tmp/cycle-scalar-v2-expanded-logs.txt`.

Current status: open; unrelated to scalar-surface shader compilation or GL
resource lifetime.

## P1: Cycle 1 can crash rasterizing presets with more than ten unison voices

Five Cycle 1 crash reports from 2026-09-29 and 2026-09-30 terminate on the
main thread with `EXC_BAD_ACCESS` in
`Cycle::Rasterization::UnisonPhaseColumnRenderer::render`, during the
`VisualDsp` spectrogram refresh after a preset load. Reports:
`~/Library/Logs/DiagnosticReports/Cycle-2026-09-29-{145603,150303,153814}.ips`
and `Cycle-2026-09-30-{014550,015513}.ips` in the same directory.

The renderer allocates `phases` and `gains` for `maximumUnisonOrder` (10),
but loops to `Unison::getOrder(false)`, which returns the uncapped saved voice
count in individual mode. Several factory `.cyc` presets contain 11–36
individual voices. This is a likely stack-buffer overrun; the exact preset
loaded at each crash has not been established. This is a visualization/preset
rasterization path, not evidence of a scalar shader failure. The earlier
scratch-buffer repair noted above did not cover this fixed-array bound.

Current status: open. Size the renderer's scratch storage for the actual
individual voice count without truncating authored voices, then regression-test
loading and rasterizing a preset with more than ten voices.
