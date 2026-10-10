# Cycle V2 Collapsible Node Groups

## Status

Implemented (2026-10-09).

## Contract

- A group is graph presentation data: a stable ID, title, member node IDs, and a collapsed flag. Nodes and edges remain the authoritative audio graph.
- A node belongs to at most one group. Group membership is explicit and persisted with the graph. Loading invalid, duplicate, or missing membership fails.
- Creating, removing, and collapsing a group are dispatcher commands with invertible deltas and one undo step each. These commands affect layout only; they do not compile or prepare audio.
- Expanded groups frame their member nodes. A collapsed group shows a compact card with a diagram of its member nodes and internal connections. Member nodes and internal cables are hidden; external cables terminate on the card.
- The card has an accessible toggle target. A selection of two or more nodes can be grouped from a context menu or keyboard shortcut. Ungroup is available from the group menu.
- Group geometry is derived from each member's presented bounds, including inline pan nodes whose displayed position follows a cable. Moving a member uses the existing node movement delta, with no aggregate graph copy on movement updates.
- The expanded frame contains connector captions as well as node bodies. Its header reserves enough screen space for the title and collapse control at every zoom, and title text ends before the control.
- Collapse and expansion keep the toggle in the same screen position. Dragging empty group background moves all members through one undoable gesture. The expanded backdrop includes visible attached Spy cards. Spy cards and markers attached to a collapsed member are hidden and reappear on expansion; their saved placement remains intact.

## Ownership and reuse

`NodeGraph` owns the membership data. `GraphCommandDispatcher` owns semantic edits and undo publication. `NodeCanvasScene` owns presented targets and cable endpoints. `NodeCanvasPresentation` consumes that scene and graph data to draw the card. Existing node dragging, box selection, and cable rendering remain authoritative. No DSP implementation is copied.

The anchor is the node-derived expanded frame's top-right toggle position. The
compact card shares that top-right corner; dragging the group translates its
member nodes, so the anchor moves with them without another stored position.
The backdrop extends around visible Spy cards while its toggle stays anchored
to the node-derived header. Background dragging reuses the existing multi-node
transaction and inverse delta. It disables node
snapping and splice validation for this gesture, leaving movement updates
proportional to group members and explicitly placed attached Spy cards. The
probe canvas owns the decision to hide or move attached cards; probe data and
audio routing do not change on collapse.

The user creates a group from a multi-node selection with the context menu or
Command/Ctrl+G. Its title can be changed from the group menu. Command/Ctrl+
Shift+G ungroups a selected member. The plus/minus control toggles the compact
card; double-clicking the card also expands it.

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

After sizes: `NodeCanvas.cpp` 3,277 lines, `NodeCanvasPresentation.cpp`
1,472 lines, `GraphSerializer.cpp` 1,322 lines. The new
`NodeGroupPresentation.cpp` is 238 lines after the expanded-frame follow-up.
The canvas increase is gesture/menu
routing and status publication; minimap geometry and rendering are delegated.
`NodeGraph::isNodeVisible` owns the collapsed visibility decision used by the
scene, painting, hit queries, selection, and snapping. No separate layer stores
group state.

## Verification

- `cmake --build --preset tests --target CycleV2_tests --parallel 10` and five
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
- Follow-up: collapsed card bounds and miniatures use the canvas's authoritative
  presented node bounds. A focused inline pan test places the stored node far
  from its cable and verifies the card and external cable stay on the canvas.
  The 14 matching CTest cases and the group UI fixture passed after this change.
- Expanded-frame follow-up: the canvas's existing endpoint renderer supplies
  caption bounds; the group adds a 39-pixel header above the visible content.
  Geometry assertions cover normal, reduced, and minimum zoom. UI fixture and
  OS compositor capture use a Global Input through Output group at
  `/private/tmp/cycle-v2-global-group-expanded-os.png`.
- Anchored-collapse follow-up: `NodeGroupPresentation.cpp` is 253 lines and
  `NodeCanvas.cpp` is 3,286 lines. Presentation alone decides toggle/grabber
  geometry; `SignalProbeCanvas` owns attached-card visibility and explicit
  placement; canvas routing delegates to the existing node-move transaction.
  No additional graph or audio preparation occurs on collapse or drag.
- Seven focused CTest cases passed, including two movement updates, commit,
  undo, and operation-counter checks at 0 and 128 unrelated nodes with 16,384
  unrelated audio samples. The 20-command
  initial group Spy fixture passed: attached Spy
  card hidden/restored, grabber movement, and undo. Collapsed and expanded
  captures are at `/private/tmp/cycle-v2-group-spy-collapsed.png` and
  `/private/tmp/cycle-v2-group-spy-expanded.png`; filtered log had no errors.

## Background-drag and Spy-bound follow-up

The Spy canvas remains authoritative for card position and group attachment.
`NodeGroupPresentation` unions the visible attached-card bounds with its
node-derived frame only for the expanded backdrop. The node-derived toggle
anchor stays fixed on collapse, including when a Spy lies above or to the right
of the member nodes. Canvas input skips Spy cards, node bodies, and ports before
starting the existing group movement gesture from empty backdrop space. The
controls paint above Spy cards where their placement overlaps the header. The
grabber target and drawing are removed. The focused UI fixture now targets the
group background, and the geometry test asserts the expanded backdrop encloses
the attached cards while the toggle remains stationary.
The fixture is `scripts/fixtures/cycle-v2-agent-group-spy-background-drag.json`.
The 26-command fixture passed with collapsed and expanded background drags,
Spy hide/restore, and undo. The nine focused group tests passed with 119
assertions. The standalone and test targets built, `git diff --check` passed,
and the architecture audit reported only the existing size triggers. Clang-tidy
found three pre-existing SignalProbeCanvas warnings outside the changed lines.
