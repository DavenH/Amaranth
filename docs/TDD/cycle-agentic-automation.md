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
- `scripts/run_cycle_agent.sh`, which launches the app bundle through
  LaunchServices and passes automation args with `--args`.
- JSON command dispatch for:
  - `snapshotState`,
  - `exportState`,
  - `screenshot`,
  - `inspectTargets`,
  - `action`.
- `CycleTour` bridge for semantic action execution.
- Named `CycleTour` area/target component lookup for screenshot targeting.
- `Document::exportSavableJSON(...)`, using `Savable::writeJSON()` with
  `writeXML()` fallback.
- LaunchServices smoke covering `snapshotState`, `inspectTargets`,
  `MeshLibrary` state export, and named-panel screenshot capture.

Known gaps:

- `snapshotState` is minimal.
- No assertion command yet.
- No update-idle barrier before assertions/screenshots.
- No CTest integration yet.
- Scoped export currently targets whole registered `Savable` sources, not
  subregions/subtrees.
- No pointer/key/wheel playback yet.
- No long-running IPC/session mode yet.

## Command Schema

Scripts are JSON objects with a `commands` array and optional `quit` flag:

```json
{
  "commands": [
    { "command": "snapshotState" },
    { "command": "inspectTargets", "area": "AreaMorphPanel" },
    { "command": "exportState", "source": "MeshLibrary", "path": "/tmp/mesh.json" },
    { "command": "screenshot", "area": "AreaMorphPanel", "path": "/tmp/morph.png" },
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

Status: partially available through existing `CycleTour` actions.

Acceptance:

- Set morph sliders by axis.
- Set effect/master knobs by parameter id.
- Set vertex property sliders for selected vertices.
- Enable/disable layers and effects.
- Assert resulting values through `snapshotState` or scoped JSON export.
- Avoid pixel coordinates for normal value-setting commands.

### Milestone 4: Dialogs, Menus, And View Navigation

Goal: an agent can move around Cycle's UI, open modal surfaces, and operate
major menu/dialog workflows.

Status: partially available for some areas through existing tour actions.

Acceptance:

- Open and close supported dialogs or panels, including the mod matrix and
  preset browser where feasible.
- Switch view stages.
- Open a factory or explicit preset.
- Capture a dialog or panel screenshot after opening it.
- Report modal/dialog visibility in target inspection or snapshots.

### Milestone 5: Mesh Inspection And Manipulation

Goal: an agent can create and edit the core Cycle sound-design data.

Status: partially available through `CycleTour` mesh actions, but needs richer
state reporting.

Acceptance:

- Add, move, select, deselect, and delete vertices using normalized coordinates.
- Set vertex parameters and guide-curve assignments.
- Export current mesh/layer JSON.
- Snapshot mesh counts, selected vertices, and stable vertex handles.
- Validate changes with assertions rather than visual inspection only.
- Keep pointer-level mesh interactions as a separate regression path.

### Milestone 6: Preset Creation Workflow

Goal: an agent can build or modify a preset and save/export reproducible state.

Status: not started.

Acceptance:

- Start from an empty or known factory preset.
- Apply semantic edits to controls, layers, effects, and meshes.
- Set preset metadata where supported.
- Save to a caller-provided preset path or export complete preset JSON.
- Reopen the saved preset and verify expected state.
- Produce report artifacts: final snapshot, exported state, screenshots, and
  logs.

### Milestone 7: Interactive Agent Session

Goal: an external agent can control a running Cycle instance over a persistent
local transport.

Status: deferred.

Acceptance:

- Launch Cycle through the platform GUI launcher with a session endpoint arg.
- Publish readiness through a report file, port file, or socket path.
- Accept commands over local IPC.
- Execute the same semantic command schema as one-shot scripts.
- Support screenshots and scoped state exports during the session.
- Shut down cleanly on command.

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

### Phase 2: Target Inspection

Supports milestones: 2, 3, and 4.

Status: partially implemented.

- Add `inspectTargets`.
- Return registered areas and targets known to `CycleTour`.
- For each resolved target include:
  - area,
  - target,
  - component name,
  - component class when available,
  - visibility,
  - enabled state,
  - local bounds,
  - screen bounds.
- Include unresolved targets with an error field so missing registry coverage is
  visible in reports.

Current limitations:

- No-argument inspection returns a curated set of known `CycleTour` areas and
  targets, not a live iteration over the private registry.
- Component class names currently use C++ RTTI names, which are useful for
  debugging but not stable public identifiers.

### Phase 3: Rich Snapshots And Assertions

Supports milestones: 3, 4, 5, and 6.

Status: not started.

- Expand `snapshotState` to include:
  - document/preset name,
  - current view stage,
  - current tool,
  - selected panel/layer where available,
  - morph position and range/link state,
  - mesh counts by layer group,
  - selected vertex ids/counts,
  - effect enablement,
  - update-idle status.
- Add `assertState` with simple operators:
  - equals,
  - notEquals,
  - lessThan,
  - lessThanOrEqual,
  - greaterThan,
  - greaterThanOrEqual,
  - exists.
- Assertions should operate on snapshot paths and report the actual value on
  failure.

### Phase 4: Update-Idle Barrier

Supports milestones: 2 through 7.

Status: not started.

- Add `waitForIdle`.
- Ensure screenshots and assertions can request an idle wait first.
- Define idle conservatively at first:
  - JUCE message queue has processed the command,
  - any requested fixed delay has elapsed,
  - no known pending automation timer remains.
- Later refine this with explicit `CycleUpdater` state if needed.

### Phase 5: Screenshots

Supports milestone: 2.

Status: named-panel screenshot smoke is passing; needs hardening.

- Smoke-test full-window screenshot.
- Smoke-test named panel screenshot. Done for `AreaMorphPanel`.
- Add target-region screenshots by `area` plus `target`.
- Report final image dimensions and target bounds.
- Fail cleanly for hidden, zero-size, or unresolved targets.
- Decide whether OpenGL-backed panel snapshots need a compositor-specific
  capture path if JUCE component snapshots are blank.

### Phase 6: Scoped State Export

Supports milestones: 1, 5, and 6.

Status: whole-savable export implemented.

- Keep whole-source export by registered savable name.
- Add source aliases for common agent-facing names:
  - `meshLibrary`,
  - `morphPanel`,
  - `mainPanel`,
  - `effects`,
  - `modMatrix`,
  - `envelopes`.
- Add optional JSON path selection for subtrees of exported savable state.
- Add mesh-focused helpers for current layer/group once stable addressing is
  defined.

### Phase 7: Mesh And Pointer Commands

Supports milestone: 5.

Status: partially covered through `CycleTour` actions.

- Keep semantic mesh edits as primary:
  - add vertex,
  - move vertex,
  - delete vertex,
  - select vertex,
  - set vertex param.
- Add stable vertex handles to snapshots and reports.
- Add low-level pointer/key/wheel commands only for interaction regressions.
- Pointer commands should target named components plus local coordinates.

### Phase 8: E2E Integration

Supports milestones: 1 through 6.

Status: not started.

- Add repeatable smoke scripts for:
  - opening the default preset,
  - exporting `MeshLibrary`,
  - setting morph position,
  - capturing a named panel,
  - executing a `CycleTour` action.
- Add a local test command that runs these through `scripts/run_cycle_agent.sh`.
- Keep GUI e2e tests opt-in if CI cannot launch app bundles.
- Save report, filtered log, raw log, screenshots, and exported JSON as test
  artifacts where possible.

### Phase 9: Long-Running Session Mode

Supports milestone: 7.

Status: deferred.

- Add an automation server mode only after one-shot commands stabilize.
- Launch through the platform GUI launcher with an endpoint argument.
- Publish readiness through a report file, port file, or socket path.
- Use local TCP/WebSocket or a local socket/named pipe.
- Keep MCP as a thin adapter over `CycleAutomation`.
- Execute all commands on the JUCE message thread.

## Verification

Current verified command:

```sh
scripts/run_cycle_agent.sh /private/tmp/cycle-agent-smoke.json /private/tmp/cycle-agent-report.json /private/tmp/cycle-agent-logs.txt
```

Repository smoke fixtures:

```sh
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-readonly.json /private/tmp/cycle-agent-readonly-report.json /private/tmp/cycle-agent-readonly-logs.txt
scripts/run_cycle_agent.sh scripts/fixtures/cycle-agent-screenshot.json /private/tmp/cycle-agent-screenshot-report.json /private/tmp/cycle-agent-screenshot-logs.txt
```

Current verified behavior:

- Cycle launches through LaunchServices.
- Default preset `AfricanHorn` opens.
- `snapshotState` reports the document name.
- `inspectTargets` resolves `AreaMorphPanel` and returns bounds.
- `exportState` writes `MeshLibrary` JSON.
- `screenshot` captures `AreaMorphPanel` as a 370 x 165 PNG in the current
  standalone-debug layout.
- Cycle exits cleanly when `quit` is true.

## Open Questions

- Should GUI automation smoke tests be part of default `ctest`, or should they
  remain opt-in because they launch a GUI app?
  - Daven: Remain opt-in for now
- Should screenshot capture use JUCE component snapshots, OS screenshots, or a
  panel compositor capture path for OpenGL-backed panels? 
  - Daven: JUCE component snapshots, unless we find they miss actual UI content (i.e. blank for some reason)
- Which fields are the minimum useful `snapshotState` contract for v1 e2e tests?
  - Daven: to be determined. 
- Should scoped JSON subtree selection use a simple dot path, JSON pointer, or a
  small set of named selectors?
