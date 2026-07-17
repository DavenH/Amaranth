# Cycle V2 Node Canvas Orchestrator

## Status

Complete.

## Problem

`NodeCanvas.cpp` remains over 3,200 lines after preview, automation-inspection,
compact-editor, palette, and cable extractions. It still owns drawing,
interaction state machines, scene queries, persistence, automation mutations,
and graph-authoring workflows. The file is difficult to navigate and unsafe to
edit because unrelated responsibilities share one component.

## Design

Reduce `NodeCanvas` to the JUCE/OpenGL lifecycle and composition root:

- `NodeCanvasPresentation` paints canvas chrome and node presentation from
  explicit scene/presentation state;
- `NodeCanvasInteraction` owns typed gesture state, hit routing, snapping, and
  connection targeting;
- `NodeCanvasAuthoring` owns graph edit, persistence, splice/layout, and
  automation mutation workflows;
- existing editor, preview, palette, viewport, scene, and invalidation objects
  remain cohesive collaborators.

The completed boundary also uses focused collaborators for semantic graph
queries, canvas hit routing, expanded-editor coordination, and automation IPC.
These keep the composition root explicit without turning presentation,
interaction, or authoring into catch-all services.

Do not replace the file with generic callback bundles, a universal node-kind
switchboard, or another oversized helper translation unit.

## Acceptance

- `NodeCanvas.cpp` is approximately 800 lines and reads as lifecycle plus named
  delegation; no extracted production translation unit exceeds the same review
  threshold without a documented cohesive reason.
- Rendering, interaction, and authoring behavior retain semantic unit coverage.
- Live authoring smoke covers creation, connection, movement, edge insertion,
  parameter editing, undo/redo, and deletion through JUCE/macOS event delivery.
- Full Cycle V2 tests, standalone build, and OS canvas capture pass.
