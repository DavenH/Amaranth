# Cycle Agentic Automation TDD

## Overview

This document tracks the implementation plan and status for Cycle's agentic UI
automation work.

It implements [ADR 007](../ADR/007-cycle-agentic-automation.md).

## Problem Statement

Cycle needs a deterministic automation surface for UI-heavy workflows:

- bug reproduction,
- end-to-end tests,
- focused panel screenshots,
- scoped JSON state export,
- MIDI-triggered audio capture,
- future preset creation,
- future interactive agent sessions.

The app already has useful automation primitives in `CycleTour`, but those
primitives are coupled to tutorial callouts and are not exposed through a stable
test/agent control surface.

## Goals

- Run automation scripts against the standalone app through a normal GUI launch.
- Execute semantic commands on the JUCE message thread.
- Reuse `CycleTour` target/action mappings where they already exist.
- Reuse `Savable::writeJSON()` and `writeXML()` for scoped state export.
- Capture full-window and named-panel screenshots.
- Capture offline rendered audio from scheduled MIDI events.
- Produce structured JSON reports for tests and bug reports.
- Keep the command model transport-independent so MCP or socket control can be
  added later.

## Non-Goals

- Replacing `CycleTour` tutorials in the first pass.
- Implementing MCP before the command schema stabilizes.
- Making pointer-coordinate playback the primary automation model.
- Building a full preset-authoring DSL before e2e test primitives are stable.

## Current Status

Implemented:

- `CycleAutomation` singleton registered during Cycle startup.
- Standalone CLI args:
  - `--agent-script`
  - `--agent-report`
  - `--agent-session`
- `scripts/run_cycle_agent.sh`, which launches the app bundle through
  LaunchServices and passes automation args with `--args`.
- `scripts/run_cycle_agent_session.sh`, which launches Cycle in session mode
  and waits for the Unix-domain socket.
- `scripts/cycle_agent_session.py`, which sends one JSON command to a running
  Cycle session and prints the JSON response.
- JSON command dispatch for:
  - `snapshotState`,
  - `exportState`,
  - `exportPreset`,
  - `savePreset`,
  - `openPreset`,
  - `openFactoryPreset`,
  - `listMenus`,
  - `invokeMenuItem`,
  - `screenshot`,
  - `captureAudio`,
  - `inspectTargets`,
  - `inspectTree`,
  - `setControl`,
  - `pointer`,
  - `assertTarget`,
  - `assertState`,
  - `listAssertionPaths`,
  - `listMeshTargets`,
  - `exportMeshState`,
  - `meshSelectionGesture`,
  - `ping`,
  - `quit`,
  - `waitForIdle`,
  - `action`.
- `CycleTour` bridge for semantic action execution.
- Main menu discovery and invocation through `SynthMenuBarModel`.
- Named `CycleTour` area/target component lookup for screenshot targeting.
- `Document::exportSavableJSON(...)`, using `Savable::writeJSON()` with
  `writeXML()` fallback.
- LaunchServices smoke covering read-only state, assertions, tree inspection,
  `MeshLibrary`/preset export, preset save/reopen, factory preset loading,
  control mutation, pointer input, and named-panel screenshot capture.

Known gaps:

- `snapshotState` is useful for core UI state but does not yet include stable
  vertex handles or all dialog/modal visibility.
- Broad UI coverage is tracked in
  [cycle-agent-control-surface-audit.md](cycle-agent-control-surface-audit.md):
  every menu item, popup choice, button, knob/slider, tab, dialog, and custom
  mouse surface needs stable addressing before a vehicle-inspection-style smoke
  can claim near-total coverage.
- No CTest integration yet.
- Mesh-focused helpers for current layer/group have stable group/layer
  addressing, but vertex handles are still index-based.
- No keyboard playback yet.
- Audio capture can assert offline render amplitude and write WAV artifacts,
  but it does not yet validate the live OS/audio-device output path.
- No audio similarity/perceptual analysis yet; current audio assertions are
  numeric peak/RMS thresholds.
- Long-running IPC/session mode is available as an initial Unix-domain socket
  implementation; an MCP adapter remains pending.

## Command Schema

Scripts are JSON objects with a `commands` array and optional `quit` flag:

```json
{
  "commands": [
    { "command": "snapshotState" },
    { "command": "inspectTargets", "area": "AreaMorphPanel" },
    { "command": "inspectTree", "area": "AreaGenControls", "maxDepth": 3 },
    { "command": "exportState", "source": "meshLibrary", "path": "/tmp/mesh.json" },
    { "command": "openFactoryPreset", "preset": "CalmingKeys" },
    { "command": "screenshot", "area": "AreaMorphPanel", "path": "/tmp/morph.png" },
    { "command": "captureAudio", "note": 60, "path": "/tmp/note.wav", "peakGreaterThan": 0.0001 },
    { "command": "assertState", "path": "document", "exists": true },
    { "command": "action", "actionType": "SetMorphRange", "area": "AreaMorphPanel", "id": "IdYellow", "value": 0.5 }
  ],
  "quit": true
}
```

Reports are JSON objects with:

- `script`: script path,
- `results`: one result per command,
- `snapshot`: final snapshot.

## Progressive Milestones

### Milestone 1: Runner, Reports, And Read-Only State

Goal: Cycle can be launched reliably by automation and can prove that commands
ran.

Status: mostly complete.

Acceptance:

- Launch Cycle through `scripts/run_cycle_agent.sh`.
- Run `snapshotState`.
- Export a whole registered `Savable` source such as `MeshLibrary`.
- Write a structured report and filtered log.
- Quit cleanly when the script requests it.

### Milestone 2: Buttons And Panel Screenshots

Goal: an agent can invoke named buttons and capture focused visual evidence.

Status: partially implemented through `CycleTour` actions and screenshot
command path.

Acceptance:

- Trigger any registered button by named area/id.
- Capture full-window screenshots.
- Capture screenshots for named panels such as `AreaMorphPanel`.
- Capture screenshots for named targets such as a callout button or slider.
- Fail cleanly for unresolved, hidden, or zero-size targets.
- Report screenshot path, target bounds, and image dimensions.

Verified:

- `scripts/fixtures/cycle-agent-screenshot.json` captures `AreaMorphPanel` to
  `/private/tmp/cycle-agent-morph-panel.png`.
- The report includes target bounds and image dimensions.

### Milestone 3: Sliders, Knobs, And Toggles

Goal: an agent can set ordinary UI control values without pointer dragging.

Status: partially implemented with generic `setControl` support for named
slider, button, and ComboBox targets, plus semantic layer enablement actions.

Acceptance:

- Set morph sliders by axis. Done.
- Set effect/master knobs by parameter id. Done for representative master,
  waveshaper, Waveform3D, and impulse slider/knob targets.
- Set vertex property sliders for selected vertices.
- Enable/disable layers and effects. Done for Waveform3D current-layer
  enable/disable through semantic `Enable`/`Disable` actions; effect
  enablement command coverage remains pending.
- Assert resulting values through `snapshotState` or scoped JSON export. Done
  through `assertTarget`/`assertState`.
- Avoid pixel coordinates for normal value-setting commands. Done for current
  slider, button, ComboBox, and semantic action paths.

Verified:

- `scripts/fixtures/cycle-agent-set-morph-slider.json` sets
  `AreaMorphPanel` / `TargSliderY` to `0.25` and reports the resulting slider
  value through `inspectTargets`.
- `scripts/fixtures/cycle-agent-broader-controls.json` covers master slider,
  waveshaper knob, ComboBox controls, Waveform3D layer pan, and current-layer
  enable/disable.

### Milestone 4: Dialogs, Menus, And View Navigation

Goal: an agent can move around Cycle's UI, open modal surfaces, and operate
major menu/dialog workflows.

Status: partially available for some areas through existing tour actions.

Acceptance:

- Open and close supported dialogs or panels, including the mod matrix and
  preset browser where feasible.
- Switch view stages.
- Open a factory or explicit preset. Done with `openFactoryPreset` and
  `openPreset`.
- Capture a dialog or panel screenshot after opening it.
- Report modal/dialog visibility in target inspection or snapshots.

### Milestone 5: Mesh Inspection And Manipulation

Goal: an agent can create and edit the core Cycle sound-design data.

Status: partially available through `CycleTour` mesh actions, but needs richer
state reporting. Mesh target discovery is now available through
`listMeshTargets`, and full current-layer mesh export is available through
`exportMeshState`.

Acceptance:

- Add, move, select, deselect, and delete vertices using normalized coordinates.
- Set vertex parameters and guide-curve assignments.
- Export current mesh/layer JSON. Done for addressed mesh layers with
  `exportMeshState`.
- Snapshot mesh counts, selected vertices, and stable vertex handles. Mesh group
  and layer discovery now reports group ids/names, current layer, layer count,
  selected count, compact mesh counts, and layer properties.
- Validate changes with assertions rather than visual inspection only.
- Keep pointer-level mesh interactions as a separate regression path.

### Milestone 6: Preset Creation Workflow

Goal: an agent can build or modify a preset and save/export reproducible state.

Status: partially available through control mutation, state export, screenshots,
and smoke artifacts; full preset save/export/reopen verification is implemented.

Acceptance:

- Start from an empty or known factory preset. Done for named factory presets
  through `openFactoryPreset`.
- Apply semantic edits to controls, layers, effects, and meshes.
- Set preset metadata where supported.
- Save to a caller-provided preset path or export complete preset JSON. Done
  with `savePreset` and `exportPreset`.
- Reopen the saved preset and verify expected state. Done with `openPreset`
  followed by assertions.
- Produce report artifacts: final snapshot, exported state, screenshots, and
  logs.

### Milestone 7: Interactive Agent Session

Goal: an external agent can control a running Cycle instance over a persistent
local transport.

Status: initial implementation complete.

Acceptance:

- Launch Cycle through the platform GUI launcher with a session endpoint arg.
  Done with `--agent-session`.
- Publish readiness through a report file, port file, or socket path. Done by
  creating the Unix-domain socket path.
- Accept commands over local IPC. Done with newline-delimited JSON over a
  Unix-domain socket.
- Execute the same semantic command schema as one-shot scripts. Done through
  shared `CycleAutomation` command dispatch.
- Support screenshots and scoped state exports during the session. Done through
  existing `screenshot`, `exportState`, `exportPreset`, and `exportMeshState`
  commands.
- Shut down cleanly on command. Done with the `quit` command.

### Milestone 8: MIDI And Audio Capture

Goal: an agent can trigger synth playback deterministically and capture the
rendered signal as a test artifact.

Status: initial implementation complete for offline render capture.

Acceptance:

- Trigger MIDI note on/off events without relying on the OS audio device. Done
  with `captureAudio` scheduled `noteOn`/`noteOff` events.
- Support common MIDI events for audio regressions. Done for `noteOn`,
  `noteOff`, `controller`, `pitchWheel`, and `allNotesOff`.
- Render a bounded duration through the active synth processor. Done with
  command-provided `durationMs`, `sampleRate`, `blockSize`, and `channels`.
- Write a WAV artifact when requested. Done with the `path` field.
- Return simple metrics for assertions. Done for total `peak`, total `rms`,
  and per-channel peak/RMS.
- Assert non-silence or clipping thresholds in the same automation report. Done
  with `peakGreaterThan`, `peakLessThan`, `rmsGreaterThan`, and `rmsLessThan`.
- Distinguish offline synth rendering from live audio-device output. Done by
  suspending the standalone live callback during capture and restoring the
  previous audio preparation afterward.
- Validate live OS output capture. Pending; this requires a separate loopback,
  virtual audio device, or platform-level capture strategy.
- Add waveform similarity or perceptual assertions. Pending; this should
  consume the WAV artifact as a separate analysis step after basic amplitude
  coverage is stable.

## Implementation Plan

### Phase 1: Stabilize Runner And Command Shell

Supports milestones: 1 and 2.

Status: mostly complete.

- Keep LaunchServices runner as the supported macOS test entry point.
- Add usage docs to the script header or a short docs section.
- Add a smoke script under `cycle/tests/fixtures/` or `scripts/fixtures/`.
- Add a test wrapper that can run the smoke in local verification without
  relying on direct executable launch.
- Improve failure reports so parse errors, missing command fields, and invalid
  targets are reported as command failures rather than assertions.
- Handle startup crashes before in-app automation starts:
  - dismiss the macOS “Cycle quit unexpectedly” Problem Reporter dialog when
    Accessibility permission allows it,
  - copy a fresh `Cycle-*.ips` from `~/Library/Logs/DiagnosticReports`,
  - append crash-report path and header content to the raw/filtered logs.

### Phase 2: Target Inspection

Supports milestones: 2, 3, and 4.

Status: partially implemented.

- Add `inspectTargets`.
- Return registered areas and targets known to `CycleTour`.
- Add `inspectTree` for bounded live child-tree discovery from a resolved
  root.
- For each resolved target include:
  - area,
  - target,
  - component name,
  - component class when available,
  - visibility,
  - enabled state,
  - local bounds,
  - screen bounds.
- For supported controls, include:
  - slider value/min/max/interval/text,
  - button toggle state and label.
- Include unresolved targets with an error field so missing registry coverage is
  visible in reports.

Current limitations:

- No-argument inspection returns a curated set of known `CycleTour` areas and
  targets, not a live iteration over the private registry.
- Component class names currently use C++ RTTI names, which are useful for
  debugging but not stable public identifiers.
- `inspectTree` returns roles, bounds, child paths, and CycleTour area/target
  annotations where a live component matches the registry. It also includes a
  `registeredTargets` side table for named target rectangles scoped to the
  requested area/root.
- Generic JUCE control metadata is intentionally limited to lightweight,
  stable widget state. Do not keep expanding `CycleAutomation::componentState`
  with every concrete Cycle component type.

Planned serialization boundary:

- `componentState` remains the generic fallback:
  - identity,
  - visibility/enabled/showing,
  - bounds,
  - stable JUCE control state for common controls such as `Slider`, `Button`,
    later `ComboBox`/`TextEditor` if needed.
- Rich Cycle components expose automation-specific state through an optional
  interface, tentatively `AutomationInspectable`.
- `AutomationInspectable` is implemented and currently attached to
  `MorphPanel`, `GeneralControls`, and `Waveform3D`.
- Full preset/model JSON continues to flow through explicit `Savable` export
  via `exportState`, not implicit target inspection.
- `inspectTargets` may later accept an option such as `includeSavable` or
  `stateDepth: "full"` to opt into larger durable state payloads.
- Mutations should remain semantic commands where possible; generic
  `setControl` is the fallback for ordinary widgets, not the only control path.

### Phase 3: Rich Snapshots And Assertions

Supports milestones: 3, 4, 5, and 6.

Status: partially implemented through richer `inspectTargets` control metadata,
the `AutomationInspectable` interface, enriched `snapshotState`, and assertions.

- Add an automation-state interface for rich UI components:
  - `exportAutomationState()` for transient UI state that is useful to agents,
  - optional apply/control hooks only where generic `setControl` is not enough,
  - no automatic full `Savable` dump from every inspected component.
- Implemented:
  - `MorphPanel.v1` reports morph slider values, linking, range flags, primary
    axis, and current MIDI key under `automationState`.
  - `GeneralControls.v1` reports current tool, view stage, transport state,
    surface-view flags, and button applicability/highlight state.
  - `Waveform3D.v1` reports time-layer index/counts, current layer properties,
    current mesh counts, view stage, wave-overlay flags, grid status, and render
    width.
- Expand `snapshotState` to include:
  - document/preset name. Done.
  - current view stage. Done.
  - current tool. Done.
  - selected panel/layer where available,
  - morph position and range/link state. Done.
  - mesh counts by layer group. Done for Waveform3D time layer.
  - selected vertex ids/counts,
  - effect enablement,
  - update-idle status.
- Add `assertTarget` with simple operators. Done for target-scoped component
  state paths.
- Add `assertState` with simple operators. Done for report snapshot paths:
  - equals,
  - notEquals,
  - lessThan,
  - lessThanOrEqual,
  - greaterThan,
  - greaterThanOrEqual,
  - exists.
- Assertions should operate on snapshot paths and report the actual value on
  failure. Done for `assertTarget` and `assertState`.
- Add assertion path discovery. Done with `listAssertionPaths`, which flattens
  `snapshotState` or target `componentState` into paths, current values, value
  types, and compatible assertion operators.

### Phase 4: Update-Idle Barrier

Supports milestones: 2 through 7.

Status: partially implemented with an explicit conservative `waitForIdle`
command.

- Add `waitForIdle`. Done for a fixed message-loop drain/delay command.
- Ensure screenshots and assertions can request an idle wait first. Done with
  `waitForIdle: true` and optional `idleDelayMs`.
- Define idle conservatively at first:
  - JUCE message queue has processed the command,
  - any requested fixed delay has elapsed. Done.
  - no known pending automation timer remains.
- Later refine this with explicit `CycleUpdater` state if needed.

### Phase 5: Screenshots

Supports milestone: 2.

Status: named-panel screenshot smoke is passing for normal JUCE-rendered panels;
OpenGL-backed panel capture works through runner-side OS crop capture.

- Smoke-test full-window screenshot.
- Smoke-test named panel screenshot. Done for `AreaMorphPanel`.
- Smoke-test OpenGL-backed panel screenshot. Done for `AreaWfrmWaveform3D`:
  target resolution succeeds and runner-side `screencapture -R` captures the
  rendered waveform surface.
- Add target-region screenshots by `area` plus `target`.
- Report final image dimensions and target bounds.
- Fail cleanly for hidden, zero-size, or unresolved targets.
- Keep OpenGL-safe capture as a runner-side OS screenshot path for now:
  - preferred: renderer/compositor-owned readback for the named panel region,
  - current local GUI smoke: runner-level OS screenshot after confirmed
    focus and screen-capture permission,
  - avoid in-app `screencapture` as the default app command path because it failed
    in automation and ties app behavior to macOS permissions.
- Preflight macOS permissions before OS-level capture:
  `scripts/capture_cycle_ui.sh` checks Accessibility and Screen Recording;
  `scripts/run_cycle_agent.sh` checks Accessibility by default and can also
  check Screen Recording with `CYCLE_PREFLIGHT_SCREEN_CAPTURE=1`.
- Runner-side OS screenshot crop is available for one named inspected target:
  set `CYCLE_OS_SCREENSHOT_AREA`, optional `CYCLE_OS_SCREENSHOT_TARGET`, and
  `CYCLE_OS_SCREENSHOT_PATH`. The runner crops the target's reported
  `screenBounds` with `screencapture -R`.

### Phase 6: Scoped State Export

Supports milestones: 1, 5, and 6.

Status: whole-savable export implemented with source aliases and optional JSON
path selection.

- Keep whole-source export by registered savable name.
- Treat `Savable` JSON as durable document/model state, not generic UI
  component state.
- Do not call full `saveJson` implicitly from `inspectTargets`; use explicit
  `exportState` or a future opt-in inspection flag when large state is wanted.
- Add source aliases for common agent-facing names. Done for `meshLibrary`,
  `morphPanel`, `mainPanel`, `effects`, `modMatrix`, `envelopes`, and related
  preset section names.
- Add optional JSON path selection for subtrees of exported savable state. Done
  with `jsonPath`/`statePath`.
- Add mesh-focused helpers for current layer/group once stable addressing is
  defined. Done for read-only discovery with `listMeshTargets`; full layer
  JSON export is available through `exportMeshState`.

### Phase 7: Mesh And Pointer Commands

Supports milestone: 5.

Status: partially covered through semantic mesh gestures, `CycleTour` actions,
and target-local pointer events. Read-only mesh target discovery and
layer-level mesh JSON export are implemented.

- Keep semantic mesh edits as primary:
  - add vertex,
  - move vertex,
  - delete vertex,
  - select vertex,
  - set vertex param.
- Add stable vertex handles to snapshots and reports.
- Add `listMeshTargets` for stable mesh group/layer addressing. Done with
  group ids/names, current layer, layer count, selected count, compact
  current/effective mesh summaries, and per-layer mesh/property summaries.
- Add `exportMeshState` for addressed layer export. Done for `group`/`groupId`
  plus `layer`/`layerIndex`, defaulting `layer` to `current`; output includes
  compact summary plus full `Mesh::writeJSON()` and layer
  `Properties::writeJSON()` state.
- Add semantic mesh mutation commands. Done for `selectVertex`, `addVertex`,
  `moveVertex`, and `deleteVertex`; the default `mode` is `gesture`, which
  drives the active interactor with synthetic mouse/key events. Explicit
  `mode: "patch"` remains available for non-e2e preset generation, and
  `mode: "tour"` remains available for the older `CycleTour` action bridge.
- Add selection-frame gestures. Done with `meshSelectionGesture` for
  `boxSelect`, `dragHandle`, `moveSelection`, and `clear`; this exercises
  `doBoxSelect`, `setHighlitCorner`, `DraggingCorner`, `doDragCorner`,
  `resizeFinalBoxSelection`, and selection-frame updates. Axe-specific
  gestures remain out of scope for now.
- Add low-level pointer/key/wheel commands only for interaction regressions.
  Done for pointer click/down/up/drag/move/doubleClick/wheel events.
- Pointer commands should target named components plus local coordinates. Done
  with local `x`/`y`, normalized coordinates, and center-point default.

### Phase 8: E2E Integration

Supports milestones: 1 through 8.

Status: partially implemented with an opt-in local smoke runner.

- Add repeatable smoke scripts for:
  - opening the default preset. Covered by `cycle-agent-readonly.json`.
  - discovering assertion paths. Covered by
    `cycle-agent-assertion-paths.json`.
  - discovering mesh targets and exporting addressed mesh state. Covered by
    `cycle-agent-mesh-targets.json`.
  - opening `CalmingKeys`, exporting the current time-layer mesh, and asserting
    its vertex/cube counts. Covered by `cycle-agent-calmingkeys-mesh.json`.
  - adding, selecting, moving, and deleting a time-layer vertex/cube edit.
    Covered by `cycle-agent-mesh-mutations.json`.
  - box-selecting a mesh vertex group, dragging a selection edge, moving the
    selection widget, and clearing selection. Covered by
    `cycle-agent-mesh-selection-gesture.json`.
  - exporting `MeshLibrary`. Covered by `cycle-agent-readonly.json`.
  - setting morph position. Covered by `cycle-agent-set-morph-slider.json`.
  - setting broader master/effect/layer controls. Covered by
    `cycle-agent-broader-controls.json`.
  - opening a named factory preset. Covered by
    `cycle-agent-factory-preset.json`.
  - triggering MIDI and capturing offline rendered audio. Covered by
    `cycle-agent-audio-capture.json`.
  - capturing a named panel. Covered by `cycle-agent-screenshot.json`.
  - listing and invoking main menu items. Covered by
    `cycle-agent-menu-commands.json`.
  - executing a `CycleTour` action. Covered by
    `cycle-agent-general-controls.json`.
- Add a local test command that runs these through `scripts/run_cycle_agent.sh`.
  Done with `scripts/run_cycle_agent_smokes.sh`.
- Keep GUI e2e tests opt-in if CI cannot launch app bundles.
- Save report, filtered log, raw log, screenshots, and exported JSON as test
  artifacts where possible. Done under `CYCLE_AGENT_ARTIFACT_DIR`, defaulting
  to `/private/tmp/cycle-agent-smokes`.

### Phase 9: Long-Running Session Mode

Supports milestone: 7.

Status: initial implementation complete.

- Add an automation server mode only after one-shot commands stabilize. Done.
- Launch through the platform GUI launcher with an endpoint argument. Done with
  `scripts/run_cycle_agent_session.sh`.
- Publish readiness through a report file, port file, or socket path. Done via
  socket creation.
- Use local TCP/WebSocket or a local socket/named pipe. Done with a
  Unix-domain socket on macOS/Linux.
- Keep MCP as a thin adapter over `CycleAutomation`.
- Execute all commands on the JUCE message thread. Done by marshalling session
  requests onto the message thread before command dispatch.

## Verification

Current verified command:

```sh
scripts/run_cycle_agent.sh /private/tmp/cycle-agent-smoke.json /private/tmp/cycle-agent-report.json /private/tmp/cycle-agent-logs.txt
scripts/run_cycle_agent_session.sh /private/tmp/cycle-agent.sock /private/tmp/cycle-agent-session.log
scripts/cycle_agent_session.py /private/tmp/cycle-agent.sock -c '{"command":"ping"}'
```

Repository smoke fixtures:

```sh
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-readonly.json /private/tmp/cycle-agent-readonly-report.json /private/tmp/cycle-agent-readonly-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-assertion-paths.json /private/tmp/cycle-agent-assertion-paths-report.json /private/tmp/cycle-agent-assertion-paths-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-mesh-targets.json /private/tmp/cycle-agent-mesh-targets-report.json /private/tmp/cycle-agent-mesh-targets-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-calmingkeys-mesh.json /private/tmp/cycle-agent-calmingkeys-mesh-report.json /private/tmp/cycle-agent-calmingkeys-mesh-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-mesh-mutations.json /private/tmp/cycle-agent-mesh-mutations-report.json /private/tmp/cycle-agent-mesh-mutations-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-mesh-selection-gesture.json /private/tmp/cycle-agent-mesh-selection-gesture-report.json /private/tmp/cycle-agent-mesh-selection-gesture-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-screenshot.json /private/tmp/cycle-agent-screenshot-report.json /private/tmp/cycle-agent-screenshot-logs.txt
CYCLE_OS_SCREENSHOT_AREA=AreaWfrmWaveform3D CYCLE_OS_SCREENSHOT_PATH=/private/tmp/cycle-agent-waveform3d-os.png scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-waveform3d-os-screenshot.json /private/tmp/cycle-agent-waveform3d-os-report.json /private/tmp/cycle-agent-waveform3d-os-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-menu-commands.json /private/tmp/cycle-agent-menu-commands-report.json /private/tmp/cycle-agent-menu-commands-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-set-morph-slider.json /private/tmp/cycle-agent-set-morph-slider-report.json /private/tmp/cycle-agent-set-morph-slider-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-broader-controls.json /private/tmp/cycle-agent-broader-controls-report.json /private/tmp/cycle-agent-broader-controls-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-factory-preset.json /private/tmp/cycle-agent-factory-preset-report.json /private/tmp/cycle-agent-factory-preset-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-general-controls.json /private/tmp/cycle-agent-general-controls-report.json /private/tmp/cycle-agent-general-controls-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-waveform3d-state.json /private/tmp/cycle-agent-waveform3d-state-report.json /private/tmp/cycle-agent-waveform3d-state-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-audio-capture.json /private/tmp/cycle-agent-audio-capture-report.json /private/tmp/cycle-agent-audio-capture-logs.txt
CYCLE_AGENT_ALLOW_FAILURES=1 scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-assert-failure.json /private/tmp/cycle-agent-assert-failure-report.json /private/tmp/cycle-agent-assert-failure-logs.txt
scripts/run_cycle_agent_smokes.sh
```

Current verified behavior:

- Cycle launches through LaunchServices.
- Startup crash handling is runner-side: `scripts/run_cycle_agent.sh` can
  dismiss the macOS crash dialog and copy a fresh `.ips` beside the logs.
- Default preset `AfricanHorn` opens.
- `snapshotState` reports the document name, current tool/view stage, morph
  state, and Waveform3D time-layer state.
- `inspectTargets` resolves `AreaMorphPanel` and returns bounds.
- `inspectTree` reports a bounded component tree with roles, rectangles,
  CycleTour area/target annotations, and a `registeredTargets` side table.
- `exportState` writes `MeshLibrary` JSON through the `meshLibrary` alias and
  can export a selected subtree through `jsonPath`.
- `exportPreset` writes full preset JSON or a selected subtree; `savePreset`
  writes a compressed `.cyc` preset file; `openPreset` reopens a saved preset
  for verification.
- `openFactoryPreset` opens a named factory preset by preset name.
- `screenshot` captures `AreaMorphPanel` as a 370 x 165 PNG in the current
  standalone-debug layout.
- `inspectTargets` resolves `AreaWfrmWaveform3D` to an `OpenGLPanel3D`.
- App-side `screenshot` currently captures only a blank OpenGL component
  snapshot for `AreaWfrmWaveform3D`.
- Runner-side OS capture resolves `AreaWfrmWaveform3D` bounds and captures a
  nonblank OpenGL screenshot. On Retina displays, the output PNG can be scaled
  relative to JUCE screen bounds; 441 x 368 bounds produced an 882 x 736 PNG.
- `setControl` sets `AreaMorphPanel` / `TargSliderY` to `0.25` and
  `inspectTargets` reports the resulting value.
- `listMenus` reports top-level menu names and recursively lists
  `PopupMenu::Item` ids, paths, enabled state, tick state, triggerability, and
  submenu presence.
- `invokeMenuItem` executes a resolved main menu item through
  `SynthMenuBarModel::menuItemSelected(...)`; the focused fixture invokes
  Graphics / Displayed Processing Stage / 2. After envelopes and asserts
  `viewStageName == postEnvelopes`.
- `setControl` also covers representative master sliders, effect knobs,
  ComboBoxes, and Waveform3D layer pan; semantic `Enable`/`Disable` actions
  cover the current Waveform3D layer active toggle.
- `GeneralControls` reports `GeneralControls.v1`; a `SwitchToTool` action
  updates `automationState.tools.name` to `pencil`.
- `pointer` can click a named GeneralControls button target and update
  `automationState.tools.name`.
- `Waveform3D` reports `Waveform3D.v1`; setting `TargPan` updates
  `automationState.layer.currentProperties.pan`.
- `assertTarget` validates target-scoped dot paths and reports actual/expected
  values on failure.
- `assertState` validates report snapshot dot paths and reports actual/expected
  values on failure.
- `listAssertionPaths` discovers assertable snapshot or target paths with
  current values, value types, and compatible operators.
- `listMeshTargets` discovers mesh groups/layers with stable ids/names, current
  layer, layer count, selected count, compact mesh counts, and Savable layer
  property JSON.
- `exportMeshState` exports an addressed mesh layer to report data and an
  optional JSON file, reusing `Mesh::writeJSON()` and
  `Properties::writeJSON()`.
- `cycle-agent-calmingkeys-mesh.json` opens `CalmingKeys`, exports `time[0]`,
  and asserts the current time mesh has 16 vertices and 2 cubes.
- `selectVertex`, `addVertex`, `moveVertex`, and `deleteVertex` provide
  semantic mesh mutation. Default gesture mode resolves the active interactor,
  maps mesh coordinates through the panel scale, and sends synthetic
  mouse/key events so e2e tests exercise hover, hit-testing, selection, drag,
  delete, update, undo, and repaint paths. Explicit patch mode is reserved for
  programmatic preset generation. The focused mutation fixture covers
  CalmingKeys `time[0]`: add changes 16/2 to 24/3, select reports one selected
  vertex, move preserves 24/3, and delete returns the mesh to 16/2.
- `meshSelectionGesture` drives group selection with synthetic mouse gestures.
  The focused fixture box-selects two CalmingKeys time vertices, verifies the
  16-entry moving selection frame, drags the right selection edge, drags the
  move handle, and clears selection.
- `captureAudio` triggers scheduled MIDI events, renders offline through the
  active synth processor, writes an optional WAV artifact, and reports peak/RMS
  metrics. The focused fixture opens `AfricanHorn`, sends C4 note on/off,
  writes `/private/tmp/cycle-agent-africanhorn-c4.wav`, and asserts non-silent
  output plus a conservative peak ceiling.
- `--agent-session` starts a Unix-domain socket server that accepts one
  newline-delimited JSON request per client connection, dispatches the command
  on the JUCE message thread, and returns one JSON response.
- `scripts/cycle_agent_session.py` can send `ping`, `snapshotState`,
  `openFactoryPreset`, `meshSelectionGesture`, and `quit` to a running session.
- `waitForIdle` performs an explicit fixed message-loop drain/delay command.
- Cycle exits cleanly when `quit` is true.

## Open Questions

- Should GUI automation smoke tests be part of default `ctest`, or should they
  remain opt-in because they launch a GUI app?
  - Daven: Remain opt-in for now
- Should screenshot capture use JUCE component snapshots, OS screenshots, or a
  panel compositor capture path for OpenGL-backed panels? 
  - Daven: JUCE component snapshots, unless we find they miss actual UI content
    (i.e. blank for some reason)
  - Current finding: `AreaWfrmWaveform3D` is blank through JUCE component
    snapshots, so OpenGL panels use runner-side OS crop capture for now.
- Which fields are the minimum useful `snapshotState` contract for v1 e2e tests?
  - Daven: to be determined. 
- Should scoped JSON subtree selection use a simple dot path, JSON pointer, or a
  small set of named selectors?
