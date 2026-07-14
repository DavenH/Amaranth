# Cycle 2 Node Canvas And Graph Document Architecture

## Status

Proposed.

Depends on `cycle-v2-node-definition-and-graph-model.md`. Node-specific curve
editor extraction is specified in `cycle-v2-curve-node-models-and-editors.md`.

## Problem

`NodeCanvas` currently owns or coordinates graph state, compilation, runtime
and preview execution, two audio executors, rendering, OpenGL lifecycle,
hit-testing, transforms, node-specific interaction, editor lifetime,
automation, undo/redo, serialization, file operations, selection, and status
presentation. Its implementation exceeds six thousand lines.

This concentration creates several failure modes:

- UI gestures directly understand graph and node-specific semantics
- automation duplicates paths through the visual component
- rendering, hit-testing, and interaction geometry can drift
- undo boundaries are scattered among mouse handlers
- file and graph-document concerns depend on a JUCE component lifetime
- adding a rich node editor expands the canvas itself
- pure graph behavior is difficult to test without constructing UI/runtime
  infrastructure

## Goals

- Make `NodeCanvas` a thin JUCE/OpenGL composition component.
- Give graph document state, commands, undo, persistence, compilation, and
  presentation explicit owners.
- Share one scene/layout model between rendering, hit-testing, automation, and
  accessibility-oriented inspection.
- Move node-specific presentation and interaction behind registered node view
  modules.
- Keep graph semantics in graph commands and validators, not gesture handlers.
- Make viewport, hit-testing, command dispatch, and presentation independently
  testable without an OpenGL context.
- Preserve the current visual behavior and agent automation surface during
  migration.

## Non-Goals

- Rewrite all rendering at once.
- Require every node body to become a JUCE child component.
- Re-enable OpenGL cable rendering or change cable tessellation policy.
- Change graph workflow, gestures, node appearance, or automation command names
  as part of architectural extraction.
- Put node-specific DSP or model behavior into generic canvas services.

## Target Ownership

### GraphDocument

Owns:

- `NodeGraph`
- file identity and dirty state
- graph revision
- command history and undo/redo
- load/save orchestration and migration results
- most recent valid compile/presentation state

It exposes semantic commands and document notifications. It does not depend on
JUCE `Component`, mouse events, OpenGL, or node editor components.

### GraphCommandDispatcher

Maps application and automation requests to the same graph transactions.
Commands include add, remove, move, resize, connect, attach, splice, parameter
change, model edit, and compound editor gestures.

Gesture coalescing belongs here or in command-history policy. A drag begins one
command transaction, updates its pending value, and commits one undo entry.

### NodeCanvasViewport

Owns pan, zoom, world/screen projection, viewport bounds, and snapping policy.
It is a pure value/service object with deterministic tests.

### NodeCanvasScene

Builds immutable presentation geometry for a document revision:

- node and port bounds
- edge endpoints and cable paths
- palette and minimap geometry
- expanded-editor and node-control targets
- selection and hover targets

Rendering and hit-testing consume the same scene snapshot. Node-specific view
modules contribute semantic target IDs rather than canvas-specific callbacks.

### NodeCanvasHitTester

Queries the scene snapshot and returns typed targets such as node, port, edge,
palette item, node action, parameter control, minimap, or empty canvas. It does
not mutate the graph.

### NodeCanvasRenderer

Draws generic grid, edges, node shells, selection, connection previews,
minimap, palette, graph status, and hover information. It consumes scene and
presentation snapshots and contains no graph mutation or file operations.

OpenGL resource ownership may remain in a small render host while extraction is
in progress. Renderer interfaces must make context-bound construction and
release explicit.

### NodeViewModule

Node definitions optionally provide a view module that owns node-specific
compact rendering, expanded-editor construction, control geometry, and
semantic interaction mapping.

```cpp
class NodeViewModule {
public:
    virtual void buildScene(const NodeViewContext&, NodeSceneBuilder&) = 0;
    virtual void renderCompact(const NodeRenderContext&) = 0;
    virtual std::unique_ptr<NodeEditor> createEditor(
            const NodeEditorContext&) = 0;
    virtual std::optional<GraphCommand> commandFor(
            const NodeInteraction&) = 0;
};
```

The exact interface may be split into smaller rendering, editor, and
interaction capabilities. A node without special UI uses the generic renderer.
The generic canvas must not learn Trimesh, Envelope, Waveshaper, IR, or Guide
parameter IDs.

### GraphPresentationModel

Coordinates control-side compilation, resolved domains, validation issues,
preview results, runtime traces, cache keys, and status text for a graph
revision. Expensive work may later be scheduled or coalesced without changing
canvas input contracts.

It rejects stale results by graph and configuration revision. Painting never
initiates audio execution or mutates the document.

### NodeAutomationFacade

Preserves the current agent automation operations but depends on document
commands, scene inspection, editor hosting, and diagnostic services rather than
the visual component's private methods. Pointer-target inspection uses the same
scene snapshot as hit-testing.

## Event Flow

```text
Mouse / keyboard / automation
  -> typed scene target or semantic request
  -> command dispatcher
  -> graph transaction
  -> GraphChangeSet
  -> document revision
  -> compile / preview / repaint scheduling by impact
  -> scene and presentation snapshots
  -> renderer
```

Node editors use the same command dispatcher. They do not mutate copied `Node`
objects and later push arbitrary strings back to the canvas.

## Caching And Lifetimes

- Scene snapshots are keyed by graph revision, viewport revision, and relevant
  presentation revision.
- Preview sprites are keyed by node identity, preview configuration revision,
  output domain, size, and scale factor.
- Node view modules own no document node by raw pointer across revisions.
- Expanded editor hosts bind to stable node identity and close or rebind when
  the node disappears or changes to an incompatible definition.
- OpenGL resources are created and destroyed only on the required context
  boundary.

## Migration Plan

### Slice 1: Viewport Transform

- Extract pan, zoom, projection, snapping, and viewport-centre calculations.
- Characterize current mouse wheel, magnification, and snap behavior.
- Leave `NodeCanvas` as the caller.

### Slice 2: Scene And Hit Testing

- Introduce typed scene targets and shared geometry snapshots.
- Move node, port, edge, palette, minimap, and generic action hit tests.
- Make renderer and automation inspection consume the same geometry.

### Slice 3: Graph Document And Commands

- Move graph ownership, serialization, file state, undo/redo, and compound edit
  boundaries into `GraphDocument` and command services.
- Route canvas and automation mutations through the same commands.
- Remove direct mutable-node access from the canvas.

### Slice 4: Presentation Coordination

- Move compilation, validation status, preview execution, trace capture, and
  refresh coalescing into `GraphPresentationModel`.
- Add revision rejection for stale asynchronous-ready results.
- Keep rendering read-only.

### Slice 5: Node View Modules

- Move generic node-body drawing into the renderer.
- Register Trimesh and curve-node view/editor modules.
- Move node-specific controls, parameter IDs, and attachment menus out of the
  generic canvas.

### Slice 6: Renderer And Automation Facade

- Extract remaining grid, cable, minimap, palette, status, and hover rendering.
- Move public automation methods to a facade while preserving external command
  behavior.
- Reduce `NodeCanvas` to JUCE event translation, rendering host composition,
  and child/editor layout.

## Verification

### Pure Service Tests

- world/screen transforms round-trip across zoom and pan ranges
- snapping behavior matches current node drag fixtures
- renderer and hit tester consume identical node, port, edge, and control
  geometry
- overlapping targets resolve with documented z-order
- scene snapshots invalidate only for relevant revisions

### Document And Command Tests

- each gesture produces one semantic command and intended undo entry
- compound splice and attachment operations are atomic
- canvas and automation requests produce equivalent graph changes
- failed load/edit leaves the active document unchanged
- change impacts schedule only required compile, preview, and repaint work

### UI Regression Tests

- focused Cycle agent fixtures cover add, connect, splice, attach, move,
  parameter edit, expanded editor, undo/redo, load/save, and automation
- before/after screenshots preserve node, cable, palette, minimap, and editor
  layout
- pointer-target automation agrees with actual click handling
- OpenGL lifecycle and cached preview rendering remain stable

## Completion Criteria

- `NodeCanvas` owns no graph serialization, undo history, audio executor, or
  node-specific parameter semantics.
- Rendering and hit-testing share one scene geometry source.
- Canvas and automation changes use the same semantic graph commands.
- Adding a rich node view/editor does not require modifying generic canvas
  interaction or rendering switches.
- Viewport, scene, hit-testing, document, and command behavior have non-UI unit
  coverage.

