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

## One-Shot Fixture Runs

Build the standalone app first if the touched code has not been built:

```sh
cmake --build --preset standalone-debug
```

Run one fixture through the LaunchServices-aware wrapper:

```sh
scripts/run_cycle_agent.sh \
    scripts/fixtures/cycle-agent-modmatrix-dialog.json \
    /private/tmp/cycle-agent-modmatrix-dialog-report.json \
    /private/tmp/cycle-agent-modmatrix-dialog-logs.txt
```

Run GUI automation fixtures sequentially. The wrapper launches Cycle through
LaunchServices and writes one report/log pair per app instance; concurrent runs
can race over the foreground app process and leave one wrapper waiting for a
missing report.

Inspect artifacts in this order:

1. Report JSON: command success/failure, snapshot, exported command data.
2. Filtered log: startup, preset open, warnings, assertions, crash handling.
3. Raw log: only when the filtered log omits needed detail.
4. `.ips` crash report beside the log: only when the wrapper reports one.

The wrapper writes:

- `<report>.json`,
- `<log>.txt`,
- `<log>.txt.raw`,
- `<log>.txt.ips` when a fresh Cycle crash report is found.

The runner exits nonzero when a report is missing or when any command returns
`"ok": false`, unless `CYCLE_AGENT_ALLOW_FAILURES=1` is set.

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
- `captureAudio`: render scheduled MIDI events offline, optionally write a WAV,
  and return amplitude metrics for assertions.
- `exportState`, `exportPreset`, `exportMeshState`: export scoped JSON, reusing
  existing `Savable::writeJSON()` boundaries where available.
- `listMeshTargets`: discover mesh groups/layers and stable target ids.
- `meshSelectionGesture`: exercise real interactor selection behavior with
  synthetic mouse gestures.
- `selectVertex`, `addVertex`, `moveVertex`, `deleteVertex`: mesh mutation
  commands. Prefer gesture mode for e2e coverage of interaction code; reserve
  direct patch mode for preset generation.
- `pointer`: targeted mouse events against a registered component.
- `waitForIdle`: fixed message-loop drain/delay when a command needs UI updates
  to settle.

When the goal is e2e test coverage, prefer commands that drive the actual UI
or interactor path. Direct state mutation is acceptable for fixture setup or
future programmatic preset generation, but it can skip important code.

## Screenshots

For ordinary JUCE panels, use app-side `screenshot` with a registered area or
target. This auto-crops to the resolved component bounds.

For OpenGL-backed panels, app-side component snapshots may be blank. Use the
runner-side OS crop path:

```sh
CYCLE_PREFLIGHT_SCREEN_CAPTURE=1 \
CYCLE_OS_SCREENSHOT_AREA=AreaWfrmWaveform3D \
CYCLE_OS_SCREENSHOT_PATH=/private/tmp/cycle-agent-waveform3d-os.png \
    scripts/run_cycle_agent.sh \
    scripts/fixtures/cycle-agent-waveform3d-os-screenshot.json \
    /private/tmp/cycle-agent-waveform3d-os-report.json \
    /private/tmp/cycle-agent-waveform3d-os-logs.txt
```

The fixture must inspect the target first so the report contains
`screenBounds`; the wrapper passes those bounds to `screencapture -R`.

On macOS, the wrapper preflights Accessibility permission by default and
preflights Screen Recording when OS screenshots are requested. If permission is
missing, it opens the relevant System Settings pane and exits with instructions.

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
sample rate, duration, event count, and output path. Use the numeric threshold
fields for first-pass assertions; reserve perceptual or similarity checks for a
separate external analysis step that consumes the WAV artifact.

## Crash And Assertion Triage

If Cycle crashes before writing a report, `scripts/run_cycle_agent.sh` can:

- temporarily suppress CrashReporter UI by setting `com.apple.CrashReporter`
  `DialogType=none` for the duration of the run, then restoring the original
  value,
- detect and dismiss the macOS "Cycle quit unexpectedly" dialog,
- copy the newest matching DiagnosticReports `.ips` file next to the logs,
- append the crash report header to the raw log.

For crash bugs, first make a minimal fixture that reaches the failing UI action.
Then rerun the same fixture after the fix. The Modulation Matrix regression
fixture is a good model:

```sh
scripts/run_cycle_agent.sh \
    scripts/fixtures/cycle-agent-modmatrix-dialog.json \
    /private/tmp/cycle-agent-modmatrix-dialog-report.json \
    /private/tmp/cycle-agent-modmatrix-dialog-logs.txt
```

If the `.ips` or filtered log identifies a useful breakpoint, rerun the same
fixture under LLDB instead of changing the normal unattended flow:

```sh
scripts/run_cycle_agent_lldb.sh \
    scripts/fixtures/cycle-agent-modmatrix-dialog.json \
    /private/tmp/cycle-agent-modmatrix-breakpoints.lldb \
    /private/tmp/cycle-agent-modmatrix-lldb-report.json \
    /private/tmp/cycle-agent-modmatrix-lldb-app.log \
    /private/tmp/cycle-agent-modmatrix-lldb.log
```

Example breakpoint file:

```lldb
breakpoint set --name ModMatrixPanel::resized
breakpoint set --name Dialogs::showModMatrix
```

The LLDB runner starts `lldb` in `process attach --waitfor` mode, launches
Cycle through `open ... --args`, sources the breakpoint file after attach, then
continues. If the process stops at an assertion, signal, or breakpoint, inspect
live state with commands such as:

```lldb
bt
frame variable
expr inputs.size()
expr outputs.size()
```

Keep `continue` out of the breakpoint file unless you deliberately want custom
control; the wrapper continues automatically by default. Set
`CYCLE_LLDB_AUTO_CONTINUE=0` to attach and source breakpoints without starting
the automation run. Set `CYCLE_SUPPRESS_CRASH_DIALOG=0` if you explicitly want
macOS to show its crash dialog during a runner or LLDB session.

## Long-Running Sessions

Use a one-shot fixture for most bug work. Use session mode when an agent needs
interactive exploration without relaunching Cycle for every command.

Start Cycle with a Unix-domain socket:

```sh
scripts/run_cycle_agent_session.sh \
    /private/tmp/cycle-agent.sock \
    /private/tmp/cycle-agent-session.log
```

Send one command per client connection:

```sh
scripts/cycle_agent_session.py \
    /private/tmp/cycle-agent.sock \
    -c '{"command":"snapshotState"}'
```

The app still launches as a GUI bundle first. The socket is only the command
transport after normal startup and initial preset loading.

## Smoke Suite

Run all repository fixtures when changing automation infrastructure:

```sh
scripts/run_cycle_agent_smokes.sh
```

The smoke runner launches one Cycle `--agent-session`, sends each fixture's
commands through that socket, writes one aggregate report, and keeps one app log
for the whole run. If a command fails, the runner records the failure, skips the
rest of that fixture, resets before the next fixture, and continues. The reset
step dismisses transient UI such as the Mod Matrix desktop panel before opening
the next preset.

Artifacts default to `/private/tmp/cycle-agent-smokes`. Override with:

```sh
CYCLE_AGENT_ARTIFACT_DIR=/private/tmp/my-cycle-smokes scripts/run_cycle_agent_smokes.sh
```

Run a focused subset by fixture name:

```sh
CYCLE_AGENT_SMOKE_FIXTURES="readonly pullout-hover" scripts/run_cycle_agent_smokes.sh
```

Resume from an existing aggregate report, skipping fixture names already
recorded:

```sh
CYCLE_AGENT_SMOKE_RESUME=1 scripts/run_cycle_agent_smokes.sh
```

OpenGL OS screenshot smoke is opt-in:

```sh
CYCLE_AGENT_SMOKE_OS_SCREENSHOT=1 scripts/run_cycle_agent_smokes.sh
```

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
