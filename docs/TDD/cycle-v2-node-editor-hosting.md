# Cycle 2 Node Editor Hosting

## Status

Proposed.

Depends on `cycle-v2-node-canvas-architecture.md` and
`cycle-v2-concrete-curve-editors.md`.

## Problem

`NodeCanvas` still decides which node kinds have expanded editors, constructs
node-specific widgets and editors, binds graph commands, manages compound
transactions, schedules compilation, triggers OpenGL repainting, and owns
editor lifetime. This keeps node-specific parameter and lifecycle knowledge in
the generic canvas despite the canvas architecture extraction.

## Goals

- Move expanded-editor construction and lifetime into an editor host service.
- Register editor capabilities with node view modules or node definitions.
- Give editors a narrow document-command and presentation invalidation API.
- Keep `NodeCanvas` responsible only for composition, bounds, and forwarding
  the selected node identity.
- Preserve editor reuse, close behavior, automation, undo, and OpenGL lifetime.

## Non-Goals

- Redesign compact node rendering.
- Move graph mutation rules out of `GraphEditor`/`GraphCommandDispatcher`.
- Make every node a child component.
- Redesign expanded-editor geometry.

## Target Design

Introduce an editor capability registered per node kind:

```cpp
class NodeEditorFactory {
public:
    virtual std::unique_ptr<NodeEditor> create(
            const NodeEditorContext&) const = 0;
};

struct NodeEditorContext {
    juce::String nodeId;
    NodeEditorCommands& commands;
    NodeEditorPresentation& presentation;
};
```

`NodeEditorHost` owns the active editor, compatible reuse/rebinding policy,
close handling, bounds, automation inspection, and teardown. It observes
stable node identity rather than holding a raw `Node*` across graph revisions.

The command interface exposes semantic node operations and transaction scope;
it does not expose `NodeCanvas`. The presentation interface coalesces compile,
preview, JUCE repaint, and OpenGL repaint invalidation based on the returned
`GraphChangeSet`.

## Migration Plan

### Slice 1: Capability Registry

- Register expanded-editor factories through node view modules or definitions.
- Replace hard-coded eligible-kind checks in `NodeCanvas`.

### Slice 2: Host And Context

- Extract active-editor ownership, binding, close, bounds, and teardown.
- Introduce command and presentation interfaces without canvas callbacks.

### Slice 3: Canvas Integration

- Make `NodeCanvas` pass selected node identity and editor bounds to the host.
- Route automation editor inspection through the host.
- Remove node-specific editor fields and parameter callbacks from the canvas.

### Slice 4: Invalidation Coalescing

- Derive compilation, preview, and repaint work from consolidated change sets.
- Ensure one semantic editor command schedules one refresh cycle.

## Verification

- A node with no editor capability opens no editor without canvas branching.
- Curve and Trimesh editors are created through registered factories.
- Deleting or changing the active node closes/rebinds safely.
- Undo/redo while an editor is open refreshes the bound editor.
- Editor commands work in tests without constructing `NodeCanvas` or OpenGL.
- Automation observes the same hosted editor state.
- OpenGL resources are created and released on the correct context boundary.
- `NodeCanvas` contains no curve/Trimesh parameter IDs or editor-kind lists.

## Completion Criteria

- Generic canvas code has no node-specific expanded-editor construction.
- Editor lifetime and command binding have an explicit owner.
- Node definitions/view modules advertise editor capability.
- Presentation invalidation is coalesced from graph change impact.

