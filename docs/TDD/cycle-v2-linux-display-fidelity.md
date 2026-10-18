# Cycle V2 Linux display fidelity

Status: implemented

## Problem

On a 1x Ubuntu display, cached node text is softened by a second image resampling
pass and legacy OpenGL curve points retain the framebuffer sizes tuned on a 2x
display. The Trimesh editor also inherits the monitor aspect ratio, producing an
over-wide editor with truncated parameter labels. JUCE's Linux native title bar
does not expose a client area in which its `MenuBarComponent` can be embedded.

## Design

- `NodeCanvasNodeLayerCache` remains authoritative for cached node compositing.
  Cached images are drawn at the reciprocal physical scale on a physical-pixel
  boundary, without stretching them back to fractional logical dimensions.
- `CommonGL` remains authoritative for legacy panel raster primitives. It maps
  the existing Retina-tuned line and point sizes to the active framebuffer
  density; panel geometry and interaction hit targets are unchanged.
- `NodeViewModule` remains authoritative for expanded-editor sizing. Trimesh
  retains its existing proportional height and gains a maximum aspect ratio.
- `TrimeshSidePanelRenderer` retains the existing control layout and reserves
  enough label width for the longest built-in parameter name.
- Keep the native Linux title bar. Moving menus into it would require replacing
  native window decoration with a custom title bar, including move, resize,
  maximise, accessibility, and desktop-theme behavior. That is a separate app
  shell design decision rather than a menu layout adjustment.

## Completion criteria

- Node cache composition is pixel-exact at 1x and 2x.
- Curve points and non-unit strokes are perceptually consistent at 1x and 2x;
  interaction targets do not shrink.
- Trimesh editors never exceed a 1.65:1 aspect ratio and remain centred.
- Vertex parameter labels render without ellipsis at production widths.
- Focused tests, an Ubuntu build, and actual-size Ubuntu screenshots pass review.

## Verification

- `cmake --build --preset tests --parallel 10`
- Focused Canvas cache tests: 116 assertions in 6 test cases
- Focused Trimesh geometry tests: 47 assertions in 7 test cases
- `cmake --build --preset standalone-debug --target CycleV2 --parallel 10`
- Ubuntu actual-size captures: `/tmp/envelope-ui.png` and `/tmp/trimesh-ui.png`

## Stable end state

The density conversion stays at the OpenGL raster boundary and the aspect-ratio
constraint stays in editor sizing. No compatibility adapter or duplicated
rendering behavior is introduced.
