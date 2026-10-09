# Cycle V2 Collapsible Node Groups

## Status

Implemented (2026-10-09).

## Contract

- A group is graph presentation data: a stable ID, title, member node IDs, and a collapsed flag. Nodes and edges remain the authoritative audio graph.
- A node belongs to at most one group. Group membership is explicit and persisted with the graph. Loading invalid, duplicate, or missing membership fails.
- Creating, removing, and collapsing a group are dispatcher commands with invertible deltas and one undo step each. These commands affect layout only; they do not compile or prepare audio.
- Expanded groups frame their member nodes. A collapsed group shows a compact card with a diagram of its member nodes and internal connections. Member nodes and internal cables are hidden; external cables terminate on the card.
- The card has an accessible toggle target. A selection of two or more nodes can be grouped from a context menu or keyboard shortcut. Ungroup is available from the group menu.
- Group geometry is derived from member bounds. Moving a member uses the existing node movement delta, with no aggregate graph copy on movement updates.

## Ownership and reuse

`NodeGraph` owns the membership data. `GraphCommandDispatcher` owns semantic edits and undo publication. `NodeCanvasScene` owns presented targets and cable endpoints. `NodeCanvasPresentation` consumes that scene and graph data to draw the card. Existing node dragging, box selection, and cable rendering remain authoritative. No DSP implementation is copied.

The user creates a group from a multi-node selection with the context menu or
Command/Ctrl+G. Its title can be changed from the group menu. Command/Ctrl+
Shift+G ungroups a selected member. The plus/minus control toggles the compact
card; clicking the card also expands it.

## Completion evidence

- Group, collapse, expand, ungroup, save/load, undo/redo, and node deletion preserve membership and signal behavior.
- A focused UI fixture groups a box selection, collapses the group, checks the compact diagram and external cables, then expands it.
- Scene and command tests assert hidden member targets/internal cables and restored expanded targets, including boundary cables.
- Architecture audit, changed-file size review, style check, focused tests, and UI screenshot review pass.

## Architecture review

Baseline sizes before this change: `NodeCanvas.cpp` 3,113 lines,
`NodeCanvasPresentation.cpp` 1,447 lines, `GraphSerializer.cpp` 1,269 lines.
These are existing plan triggers. This slice adds only gesture routing and menu
dispatch to `NodeCanvas`; group layout and drawing live in
`NodeGroupPresentation`, while membership rules live in `NodeGraph`. The
canvas still owns popup placement and keyboard dispatch, matching its existing
collaborators. The stable deletion target for a future canvas extraction is to
move all context-menu construction and keyboard action mapping into a canvas
command controller, including the pre-existing edge and probe menus; extracting
only group menus would add another thin facade without reducing the root's
responsibilities. `GraphSerializer` remains the authoritative graph schema
codec; group encoding is a small peer of nodes/edges, with no second codec.

Policy sites: `NodeGraph` alone validates membership, `GraphCommandDispatcher`
alone owns undoable group mutations, `NodeGroupPresentation` alone defines
group/card geometry, and `NodeCanvasScene` alone decides which member targets
and cables are visible. Canvas input reads shared geometry and delegates to the
dispatcher. Group edits do not add DSP or compiler branches.

After sizes: `NodeCanvas.cpp` 3,276 lines, `NodeCanvasPresentation.cpp`
1,472 lines, `GraphSerializer.cpp` 1,322 lines. The new
`NodeGroupPresentation.cpp` is 190 lines. The canvas increase is gesture/menu
routing and status publication; minimap geometry and rendering are delegated.
`NodeGraph::isNodeVisible` owns the collapsed visibility decision used by the
scene, painting, hit queries, selection, and snapping. No separate layer stores
group state.

## Verification

- `cmake --build --preset tests --target CycleV2_tests --parallel 10` and four
  focused CTest cases passed. They cover persistence and undo/redo, membership
  cleanup and restoration, hidden targets/internal cables, external cable
  continuity, and hidden-node box selection.
- `cmake --build --preset standalone-debug --target CycleV2 --parallel 10`
  passed. `scripts/fixtures/cycle-v2-agent-node-groups.json` passed against
  the built app with box selection, grouping, collapse, expansion, and ungroup.
- In-app compact and expanded screenshots are at
  `/private/tmp/cycle-v2-node-group-collapsed.png` and
  `/private/tmp/cycle-v2-node-group-expanded.png`. OS compositor review used
  `/private/tmp/cycle-v2-node-group-os.png`; the compact map, toggle, and
  boundary cables were visible. The filtered launch log contained no errors.
- `python3 scripts/cycle_v2_architecture_audit.py`, touched-file line counts,
  `git diff --check`, and clang-tidy on the new presentation source were run.
