# Cycle V2 Spy card interactions

## Status

In progress.

## Architecture review

`NodeCanvas.cpp` was already above the 1,200-line review threshold and is now
3,079 lines. It coordinates input routing, editor overlays, canvas selection,
document commands, and repaint scheduling. Its stable collaborators for this
slice are `NodeCanvasInteraction`, `NodeCanvasAuthoring`,
`GraphCommandDispatcher`, and `SignalProbeCanvas`. The dispatcher owns graph
mutation and undo; the canvas owns gesture routing and selected-card display.
The selection and movement policy is currently only in `NodeCanvas`.

Extraction plan: move the card selection and drag session into a dedicated
canvas interaction collaborator after this behavior has native fixture proof.
That collaborator should own gesture state and return semantic deltas while
`NodeCanvas` continues to route input and dispatch commands. Delete the current
`ProbeCardGesture` and card-specific drag branches in `NodeCanvas` as part of
that extraction; do not add a second policy site.

The implicit output Spy remains presentation state so existing preset artwork
can keep its output preview. Its visibility, domain, and position persist in
the preset, but the graph undo stack does not currently capture these three
presentation properties. Completing ordinary graph-style undo for the output
Spy requires a presentation delta in `GraphDocument` or a unified output Spy
model; that is the remaining interaction parity work for this TDD.

## Design

`SignalProbeCanvas` owns card geometry and painting. The existing
`NodeCanvasInteraction` node move transaction and `GraphCommandDispatcher`
position delta remain authoritative for group movement. Canvas selection
includes Spy cards, including the output card, while graph node selection
remains in `NodeCanvasAuthoring`. A Spy drag offsets selected cards visually;
selected graph nodes move through the existing node translation command. On
release, all ordinary Spy position deltas join the node move transaction.
No pointer movement copies the graph, serializes state, or prepares audio.
The mature Cycle 1 node move path does one hit/selection lookup on pointer
down, updates only the selected objects on movement, and captures undo once
at commit. This canvas path keeps the same phase costs: selected Spy start
positions are captured once, movement applies the existing incremental node
translation plus a visual Spy offset, and Spy positions commit once on release.

`DefaultOutputProbeResolver` and `DefaultOutputPreview` remain the capture and
FFT authorities. The output preview must continue to exist for preset artwork
even when the canvas Spy is removed. Canvas visibility is a persisted preset
presentation choice. Removing the output card hides its cable tether and
card; the output preview remains available to preset generation. Its card
uses the same Stop Spying action and Delete key as other cards. The special
`out` badge is removed.

Time-domain Spy previews gain a persisted Time/Frequency display property.
Ordinary Spies save it on `SignalProbe`; the implicit output Spy saves it in
`PresetPresentation`. Frequency uses `DefaultOutputPreview::spectrum` at
preview publication, not during paint. Spectral magnitude and phase previews
have no domain action because their source signals lack the complementary
component. Cards display the preview without a title or header. A right-click
menu offers the applicable domain switch and removal.

Escape prioritizes the open Spy detail view over canvas selection, so one
press dismisses it.

## Verification

2026-10-09: `cmake --build --preset standalone-debug --target CycleV2
--parallel 10` passed. Native fixtures
`cycle-v2-agent-spy-card-interactions.json` and
`cycle-v2-agent-spy-card-group-drag.json` passed. The first covers one-press
Escape and output-card removal across save/reload; the second covers mixed
node/Spy movement and undo. Focused Catch2 cases for frequency capture,
display-domain serialization, and output visibility passed (23 assertions).
`git diff --check` passed. The broader `[cycle-v2][probe]` filter retains
three unrelated failures tracked in `audio-bugs.md` and `ui-bugs.md`.

The selected-card paint check captures the same Spy before and after a click
with `cycle-v2-agent-spy-selected-contrast.json`. Run
`swift scripts/check_cycle_v2_spy_selection_contrast.swift
/tmp/spy-unselected.png /tmp/spy-selected.png
/private/tmp/cycle-agent-report.json` to assert the preview brightness is
unchanged. The regression was caused by the tile border colour's alpha being
left as the graphics opacity for the cached preview image. The final fix
belongs in both cached-image draw paths: Spy and ordinary node previews each
draw at full opacity within a scoped graphics state. Both cache tests render
after a low-alpha caller colour to guard against inherited dimming.

The Spy and cable context menus now use the click's screen rectangle after
setting their target component. The focused
`cycle-v2-agent-spy-popup-anchor.json` fixture verifies a non-origin Spy menu
anchor (1649, 492 in the captured run). `AGENTS.md` records the JUCE ordering
rule so future menus do not regress to the canvas origin.

- One area-selection gesture includes graph nodes and Spy cards; dragging
  either selected kind moves the complete group. Undo restores ordinary Spies
  and graph nodes.
- Removing the output Spy persists across save/reload and does not remove
  the preset preview product.
- Right-click domain switching changes a time Spy preview, persists across
  save/reload, and is absent for magnitude and phase signals.
- A native fixture and production-size screenshot verify full-card preview,
  group movement, removal, and one-press Escape.
