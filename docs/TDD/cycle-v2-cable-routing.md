# Cycle V2 cable routing

## Status

Complete (revised 2026-10-09 after curve-continuity review). Right-to-left
cables that cross an unrelated node body take one continuous cubic detour above
it. Clear cables keep their original curve even when their source has other
destinations.

## Design

`NodeCanvasSceneSnapshot` is the authoritative cable geometry for painting,
selection, splice targeting, and automation. Routing changes only that scene
product; the graph's edges and their domains remain unchanged. The renderer and
hit tester continue to consume the same path. `NodeCanvasScene::cablePath`
remains the direct path used for pending gestures and inline Pan placement.

For a right-side source feeding a left-side destination, the router first checks
the direct curve against unrelated node bodies. Only a collision triggers a
detour. One cubic bows above the affected row and descends to the destination,
avoiding the abrupt curvature change at the former join. The scene checks the
candidate against node bodies before using it. The renderer's original stroke,
glow, and opacity are
unchanged. Route computation occurs only when the scene snapshot changes;
painting and hit testing reuse it.

## Completion criteria

- Baroque-flute scratch Envelope connections clear all three Trilinear Mesh
  previews at normal zoom.
- A far branch remains individually hittable, and moving a node updates the
  route in the same scene snapshot as its endpoint.
- Direct unobstructed cables retain their original geometry.
- A clear long fan-out is not diverted by a second target below it.
- Screenshot review, focused semantic tests, build, architecture audit, and
  diff/style review pass.

## Verification and architecture review

The checked-in baroque-flute fixture opened successfully and produced
`/private/tmp/cycle-v2-baroque-cable-routing.png`. At fit zoom, the near
connection keeps its direct curve while the farther scratch connections clear
the mesh previews with continuous curves. Focused scene tests and the standalone
`CycleV2` build passed. `git diff --check` and the architecture audit passed.
Clang-tidy reported only existing warnings in the touched files.

`NodeCanvasScene.cpp` grew from 520 to 691 lines. Its responsibilities remain
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
