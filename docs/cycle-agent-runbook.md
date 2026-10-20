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
and reuses that workspace's healthy session. `--no-build` opts out of freshness
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

Inspect artifacts in this order:

1. Report JSON: command success/failure, snapshot, exported command data.
2. Filtered log: startup, preset open, warnings, assertions, crash handling.
3. Raw log: only when the filtered log omits needed detail.
4. LLDB transcript for breakpoint runs, or a crash report when available.

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
