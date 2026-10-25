# Cycle Agent Automation Runbook

This runbook is for agents using Cycle's in-app automation to reproduce UI
bugs, write focused regression fixtures, inspect state, and capture screenshots.
For architecture and command-model background, see
`docs/ADR/007-cycle-agentic-automation.md` and
`docs/TDD/cycle-agentic-automation.md`. In general, it's not required reading.

## When To Use It

Use Cycle agent automation for UI-heavy work where normal unit tests cannot
exercise the real workflow:

- dialogs and menu actions,
- component visibility, enabled state, bounds, and target lookup,
- morph sliders, tool buttons, layer toggles, and effect controls,
- mesh target discovery, mesh state export, selection, and pointer gestures,
- OpenGL panel screenshots,
- crash/assertion reproduction from an ordered UI workflow.

Prefer a focused fixture over the full smoke suite while debugging. Run the
full suite only when changing shared automation behavior or before handing off
a broad automation change.

## Coordinated Fixture Runs

Use the coordinator with an explicit product. `run` builds the exact standalone
target incrementally, launches through NSWorkspace, validates the returned PID,
and reuses that workspace's healthy session. That session may also be in use by
the user. Use `--clean` for automated fixtures while the user is working in the
app: it creates an isolated run PID and socket and leaves the persistent session
intact. Use only the socket returned by that run; do not reuse a socket from an
earlier artifact. `--no-build` opts out of freshness
building. Default reports and screenshots live under
`.context/cycle-automation/<product>/<run-id>/`.

```sh
scripts/cycle_automation.py run --product cycle1 \
    --fixture scripts/fixtures/cycle-agent-modmatrix-dialog.json

scripts/cycle_automation.py run --product cycle-v2 \
    --fixture scripts/fixtures/cycle-v2-agent-readonly.json --json

scripts/cycle_automation.py capture --product cycle-v2
scripts/cycle_automation.py status
scripts/cycle_automation.py stop --product cycle-v2 --owned
```

`run_cycle_agent.sh` and `run_cycle_v2_agent.sh` remain product aliases. A
conflicting lifecycle or foreground operation fails with the owning workspace,
PID, and run ID; pass a bounded `--wait` when waiting is intentional. Semantic
fixtures on separate registered instances can run concurrently. Foreground
capture takes the machine-wide foreground lease.

For exploratory Cycle V2 work, keep one connection open instead of writing a
new script for each command:

```sh
scripts/cycle_automation.py repl --product cycle-v2 --no-build
```

Run this in a PTY and send successive lines through its stdin. Each line is a
JSON command or a command name such as `ping` or `snapshotState`. `:exit` closes
the client; it leaves the app running. `:reconnect` opens a fresh connection and
never replays a previous command. Command responses include distinct request IDs.
The REPL holds no foreground lease: native input and capture still use the
coordinated `exec --foreground` / `capture` routes.

Repeatable Python workflows use the same client and command schema:

```python
from cycle_automation_support.session import Session

with Session(socket_path, timeout=120) as session:
    state = session.command({"command": "snapshotState"})["result"]
    controls = session.command({"command": "inspectNodeControls", "nodeId": "voice"})["result"]
```

One `Session` is sequential and retains partial response lines between requests.
Disconnects, timeouts, malformed responses, and mismatched IDs close the
connection and raise; a command may already have run, so the client never retries
it automatically. An explicit reconnect or a later command opens a new socket.
Cycle V2 accepts up to sixteen clients, buffers at most 16 MiB of queued request
bytes per client, and bounds each response at 64 MiB. Idle or slow clients do not
block other connections. Existing one-shot clients remain supported.

Inspect artifacts in this order:

1. Report JSON: command success/failure, snapshot, exported command data.
2. Filtered log: startup, preset open, warnings, assertions, crash handling.
3. Raw log: only when the filtered log omits needed detail.
4. LLDB transcript for breakpoint runs, or a crash report when available.

## Direct Live Authoring

Keep one socket open and inspect the current model before editing. Capabilities
advertise `shapeAuthoring`, `guideAuthoring` and `previewControllers` contracts.
`inspectTrimesh` and `inspectEnvelope` accept `nodeId` without opening an editor.
They report document/model revisions, geometry, field units and bounds, Guide
attachments and authored/effective morph positions. Use those revisions for one
atomic edit; stale requests are rejected without changing history.

| Command | Edit representation |
| --- | --- |
| `editTrimesh` | `vertex` with vertexIndex/field/value; `cubeCurve` with cubeIndex/value; `guideGain` with vertexIndex/field/value. |
| `editEnvelope` | Stable cubeId, corner and field/value; `guideGain`; loop/sustain states. Supply dimensionCount and linkedAxes yellow/red/blue. |
| `editMeshTopology` | Insert cubeIndex/phase/amp/curve; remove Trimesh cubeIndex or Envelope cubeId. Addresses follow the sequential candidate. |
| `setPreviewController` | Integer controller/value 0..127 on audition channel 1, even with keyboard hidden. |
| `inspectPreviewControllers` | All 128 ephemeral values and document/audio-plan counters. |

Value batches allow 64 edits; topology batches allow 16. Trimesh ordinals are
revision-scoped. Envelope cube IDs survive edits to other cells. Re-inspect after
topology changes. New poles start constant over the full morph range and can be
refined with addressed values. Guide targets follow retained cells through index
changes; removed cells lose their attachments in the same undo step.

Controller auditions leave preset bytes and undo history unchanged. The expanded
editor displays effective coordinates without publishing them as authored morph.
`sendMidi` also accepts `type: "controller"`, channel 1..16, controller/value
0..127 for raw delivery; that command does not synchronize preview readback.
Keep a pinned Spy/reference for comparison, make one targeted edit, audition and
undo when needed. Refine the running preset directly through these commands.

Trimesh's primary morph rail navigates the local slice without a graph edit,
including on release. Secondary rails and cube gestures keep movement local and
commit saved fallback coordinates on release. An unchanged live controller must
not overwrite manual editor navigation; a changed controller resumes preview on
its axis. Explicit `setNodeParameter` remains available for saved fallback edits.
Run `python3 scripts/test_cycle_v2_trimesh_interaction_publication.py` for the
actual-app pointer/readback, edit-ID, document, surface and commit/undo checks;
hosted editor tests also assert zero copies and serialization during movement.

Run `python3 scripts/test_cycle_v2_envelope_interaction_feedback.py` for actual-app
Envelope hover and Shift-box-selection checks. The report compares edit/model
revisions and rasterizer, slice, panel-preparation, layout and surface counters;
unchanged graph revisions alone do not establish cheap pointer feedback.
Add `--morph` to check Envelope cube/plane coordinates across press, held updates,
release, editor reopening, undo/redo, controller takeover and document reset.
The cursor, effective panel position and saved coordinates are checked separately;
publication alone cannot establish that a morph gesture stays where released.
`python3 scripts/test_cycle_v2_native_edit_smoke.py envelope-feedback` exercises
physical highlights, selection, reduced-plane cursor movement and both surface
orientations. Set `CYCLE_V2_NATIVE_CAPTURE_DIR` under `.context/` to retain normal
and compact captures for every primary-axis colour.

## Whole Model Import And Compatibility

Use `inspectSynthesisCapabilities` to discover `authoring` bounds and each model's
`authoringReplacement` support. `inspectNodeControls` returns the current typed
model and `documentRevision` without opening an editor. Submit one synchronous
`applyNodeEdits` request with that expected document revision and an `edits` array:

```json
{
  "command": "applyNodeEdits",
  "expectedDocumentRevision": 12,
  "edits": [{
    "nodeId": "shape",
    "parameters": [{"id": "pre", "value": "0.65"}]
  }]
}
```

A replacement additionally supplies `model` using its inspected codec envelope and
`expectedModelRevision`. Increment both outer and inner model revisions when the
codec has an inner revision. Values are stored strings; use existing physical
conversion discovery when authoring in physical units. A changed batch publishes
once and uses one undo step. Rejection preserves graph/document/history state;
active gestures and stale revisions are rejected. Model validation covers safe
structure and factory-compatible coordinates; musical quality and gesture-specific
collision eligibility require separate evaluation.

`applyNodeEdits` remains available for explicit whole-model import and existing
repeatable fixtures. The historical candidate demonstration uses that lifecycle
and the performance-suite renderer:

```sh
python3 scripts/create_cycle_v2_candidates.py --no-build \
    --output-dir .context/semantic-candidates
```

The default example uses existing `waveshaper`, `timeLayer1` and `voice` nodes in
`old/sat-bass.cyclegraph`; custom baselines must retain those node contracts.
It saves two candidate presets, command evidence and suite WAV/results, checking
undo/redo, reload, rejection isolation, distinct nonzero audio and repeatability.
Build the exact app first; it attaches through the canonical coordinator/session.

## Group Explanations And Information Cards

`inspectSynthesisCapabilities.explanations` advertises the supported scopes and
canonical commands. Use `scope: "group"` with `groupId` for group rationale;
node targets require `nodeId`, and preset targets carry neither ID. The same
`setExplanation` / `inspectExplanation` commands serve all three scopes.

`setGroupInformationCard` takes group scope, a boolean `visible`, and an optional
`offset: {"x": 0, "y": 260}` in world units relative to the group's member-bounds
origin. Without an offset, Show reuses an enabled placement or derives a default.
Hide retains the rationale. Clearing rationale retains placement but suppresses
its view. `openExplanationEditor` opens the same local-draft editor for any scope.
No note/card command changes the synthesis graph revision or prepares audio.

Run the focused exact-instance integration check after building the app:

```sh
python3 scripts/test_cycle_v2_group_explanations.py --no-build \
    --output-dir .context/group-explanation-check
```

It creates a group through existing canvas gestures, checks strict rejection,
shared editing, two local card-drag previews, commit/undo/redo, collapse,
hide/clear and reload. Its saved graph and screenshots stay in the output folder.
Native AppKit/JUCE delivery still requires the native smoke route below.

## Native macOS Edit Smoke

In-app `pointer` commands call component methods directly and do not verify
AppKit/JUCE event delivery. For CycleV2 node-editor acceptance, build the app
and run the external native smoke:

```sh
cmake --build --preset standalone-debug --target CycleV2 --parallel 10
scripts/test_cycle_v2_native_edit_smoke.py
```

The smoke launches one persistent app instance by its exact configured `.app`
path so another worktree's Cycle build cannot receive its native gestures. It
activates and raises that exact PID through NSRunningApplication/Accessibility, resolves target screen bounds,
and drives `cliclick` through macOS. It covers Waveshaper add, move, reshape,
delete, and re-add, followed by Trimesh add, collision rejection, move,
vertex-parameter editing, morph editing, delete, and re-add. Each edit settles
for at most 80 ms; do not replace these gestures with in-app pointer commands.
Run `python3 scripts/test_cycle_v2_native_edit_smoke.py envelope-markers` for
Envelope marker selection, pointer departure, Loop/Sustain publication and Undo.
Use the `cube-faces` sequence for all three cube faces in both mesh editors,
paired-axis movement, held coordinates, geometry preservation and Undo.
Use `envelope-scales` for native Volume Linear/Logarithmic clicks and cursor captures,
hidden scale controls/targets in other purposes, and their return on Volume.

The coordinator writes `report.json`, `app.log`, `app.log.raw`, and `run.json`
under the run directory. It exits nonzero for failed commands unless
`--allow-failures` is set.

## Fixture Authoring Pattern

Fixtures live in `scripts/fixtures/*.json`. Keep them small and named after the
behavior they prove.

Useful command sequence for a UI bug:

```json
{
  "commands": [
    {
      "command": "action",
      "actionType": "ShowArea",
      "area": "AreaModMatrix",
      "waitForIdle": true,
      "idleDelayMs": 300
    },
    {
      "command": "inspectTargets",
      "area": "AreaModMatrix"
    }
  ],
  "quit": true
}
```

Use `inspectTargets`, `inspectTree`, and `listAssertionPaths` before inventing
new selectors. They reveal the current registry names, rectangles, component
classes, state paths, and assertion-compatible values. If a target does not exist
in the tree, but exists on the UI, and you want to trigger it, wire it up! 

Use `assertState` or `assertTarget` once the path is known. Do not validate by
visually reading a screenshot if a state assertion can prove the behavior.

## Choosing Commands

### Live preset authoring

Refine presets with targeted semantic commands against the running document.
Do not use preset generators, patched graph JSON or reload cycles for new sound
design. Inspect, edit, compare the pinned live Spy/reference, and retain or undo.
When a needed edit is missing, expose its existing command owner through the
socket before proceeding. Saved graphs are explicit checkpoints.

The persistent socket also accepts raw JSON lines without a Python authoring
client. Hold the coordinator's exact-instance lease while using `nc`:

```sh
python3 scripts/cycle_automation.py exec --product cycle-v2 --no-build -- \
    /bin/sh -c 'exec /usr/bin/nc -U "$CYCLE_AGENT_SESSION"'
```

Send one request at a time, with a distinct ID, and inspect its response before
the next mutation. Transport reconnects must not replay a failed edit:

```json
{"id":"guide-1","command":{"command":"inspectGuideCurve","guideId":"reedOnsetPhase"}}
{"id":"guide-2","command":{"command":"editGuideCurve","guideId":"reedOnsetPhase","expectedDocumentRevision":7,"expectedModelRevision":1,"edits":[{"op":"set","vertexId":1,"y":0.507}]}}
```

Use the revisions from inspection, rather than copying these example values.
`inspectSynthesisCapabilities.guideAuthoring` advertises commands and limits.
`inspectGuideCurve` returns resource/model/document revisions, canonical points,
controls and resolved assignments. `editGuideCurve` supports partial `set`,
`insert` and `remove` point edits, bounded to 64 per atomic request. Point IDs
are stable; inserting allocates an ID visible on the next inspection. Coordinates
and sharpness are in [0,1]; y=0.5 is neutral Guide displacement.

`createGuideCurve` requires the current `expectedDocumentRevision` and returns
the new Guide ID. `assignGuideCurve` / `detachGuideCurve` additionally take
`nodeId`, `expectedModelRevision`, `selectionIndex` and `field` (`time`, `red`,
`blue`, `phase`, `amp` or `curve`); assignment also takes `guideId`. The index
addresses a Trimesh vertex array entry or an Envelope cube array entry, and
resolves every owning rail through the same rules as the editor. It does not
refer to the current selection. Inspect actual assignments after attachment.
These operations work with all editors closed and use normal undo/redo.

### General automation

Common command families:

- `action`: semantic `CycleTour` action such as `ShowArea`, `HideArea`,
  `SwitchToTool`, `Enable`, or `Disable`.
- `inspectTargets`: resolve registered areas/targets and return component
  state plus local and screen bounds.
- `inspectTree`: dump a bounded component tree and registered target side
  table for coordinate planning.
- `snapshotState`: capture document, morph, tool, panel, and current mesh
  summary state.
- `listAssertionPaths`: discover dot paths for `assertState` and
  `assertTarget`.
- `setControl`: set sliders, combo boxes, buttons, and target-backed controls.
- `screenshot`: capture a registered panel/target using app-side component
  rendering.
- `openGLDiagnostics`: poll OpenGL-backed panels on their GL thread and return
  context attachment, render counts, context create/close counts, and the last
  recorded GL error. Use this before/after suspect session gestures when panels
  go black without an obvious automation failure.
- `resetCanvasPerformance`, `inspectCanvasPerformance`: delimit and capture a
  canvas measurement window containing trigger frequency/cost, repaint latency
  and coalescing, and JUCE/OpenGL frame-duration distributions. Use
  `cycle-v2-agent-canvas-performance.json` as the stable before/after baseline
  for canvas optimization work. For preset loading, use
  `cycle-v2-agent-preset-load-latency.json`: its `presetLoad` object reports
  synchronous return, first full canvas paint, and posted message turns
  relative to load entry; `slowestNodeTiles` identifies cold node paint costs.
  These posted turns are queue markers, not proof that the entire queue is idle.
- `resetAudioPerformance`, `inspectAudioPerformance`: start an opt-in audio
  measurement generation and inspect callback/deadline distributions, phase
  durations, workload, overruns, and telemetry drops. Use `sendMidi` with
  `noteOn`, `noteOff`, or `allNotesOff` for stable polyphony workloads.
- `captureAudio`: render scheduled MIDI events offline, optionally write a WAV,
  and return amplitude metrics for assertions.
- `exportState`, `exportPreset`, `exportMeshState`: export scoped JSON, reusing
  existing `Savable::writeJSON()` boundaries where available.
- `exportPresetFile`: decode and migrate a `.cyc` directly to canonical JSON
  without applying it to the live document. Use this for library migration and
  inspection when editor, updater, rasterizer, and audio side effects are not
  part of the behavior under test.
- `listMeshTargets`: discover mesh groups/layers and stable target ids.
- `meshSelectionGesture`: exercise real interactor selection behavior with
  synthetic mouse gestures.
- `selectVertex`, `addVertex`, `moveVertex`, `deleteVertex`: mesh mutation
  commands. Prefer gesture mode for e2e coverage of interaction code; reserve
  direct patch mode for preset generation.
- `pointer`: targeted mouse events against a registered component. A move can
  supply `expectedCursor` (`pointingHand`, `leftRightResize`, `upDownResize`,
  `move`, `crosshair`, or `normal`) to fail at the actual event target when its
  hover affordance regresses; successful pointer results also report `cursor`.
- `key`: dispatch a named keyboard event (`tab`, `shiftTab`, arrow directions,
  `return`, `space`, `escape`, or `delete`) to a registered area and report
  whether its real component interaction path handled the event.
- `waitForIdle`: fixed message-loop drain/delay when a command needs UI updates
  to settle.

## Cycle V2 Performance Regression Loop

Build and run the focused fixture against the exact standalone target. The
coordinator builds by default and records the executable fingerprint and PID.
Use `--clean` when the fixture must start with a fresh process; this creates a
run-scoped instance and leaves the workspace's persistent session intact.

```sh
scripts/cycle_automation.py run --product cycle-v2 --clean \
    --fixture scripts/fixtures/cycle-v2-agent-preset-load-latency.json
```

Compare reports from the same build, preset contents, window size, cache state,
and power source. `waitForIdle` marks queue progress rather than visibility.
For expanded Trimesh morph gestures, run the live and on-release fixtures
separately and inspect their operation-count assertions.

## Screenshots

For ordinary JUCE panels, use the app-side `screenshot` command with a
registered area or target. The coordinator holds the foreground lease for
fixtures containing app-side screenshots. For composited or OpenGL content,
use coordinator capture. Its default ScreenCaptureKit path selects one window by the recorded
PID and reports its window ID, bounds, and pixel dimensions. `--rect` captures
a desktop region after focusing that same PID. Both methods hold the foreground
lease conservatively.

```sh
scripts/cycle_automation.py capture --product cycle-v2
scripts/cycle_automation.py run --product cycle1 \
    --fixture scripts/fixtures/cycle-agent-waveform3d-os-screenshot.json \
    --area AreaWfrmWaveform3D --screenshot .context/waveform3d.png
```

The fixture must inspect the area before a report-based crop. Accessibility
and Screen Recording permissions belong to macOS privacy controls; an
AppleScript or LaunchServices error alone does not identify a missing privacy
permission.

## UI Video

Pass `--video <path>` to `run`, or set `CYCLE_OS_VIDEO_PATH` with a product
wrapper. The recorder selects the window by the registered PID. Recording
holds the foreground lease for the fixture duration and stops when the fixture
finishes. The default frame rate is 30 fps.

```sh
scripts/cycle_automation.py run --product cycle-v2 \
    --fixture scripts/fixtures/cycle-v2-agent-trimesh-guided-vertex-insertion.json \
    --video .context/trimesh-gesture.mp4
```

## Audio Capture

Use `captureAudio` when a bug or regression needs proof from the rendered
signal rather than only UI state. The command temporarily suspends the live
standalone audio callback, prepares the synth for an offline render, sends the
scheduled MIDI events, writes a WAV when `path` is supplied, and restores the
previous audio sample rate and buffer size.

Minimal fixture command:

```json
{
  "command": "captureAudio",
  "path": "/private/tmp/cycle-agent-note.wav",
  "durationMs": 1500,
  "sampleRate": 44100,
  "blockSize": 512,
  "channels": 2,
  "events": [
    { "type": "noteOn", "timeMs": 0, "note": 60, "velocity": 0.8 },
    { "type": "noteOff", "timeMs": 850, "note": 60 }
  ],
  "peakGreaterThan": 0.0001,
  "rmsGreaterThan": 0.00001,
  "peakLessThan": 1.5
}
```

Supported event types are `noteOn`, `noteOff`, `controller`, `pitchWheel`, and
`allNotesOff`. The report data includes `peak`, `rms`, per-channel metrics,
`maxNoteOffSecondDifference`, `terminalDelta`, sample rate, duration, event
count, and output path. The corresponding event-specific thresholds are
`noteOffSecondDifferenceLessThan` and `terminalDeltaLessThan`. Use the numeric
threshold fields for first-pass assertions; reserve perceptual or similarity
checks for a separate external analysis step that consumes the WAV artifact.

For the Cycle 1 OohAah release regression, run
`scripts/test_cycle1_ooh_aah_release.py`. It renders 50, 150, 400, and 800 ms
notes in separate application processes and asserts both the MIDI note-off and
release-completion boundaries.

For same-instance voice reuse, run
`scripts/test_cycle1_ooh_aah_repeat_note.py`. Each case renders two equal notes
with enough silence for the first release to finish, then compares their 5 ms
RMS amplitude envelopes. The default matrix covers 100, 400, and 800 ms notes
at both 44.1 and 48 kHz.

For Cycle 1 spectral voice preparation and phase-power regressions, run
`scripts/test_cycle1_spectral_phase.py`. It checks dry Acidic and Anasound2
output, then verifies that toggling Baroque Flute's phase layer preserves
steady-state power. The default matrix covers both 44.1 and 48 kHz.

For saved Cycle V2 presets, use
`scripts/render_cycle_v2_performance_suite.py` to render a bounded suite of
notes, repeated notes, timed CC/pressure and shared library patterns. It delegates
to the coordinator and existing `captureAudio`, records preset/performance/audio
fingerprints, and verifies repeated PCM. See
[Performance Suites](cycle-performance-suites.md) for the manifest and artifact
contract; `--plan` validates without starting an app.

For the bounded expressive-saxophone pilot, use the existing suite and reference
owners through two separate stages:

```sh
python3 scripts/run_cycle_v2_saxophone_study.py --stage objective --output-dir .context/sax-pilot
python3 scripts/run_cycle_v2_saxophone_study.py --stage audit --output-dir .context/sax-pilot
```

Build the exact app first; `--no-build` reuses that build. Plot generation uses
the host dependencies in `scripts/requirements-auditions.txt`. The objective
stage saves the fixed slower-peak intervention and freezes its identities before
the audit renders reserved performance combinations. Never reuse an objective
output directory or change frozen inputs to continue an audit. The one growl
recording has unverified musical labels: the report remains unpitched and does
not validate saxophone expression across the synthetic test performances. See
`cycle-v2/content/studies/expressive-sax/README.md` and the pilot TDD for coverage,
control timing and the still-open acoustic/search gate.

The follow-up VCSL calibration uses downloaded, pinned CC0 originals at native
48 kHz. Supply the corpus directory containing `download-manifest.json`,
`selection.json`, `LICENSE` and the original `audio/` tree:

Use `cycle-v2/content/studies/expressive-sax/vcsl-corpus/README.md` to reconstruct
that corpus from the tracked acquisition records when starting from a fresh
checkout.

```sh
python3 scripts/run_cycle_v2_saxophone_calibration.py --stage objective --corpus-root /path/to/vcsl --output-dir .context/sax-calibration
python3 scripts/run_cycle_v2_saxophone_calibration.py --stage audit --corpus-root /path/to/vcsl --output-dir .context/sax-calibration
```

The fixed plan reserves four D3 parents before decoding them. Objective hashes
all twelve files, compares eight sources and freezes a single pitch-envelope
ablation; audit decodes the reserved four with no candidate edits. Reference,
intervention and pitch-trace reports retain raw timing and confidence. Pitch
frames describe the effective library path, not raw detector observations.
Source velocity and control settings remain unknown. See
`docs/TDD/cycle-v2-saxophone-reference-calibration.md`; straighter tracked pitch
alone does not establish timbre, taste, economy or an expressive saxophone.

To author the core articulation preset and its embedded 28-second MIDI/CC phrase,
use the canonical semantic editor and the existing performance/audition owners:

```sh
python3 scripts/create_cycle_v2_saxophone_articulation.py --no-build --output-dir .context/sax-articulation
```

Build `CycleV2` first and install `scripts/requirements-auditions.txt` in the
invoking Python environment. Each output directory is new. The driver verifies
shape undo/redo, save/reload, invalid sequence rejection, independent controls,
connected notes and exact repeated captures. Its saved graph contains the
controller lanes; load it and use the keyboard's Play button to hear the phrase.
The report provides raw and RMS-matched comparisons plus the complete phrase.
These are technical control demonstrations; acoustic growl and human taste
still require listening. See `cycle-v2-expressive-saxophone-articulation.md` in
`docs/TDD/` for the live-envelope cadence and legato contract.

## Crash And Assertion Triage

Use a minimal fixture and inspect its report, `run.json`, and scoped app logs.
A process that exits before session readiness is reported by its exact PID.
The coordinator leaves artifacts in place on failure and never dismisses or
quits another workspace's application by process name.

For breakpoint work, the LLDB alias attaches to a run-scoped PID and records
its transcript in the run directory:

```sh
scripts/run_cycle_agent_lldb.sh \
    scripts/fixtures/cycle-agent-modmatrix-dialog.json \
    /private/tmp/cycle-agent-modmatrix-breakpoints.lldb
```

The breakpoint file may contain normal LLDB commands such as
`breakpoint set --name Dialogs::showModMatrix`. The coordinator attaches to
the exact PID, continues, runs the fixture, and stops its scoped instance.

## Long-Running Sessions

`start` creates or reuses a healthy workspace session. `status` shows its
validated PID and socket. Send a command through the printed socket, then use
`stop --owned` when done:

```sh
scripts/cycle_automation.py start --product cycle1 --json
scripts/cycle_agent_session.py <printed-socket> -c '{"command":"snapshotState"}'
scripts/cycle_automation.py stop --product cycle1 --owned
```

For coordinated exploratory scripts that drive native input, use `exec` with
`--foreground`; it provides `CYCLE_AGENT_SESSION` and `CYCLE_APP_PID` to the
child while holding the instance and foreground leases.

## Smoke Suite

Run focused fixtures while debugging. The Cycle 1 suite wrapper delegates its
session lifecycle to the coordinator:

```sh
CYCLE_AGENT_SMOKE_FIXTURES=readonly CYCLE_AGENT_SMOKE_RESET_PRESET= \
    scripts/run_cycle_agent_smokes.sh
```

The aggregate report defaults to a run-scoped directory under
`.context/cycle-automation/cycle1/`. The standard reset preset is `OohAah`;
override it when a fixture needs another baseline. The
Cycle 2 smoke alias reads its fixture manifest and records one report per
fixture under its own run directory.

## Practical Rules

- Use the wrapper scripts; do not launch `Cycle.app/Contents/MacOS/Cycle`
  directly for GUI automation on macOS.
- Keep fixtures focused and reproducible; avoid full smokes during tight bug
  loops.
- Read filtered logs before raw logs.
- Prefer semantic actions and state assertions over absolute screen
  coordinates.
- Use `inspectTree` and target bounds when pointer coordinates are needed.
- Preserve failing fixtures as regression coverage when they are small and
  stable.
