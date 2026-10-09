# Cycle V2 cable routing

## Status

Complete (2026-10-09). Right-to-left fan-outs and direct cables that cross an
unrelated node body route through lanes above the affected row.

## Design

`NodeCanvasSceneSnapshot` is the authoritative cable geometry for painting,
selection, splice targeting, and automation. Routing changes only that scene
product; the graph's edges and their domains remain unchanged. The renderer and
hit tester continue to consume the same path. `NodeCanvasScene::cablePath`
remains the direct path used for pending gestures and inline Pan placement.

For a right-side source feeding left-side destinations, the router uses a short
lead from the source, a lane above the node row, and a drop in the gap before
each destination. Fan-out lanes are ordered by destination position to keep
individual edge paths selectable and prevent coincident strokes from becoming
too bright. The renderer removes the broad glow on ordinary fan-out lanes and
reduces their opacity; selection restores full emphasis. An isolated cable
keeps its direct curve unless it crosses an
unrelated node. Route computation occurs only when the scene snapshot changes;
painting and hit testing reuse it.

## Completion criteria

- Baroque-flute scratch Envelope connections clear all three Trilinear Mesh
  previews at normal zoom.
- A far branch remains individually hittable, and moving a node updates the
  route in the same scene snapshot as its endpoint.
- Direct unobstructed cables retain their original geometry.
- Screenshot review, focused semantic tests, build, architecture audit, and
  diff/style review pass.

## Verification and architecture review

The checked-in baroque-flute fixture opened successfully and produced
`/private/tmp/cycle-v2-baroque-cable-routing.png`. At fit zoom, all three
scratch connections run above the mesh previews and remain visually separate.
Five focused CTest cases passed; the standalone `CycleV2` build passed.
`git diff --check` and the architecture audit passed. Clang-tidy reported only
existing warnings in the touched files.

`NodeCanvasScene.cpp` grew from 520 to 713 lines. Its responsibilities remain
port placement, scene target construction, cable geometry, and scene hit
testing. `NodeCableRenderer` owns only stroke styling, while
`NodeCanvasCableLayerCache` owns sprite invalidation. The route planner uses
the existing `GraphEdgeIndex`, the scene's node targets, and the viewport;
there is no graph mutation or second cable geometry source. If route search
expands beyond row corridors, extract routing from the scene builder into a
dedicated scene-geometry collaborator before this file reaches 800 lines.

## Follow-up scope

Routing around arbitrary dense obstacles, leftward feedback connections, and
manual waypoints need a general path search and stable route memory. They are
outside this initial row-routing slice.
