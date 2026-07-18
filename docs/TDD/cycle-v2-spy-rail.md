# Cycle V2 Passive Spy Rail

## Status

Implementation in progress as of 2026-07-17. Passive probe storage, preview
routing, cable gestures, traversal ordering, rail presentation, scrolling, and
editor-safe hosting are implemented. Tile reordering, renaming, and the larger
double-click preview remain open and must be completed before this TDD returns
to an implemented state.

Supersedes the canvas-node and node-drop interaction described in
`spy-node.md`. The signal-traversal contract remains authoritative.

## Problem

Spy is passive graph inspection, but Cycle V2 currently presents and compiles
each Spy as an ordinary node fed by another fan-out edge. Several inspection
points therefore consume canvas space, add long cables, and obscure the
processing graph. Expanded editors also cover the place where inspection
feedback is most useful.

## Goals

- Represent inspection separately from processing topology.
- Keep several live traversal previews organized in a persistent bottom rail.
- Attach probes directly to existing signal cables without creating nodes.
- Keep the rail visible while editing Trimesh and curve nodes.
- Preserve immediate feedback without changing audio execution.

## Probe Model

`SignalProbe` is graph metadata, not a `Node`:

```cpp
struct SignalProbe {
    String id;
    String sourceNodeId;
    String sourcePortId;
    String anchorDestNodeId;
    String anchorDestPortId;
    String label;
    float tapPosition { 0.5f };
    int railOrder {};
};
```

The source output address is the semantic subject. The destination address and
normalized tap position only locate its marker on one outgoing cable. There is
at most one probe per source output because all ordinary outgoing cables from
that output carry the same payload.

If the anchored cable disappears, presentation chooses another compatible
cable from that source, then falls back to the source port. If the source node
or port disappears, the probe remains as a disconnected rail entry until it is
reattached or deleted.

Probe records serialize separately from nodes and edges. Cycle V2 presets are
updated directly; the obsolete Spy-node format receives no migration layer.

## Runtime Contract

Probes do not have ports, audio processors, execution steps, dependencies, or
buffers. Adding or removing one cannot change validation, node order, latency,
buffer lifetime, audio samples, or downstream invalidation.

Preview execution indexes captured output payloads by source node and output
port, then emits `SignalProbePreview` values separately from node previews. A
probe renders the exact traversal grid, dimensions, domain, and channel layout
of that output. Lookup is linear in the number of execution outputs plus the
number of probes, rather than scanning the execution plan for every probe.

## Cable Interaction

- Right-click or macOS Control-click a compatible time, magnitude, or phase
  cable to open its contextual menu.
- The menu says `Spy on Signal` when absent and `Stop Spying` when present.
- Option-click toggles the same operation without opening the menu.
- Control-click no longer deletes cables. Cable deletion remains an explicit
  menu/selection command.
- Dragging a tap marker to another compatible cable reattaches its probe.
- A compact numbered and colored marker is always visible at the tap position.
- A marker-to-tile tether is visible only while either endpoint is hovered or
  selected.

Spy is removed from the node palette. Creating and positioning a diagnostic
node is not part of the probe workflow.

## Rail Presentation

The rail is a bottom workspace tray, above all canvas and expanded-editor
content. Its expanded height defaults to 190 pixels, is user-resizable from
120 pixels to 40 percent of workspace height, and is application UI state.

Expanded rail tiles:

- form one horizontally scrollable, reorderable row;
- use the existing traversal-grid renderer;
- show probe number, label, domain, disconnected state, and delete control;
- open a larger preview on double-click;
- publish selection back to the cable marker.

Tile order, labels, and display settings belong to graph content. Rail height
and collapsed state do not.

Collapsed state is one handle showing probe count and disconnected status.
Tiles and their visual rendering stop while collapsed; cable markers remain.

## Expanded Editor Hosting

The editor host receives the canvas content rectangle above the rail, not the
whole workspace. Curve and Trimesh editor JUCE bounds, OpenGL viewport/scissor,
and input capture must not intersect the rail.

Visible probe tiles refresh in the same publication and preview cycle as an
expanded-editor edit. Trimesh morph/vertex edits and Envelope, Guide,
Waveshaper, and IR edits must therefore be observable without closing the
editor.

## Verification

- Graph tests cover toggle, uniqueness, reattachment, disconnection, deletion,
  ordering, serialization, undo/redo, and preset loading.
- Compiler/audio tests prove equal execution plans, buffers, latency, and
  samples with and without probes.
- Preview tests prove exact addressed traversal data and immediate refresh.
- Presentation tests cover rail geometry, collapse, marker identity,
  interaction-only tethers, and editor-safe bounds.
- Native macOS automation covers right-click and Control-click menus,
  Option-click toggling, marker reattachment, rail resizing/collapse, deletion,
  and save/reload.
- An OS capture with a Trimesh editor open demonstrates that the rail remains
  visible and its preview changes after an edit.

## Completion Criteria

- No Spy palette item, canvas card, port, or runtime processing step remains.
- Probes persist and preview the exact source output without affecting audio.
- Dense inspection no longer competes with processing nodes for canvas space.
- Expanded editors never obscure the rail or prevent its feedback.

## Implementation Evidence

- `TestSignalProbe.cpp` covers uniqueness, rejection of non-signal cables,
  serialization, disconnection, exact execution-plan preservation, reattach,
  removal, and undo/redo.
- Graph preview tests cover exact traversal payloads for every probe in the
  bundled eight-probe graph.
- Canvas architecture tests prove that expanded editor bounds remain inside
  the content rectangle reserved above expanded and collapsed rails.
- `cycle-v2-agent-probe-rail-os-screenshot.json` loads the bundled graph,
  asserts all eight probes, opens the Trimesh editor, and supplies the native
  macOS OS-capture target. The verified capture is
  `/private/tmp/cycle-v2-probe-rail-fixed.png`.
- `cycle-v2-agent-probe-rail-empty-os-screenshot.json` covers the zero-probe
  startup case and verifies that the rail collapses instead of reserving an
  empty tray. Its verified capture is
  `/private/tmp/cycle-v2-probe-rail-empty-fixed.png`.
