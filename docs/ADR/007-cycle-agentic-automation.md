# ADR 007: Agentic Automation For Cycle UI Workflows

## Status

Proposed

## Context

Cycle is a UI-heavy synthesizer. Important workflows pass through menus,
dialogs, ordinary widgets, and flexible 2D/3D editing environments where users
manipulate mesh data directly.

Agentic workflows need a reliable way to operate that interface for:

- easier bug reproduction,
- deterministic end-to-end tests,
- screenshot and state regression checks,
- future preset creation and sound-design workflows.

The current `CycleTour` implementation already contains useful pieces of this
model:

- named application areas and target components,
- lookup from tutorial targets to live JUCE components,
- semantic actions for panels, layers, morph values, mesh points, effects,
  playback, presets, and mod matrix state,
- XML parsing for ordered action sequences.

However, `CycleTour` is shaped around tutorials and callout rendering. It mixes
target lookup, action execution, tutorial progression, highlighting, and
explanatory UI. That makes it a useful seed for automation, but not the right
long-term API for tests or external agents.

## Decision

We will build a deterministic in-app automation layer for Cycle and expose the
first milestone through CLI JSON scripts and structured JSON reports.

The automation core will be extracted or generalized from the non-tutorial
parts of `CycleTour`. `CycleTour` may continue to use the same registry and
action execution facilities, but tutorial rendering and automation execution
will be separate concerns.

The primary control model will be semantic commands, not screen scraping or
absolute mouse coordinates. Commands should address Cycle concepts directly:

- presets and documents,
- application areas and dialogs,
- named widgets and target components,
- morph position and view stage,
- layer and effect state,
- normalized mesh coordinates and vertex handles,
- playback and MIDI note state,
- mod matrix cells.

The v1 command surface will support:

- running an ordered action list,
- inspecting registered targets and component bounds,
- taking state snapshots,
- exporting scoped JSON state for a named panel, region, mesh, or registered
  save source,
- evaluating simple assertions,
- capturing full-window screenshots to known paths,
- capturing screenshots for named panels or target regions, automatically
  cropped to the registered component bounds,
- writing a structured result report.

Scoped JSON export should reuse existing `Savable` methods. Automation should
not create a separate serialization model for panels, meshes, effects, or
document-backed state when a component already participates in
`Savable::writeJSON()` or `Savable::writeXML()`. Where automation needs only a
subtree of a larger savable source, it should select from the same JSON shape
used by the preset/document path.

Pointer, keyboard, and wheel playback will exist only as a secondary path for
reproducing bugs in the real interaction layer. Those commands should target a
named component plus local coordinates, rather than global screen positions.

MCP is deferred until the command model is proven. A future MCP server should be
a thin adapter over the same automation core, with tools such as
`cycle.run_actions`, `cycle.inspect`, `cycle.snapshot`, and
`cycle.screenshot`.

## Consequences

### Positive

- End-to-end tests can reproduce UI-heavy workflows deterministically.
- Tests can operate on Cycle's domain state instead of brittle pixels.
- Bug reports can include the input script, structured report, state snapshot,
  scoped JSON export, and screenshot.
- Panel-specific screenshot capture produces focused visual artifacts without
  requiring every test to know current layout coordinates.
- Scoped JSON export reuses preset/document serialization semantics instead of
  creating a competing state format.
- Future agent workflows can reuse the same command model for live control and
  preset creation.
- Existing JUCE layout, focus, and interaction code remains authoritative.
- `CycleTour` can keep its guided UI role while sharing target registration and
  semantic actions.

### Negative

- Existing `CycleTour` responsibilities must be separated carefully.
- Command names and schemas become a compatibility surface and need review
  discipline.
- Automation must account for asynchronous JUCE message-thread work and Cycle's
  visual update graph before assertions or screenshots run.
- Mesh commands need stable addressing rules so tests do not depend only on
  incidental vertex order.
- Some desired region-level exports may require small adapters when the current
  `Savable` boundary is coarser than the UI region an agent asks for.
- Pointer-level tests still require care because they intentionally exercise
  layout and event-routing behavior.

## Alternatives Considered

### Extend `CycleTour` directly

Rejected as the long-term shape.

`CycleTour` has the right raw ingredients, but tutorial progression, callout
positioning, highlighting, and automation execution should not remain coupled.
Keeping those responsibilities together would make test and agent APIs depend
on tutorial UI behavior.

### Build MCP first

Rejected for the first milestone.

MCP is a good eventual interface for external agents, but starting there would
mix three problems: the application command model, protocol lifecycle, and
agent-tool ergonomics. A CLI JSON runner proves the app-side automation surface
first and can later be wrapped by MCP without changing Cycle's internals.

### Use OS-level clicking and screen scraping

Rejected as the primary model.

Cycle's important state is semantic mesh, layer, parameter, and preset data.
Screen coordinates are too brittle for routine testing and preset creation.
Low-level event playback remains useful for targeted interaction regressions,
but it should not be the main automation interface.

### Start with a socket API

Deferred.

A socket API is useful for long-running live agent sessions, but it adds
lifecycle, security, concurrency, and shutdown behavior before the command
schema is stable. It can be added later as another adapter over the same
automation core.

## Implementation Plan

### Phase 1: Separate automation from tutorial UI

- Extract or introduce a shared automation registry for areas, targets, action
  names, ids, and component lookup.
- Move semantic action execution behind an automation service that runs on the
  JUCE message thread.
- Keep `CycleTour` as a client for tutorial callouts and highlighting.

### Phase 2: Add CLI JSON execution

- Add standalone-app command-line options for an automation script path and
  report output path.
- Parse an ordered JSON action list into validated automation commands.
- Execute commands after startup and requested preset loading are complete.
- Write structured success, failure, timing, and diagnostic output.
- Support an explicit command to exit the app after the script finishes.

### Phase 3: Add inspection, snapshots, assertions, and screenshots

- Expose target inspection with area, target, visibility, enabled state, bounds,
  component name, and component class where available.
- Expose state snapshots for document name, selected panels/layers, morph
  position, mesh counts, selected vertices, effect enablement, and update-idle
  status.
- Add scoped JSON export for named panels, regions, meshes, and registered
  savable sources.
- Route export through existing `Savable::writeJSON()` where available, falling
  back to `writeXML()` only for legacy sources without JSON support.
- Add simple assertions over snapshot fields.
- Add screenshot capture to a caller-provided path.
- Add named panel and target-region screenshots by resolving the component
  through the automation target registry and cropping to its screen bounds.
- Ensure assertions and screenshots wait for pending visual/UI updates to settle.

### Phase 4: Cover core e2e workflows

- Open a known factory preset and assert expected document state.
- Set morph values and verify snapshot state.
- Enable and disable layers and effects.
- Add, move, select, and delete mesh vertices using normalized coordinates.
- Capture full-window and named-panel screenshots for visual regression checks.
- Export scoped JSON for a panel, mesh, and effect state and compare it against
  expected state.
- Add one pointer-event regression test for a real editor interaction path.

### Phase 5: Add live agent adapters

- Add an MCP server or socket API only after the CLI schema and automation
  reports are stable.
- Keep external adapters thin; they should call the same automation service
  instead of reimplementing command behavior.

## Success Metrics

- A standalone end-to-end run can open a preset, perform semantic UI and mesh
  edits, assert final state, capture a screenshot, write a JSON report, and exit.
- The same run can capture a cropped screenshot for a named panel or target
  region without hard-coded screen coordinates.
- The same run can export scoped JSON state for a named panel, mesh, or
  registered savable source using the existing `Savable` serialization path.
- Test scripts avoid absolute screen coordinates except for explicit
  pointer-event regression cases.
- `CycleTour` tutorial behavior remains intact.
- Automation failures report the command, reason, current state snapshot, and
  enough context to reproduce the failure locally.
- The same automation core can later be exposed through MCP without redesigning
  command semantics.
