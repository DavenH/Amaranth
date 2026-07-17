# Cycle V2 Canvas Scene Geometry And Cables

## Status

Complete (2026-07-17).

`NodeCanvasSceneSnapshot` now owns the visible and hit-test cable paths,
endpoints, and endpoint kind. JUCE/OpenGL rendering, automation bounds, edge
selection, and splice targeting consume that shared product. Cable painting is
isolated in `NodeCableRenderer`; `NodeCanvas` no longer constructs cable paths
or resolves attachment endpoints.

Verified by the full Cycle V2 suite (3,298 assertions in 283 cases), the
standalone build, focused cable/scene contracts, and an OS-captured canvas with
ordinary and attachment cables plus an expanded OpenGL editor.

## Problem

`NodeCanvas` still rebuilds port locations, attachment endpoints, cable paths,
edge hit geometry, and cable painting independently of `NodeCanvasScene`.
Rendering, hit testing, automation inspection, and splice targeting can
therefore disagree, while the component retains several hundred lines of
non-coordination code.

## Design

- Make `NodeCanvasSceneSnapshot` the authoritative screen-space product for
  ports and cables, including the visible cable path and endpoint kind.
- Add a stateless `NodeCableRenderer` that paints a scene edge from an explicit
  style value and paints an in-progress connection from explicit geometry.
- Have hit testing, splice targeting, JUCE cable painting, and OpenGL cable
  submission consume the same scene edge.
- Keep graph validation/domain resolution, gesture state, and graph mutation
  in their existing owners. Do not introduce callbacks or a node-kind registry.

## Acceptance

- Standard and dynamic-attachment edges share one path/endpoints across
  rendering and hit testing.
- Splice targeting uses cached scene hit paths and preserves graph-validity
  filtering.
- Cable rendering tests cover ordinary, attachment, invalid, selected, and
  splice-target styles without exact-pixel assertions.
- Cycle V2 tests and standalone build pass; `NodeCanvas` no longer implements
  cable path construction or endpoint resolution.
