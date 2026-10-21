# Cycle V2 Group Explanations And Attached Cards

## Status

In Progress. Roadmap PR 2, based on merged master `984d1ff9`.

## Product contract

Groups gain an optional plain-text rationale. Humans read it through the existing
console and edit it through the shared `ExplanationEditor`; agents inspect and
edit the same persisted text. A group information badge and group-menu action
open that editor. No tooltip or automatic popup duplicates console reading.

An attached information card is an explicitly enabled view of that same group
text. It has no independent text, identity, membership or audio role. This slice
supports group attachment only. Node-attached, free and multi-target cards are
explicitly deferred; there is no generic card/attachment database.

Card placement is an optional group-owned world-space offset, separate from the
rationale. Its origin is `NodeGroupPresentation::memberWorldBounds(...).getPosition()`.
This is independent of viewport padding, expanded-frame width and collapse.
Cards remain visible across collapse, follow group movement through the derived
anchor, and do not extend their own group frame/backdrop or affect its geometry.
Moving an individual member can change the derived group origin predictably.

The group menu shows Edit explanation and Show/Hide information card. Show uses
one default offset derived from current member bounds; an enabled card has a
quiet title/header, text body and close action. Header dragging previews locally
and commits one placement edit on mouse-up. Escape cancels that preview. Close
and Delete while the card itself has explicit focus hide its view while retaining
the rationale. Clicking an ordinary node/group clears card focus; its existing
Delete behavior remains authoritative. Empty rationale suppresses card drawing
while retaining saved placement for undo and subsequent editing.

Group pruning, ungrouping and deletion retain their existing ownership. A group
that retains at least two members keeps its note and placement; a pruned/removed
group loses both as part of that group, and existing structural undo restores
both. Save/reload preserves authored text, placement and visibility. Old graphs
with omitted optional fields retain their current behavior.

## Reuse, ownership and boundaries

- `NodeGroup` owns rationale and optional placement, alongside its existing group
  presentation data. `GraphSerializer` owns their optional JSON fields. There is
  no new node kind, port, attachment, runtime operation or DSP branch.
- `GraphExplanationCommands` and `GraphDocument` own validation, application and
  lightweight annotation history. Introduce an explicit Preset/Node/Group target
  instead of interpreting an empty node ID. Extend the existing explanation edit
  history to retain only the changed text or optional placement, not a graph,
  whole group/membership list, mesh or sample state. Existing node/preset command
  convenience methods remain stable thin delegates to that canonical policy.
- Annotation commands publish only `explanationsChanged`. Text and card placement
  advance document history/revision/dirty state but not `NodeGraph` synthesis
  revision. They do not rebuild group/member indexes, compile plans or prepare
  audio/preview resources. Existing explanation undo/redo shortcuts remain.
- `ExplanationEditor` retains local drafts, Save/Cancel, multiline Return, Escape
  and Command+Return. It is not copied or specialized into a second group editor.
  Canvas explanation orchestration carries an explicit target and rejects stale
  editors after target deletion or document replacement.
- `NodeGroupPresentation` remains the group-geometry owner. A small
  `GroupInformationCardPresentation` collaborator owns information-card layout,
  text drawing and hit rectangles. Painting, hit testing and automation consume
  the same bounds. Group anchors are computed once per frame/gesture; a card's
  local preview changes only its offset. Reverse hit order matches paint order.
- `NodeCanvasExplanations` owns card/editor/hover orchestration. The main canvas
  delegates early card hits before group-background movement, then retains all
  existing node/group/probe gestures. No copied group-drag or persistence state
  machine is introduced. Card focus is separate from ordinary node selection.
- Existing App/Workspace adapters translate explicit scope/ID and optional card
  coordinates into these semantic commands; they contain no mutation policy.

## UI design and geometry

The primary task is reading a group's intent while tracing its member signal
path. Group chrome remains subdued navy and square; information uses the existing
14-pixel text family and quiet information-circle vocabulary. The optional card
is subordinate to node controls. Start with roughly 300 world units of width,
10-unit content insets, a 26-unit header and a natural-height body capped at eight
readable lines. Short notes must not leave a large empty panel. Long text clips
with a continuation indication; the console/shared scrollable editor exposes the
complete note. Header drag, close and text-edit hit targets remain distinct.

Before visual implementation, inspect a production-size current group screenshot.
Verify the final rendered footprint, baseline/insets, title/close separation,
normal/narrow windows and representative zooms. A card may overlap canvas content
when explicitly placed there, but its own close/edit/drag targets must remain
consistent with its rendering. No extra persistent help text or card framework
is added to the UI.

## Cost and lifecycle

All editing runs on the message thread. Text typing touches only the existing
local editor. A note commit is O(text length) after indexed group lookup. Card
pointer-down captures the group origin, existing offset and pointer position;
updates are O(1) local preview changes, and commit records one offset delta.
Moving a whole group does not edit its card data. No live update clones or
serializes a graph, copies a group membership vector/text, walks unrelated nodes,
or prepares audio. Rendering may traverse visible groups as it already does;
anchor work scales with group members and is reused within that frame.

Deletion targets are the empty-node-ID scope discriminator in note history,
canvas/automation note routing, and any duplicated note validation. Existing
node/preset external commands remain supported through the same explicit target
owner. No interim compatibility layer or second annotation store is needed.

## Expected production files and size review

- `Graph/NodeGraph.h`, `GraphEditTypes.h`, `GraphDocument.h`,
  `GraphExplanationCommands.cpp`, `GraphSerializer.cpp`, and narrow graph lookup
  support: approximately 140–220 added/changed lines.
- `UI/NodeCanvasExplanations.cpp`, `ExplanationEditor.{h,cpp}` and new
  `GroupInformationCardPresentation.{h,cpp}`: approximately 220–320 lines.
- `NodeCanvas` input/menu/presentation/inspector, `NodeWorkspace` and automation
  forwarding: approximately 100–150 lines of bounded routing.
- Focused annotation/history, group lifecycle, card geometry/gesture and
  representation tests; a coordinated real-app/native fixture and screenshots.

Keep large canvas, serializer and dispatcher owners orchestration-only. Record
actual changed-file sizes and responsibility review before completion. Update
existing explanation map/feature-route entries without growing the fixed source
map budget. Run code-map, architecture, style and whitespace checks.

## Acceptance

- Humans and agents create/read/edit/clear group rationale without opening a node
  editor; generic preset/node explanation behavior remains intact.
- Explicit scope parsing rejects mixed/missing IDs, unknown groups, non-string or
  oversized text, invalid/nonfinite/out-of-bounds placement and active edit
  sessions atomically. No-op edits preserve redo.
- Optional cards appear only after an explicit action/saved placement; empty notes
  suppress their view. They show the same text, move independently, follow group
  movement, stay anchored on collapse/zoom and survive save/reload.
- Two or more local drag updates followed by commit make one undo step; Escape
  leaves durable state untouched. Card focus/close does not steal ordinary
  group/node Delete. Editor drafts and target identity survive or dismiss safely
  across deletion, undo and document replacement.
- Group 3-to-2 membership retains note/card; 2-to-1 pruning, ungroup and auto-pan
  cable deletion remove/restore the group-owned state through existing undo.
- Annotation commit/undo/redo and card drag preserve synthesis graph revision,
  compiled/prepared/runtime revisions and audio. At 0/128 unrelated nodes and
  unrelated mesh/sample resources, counters show no unrelated copying or compile.
- Exact app/tests build, focused semantic/geometry/interaction checks, coordinated
  actual gesture fixture and native screenshot review complete. Record incidental
  failures in the established ledgers. Commit coherent completed slices; do not
  modify parent-workspace state, factory presets or unrelated branches.

## Evidence

Pending implementation and coordinated baseline capture.
