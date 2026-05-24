# ADR 009: Panel Core And Single-Context Hosting

## Status

Proposed

## Context

ADR 003 proposes moving Cycle panel rendering away from one OpenGL context per
panel and toward a shared renderer. Cycle v2 has made the pressure more
concrete: the node editor owns an OpenGL-backed canvas, while bridged Cycle 1
`Panel2D` and `Panel3D` instances currently bring their own `OpenGLPanel` or
`OpenGLPanel3D` child components and their own `juce::OpenGLContext`.

That nested-context model is unstable in practice. Repeatedly opening and
closing an expanded mesh node can leave the 3D panel flickering between stale
background, pure black, and partially initialized overlays. Rails/intercepts
may appear only after an edit because panel geometry and GL resource lifecycle
are coupled to component/context activation rather than to explicit model and
view invalidation.

The useful Cycle 1 code is not the component shell. The useful parts are the
panel and interactor semantics:

- mesh, vertex, cube, morph-position, and rasterization data structures,
- `Panel2D` / `Panel3D` coordinate transforms and drawing sequences,
- `Interactor2D` / `Interactor3D` hit testing, selection, vertex motion, and
  mesh-editing behavior,
- `CubeDisplay` and related morph/vertex visual idioms,
- rasterizer snapshots and surface/curve generation.

The problem is that the current inheritance leaves combine multiple unrelated
responsibilities:

- JUCE component bounds and event source,
- OpenGL context ownership,
- OpenGL renderer callback target,
- GL texture/cache ownership,
- panel drawing backend,
- panel model and interaction state.

Cycle 1 still needs to build and preserve the existing panel UI while this
refactor happens. Cycle v2 needs to render expanded node editors inside one
global node-editor OpenGL context.

## Decision

We will split Cycle panel code into context-free panel cores and host-specific
component/context leaves.

The common panel layer must not own a `juce::OpenGLContext` and must not require
its bounds from `juce::Component::getBounds()`. Instead, it will be given an
explicit render/input context by its host.

The target shape is:

- `PanelCore` or equivalent shared base:
  - owns panel state, rasterizer references, zoom/projection state, and
    interactor-facing behavior,
  - renders through a supplied `CommonGfx` or future `PanelRenderContext`,
  - receives explicit bounds, scale, clip, and local input coordinates,
  - owns CPU-side state only.
- Cycle 1 host leaves:
  - are JUCE components,
  - own per-panel `juce::OpenGLContext` during the transition,
  - pass their component bounds and JUCE mouse events into the panel core,
  - preserve existing Cycle 1 layout, focus, and visual behavior.
- Cycle v2 node-editor host:
  - is the sole JUCE component and OpenGL context owner for the node canvas,
  - computes expanded-node panel bounds from node-editor layout,
  - forwards input to panel cores using local coordinates,
  - renders all visible node panels through the node canvas renderer and a
    context-scoped resource cache.

In short, Cycle 1 leaves may continue to inherit `juce::Component` and own
OpenGL contexts, but the shared panel classes must not. Cycle v2 will not mount
Cycle 1 `OpenGLPanel` children inside the node editor; it will host the same
panel core inside the node editor's single OpenGL context.

## Bounds And Hit Testing

Panel bounds become a host-provided value.

Cycle 1 obtains those bounds from the component:

```cpp
PanelHostContext context;
context.bounds = getLocalBounds().toFloat();
context.scale = openGLContext.getRenderingScale();
panelCore.render(context);
```

Cycle v2 obtains those bounds from node-editor layout:

```cpp
Rectangle<float> panelBounds =
        TrimeshWidget::expandedWavePanelContentBounds(expandedContent);

if (panelBounds.contains(mousePosition)) {
    PanelPointerEvent event;
    event.localPosition = mousePosition - panelBounds.getPosition();
    event.bounds = panelBounds;
    event.modifiers = modifiers;
    panelCore.mouseDown(event);
}
```

Panel code should ask the supplied host context for bounds, not query a JUCE
component. This is what lets the same panel logic run in Cycle 1 components,
Cycle v2 node popups, thumbnails, inspectors, and future compositor surfaces.

## Rendering Interface

The first extraction target should be conservative. Existing panel drawing can
continue to call a `CommonGfx`-style interface, but the concrete implementation
is host supplied:

- Cycle 1 supplies `CommonGL` bound to the panel-local context.
- Cycle v2 supplies a node-canvas `CommonGfx`/GL adapter bound to the one
  active node-editor context.

Over time this should converge with ADR 003's backend-neutral renderer work, but
the immediate goal is to remove component/context ownership from shared panel
logic without rewriting every draw call.

GL texture and surface caches must be context-scoped. A panel core may request a
surface cache or texture update, but the host renderer owns the actual GL
resource handles. This avoids stale texture handles crossing context lifetimes.

## Interaction Interface

Interactors should be reusable as much as possible, but calls that assume Cycle
1 global infrastructure must be made host-aware.

The host-provided interaction context should cover:

- panel bounds and coordinate transforms,
- mouse position in local panel coordinates,
- modifiers and button state,
- repaint or redraw requests,
- mesh edit notifications,
- undo transaction routing,
- optional global update hooks.

Cycle 1 can route those hooks to its existing `Updater`, `EditWatcher`, and
component repaint path. Cycle v2 can route them to the node graph model, node
undo stack, and node canvas redraw scheduler.

## Consequences

### Positive

- Cycle v2 can render node editor panels in a single OpenGL context.
- Cycle 1 can keep its current JUCE panel layout while the common panel code is
  extracted.
- Mature Cycle 1 mesh, rasterizer, panel, interactor, and morph display logic
  remains reusable.
- Panel hit testing becomes explicit and testable because bounds are supplied
  by the host.
- GL resource ownership becomes clear: resources belong to the active renderer
  context, not to shared panel model objects.
- Repeated show/hide of node popups should no longer churn child OpenGL
  contexts or leak stale texture state into new popup openings.

### Negative

- This is a real inheritance and ownership refactor, not a local Cycle v2 patch.
- Some existing `Panel`, `Panel2D`, `Panel3D`, `Interactor2D`, and
  `Interactor3D` methods assume component, context, updater, or repaint
  ownership and will need seams.
- During migration there will be two host leaves: Cycle 1 component hosts and
  Cycle v2 node-editor hosts.
- Texture/cache APIs need care so CPU-side panel state does not accidentally
  own GL handles.

## Alternatives Considered

### Keep embedding Cycle 1 OpenGL panel components in Cycle v2

Rejected.

This is the current bridge and it is fragile. It nests independent OpenGL
contexts inside a GL-backed node canvas and exposes context attach/detach,
texture cache, and repaint ordering problems.

### Rewrite the mesh editor panels from scratch for Cycle v2

Rejected as the immediate approach.

Cycle 1 already has substantial panel and interactor behavior that should be
reused. Rewriting would likely lose subtle mesh editing behavior and slow down
DSP/UI parity work.

### Move all panel rendering to JUCE software painting

Rejected for the main panel path.

It would simplify component hosting, but the waveform, spectrum, and 3D surface
panels are high-density visualizations that should remain GPU-rendered.

## Implementation Plan

### Phase 1: Identify and name the core/host split

- Audit `Panel`, `Panel2D`, `Panel3D`, `OpenGLPanel`, `OpenGLPanel3D`,
  `Interactor2D`, `Interactor3D`, and `CubeDisplay`.
- Classify methods as model/core, drawing, input, host/component, GL-resource,
  or Cycle 1 global-update behavior.
- Define the first minimal host context types for bounds, scale, redraw, input,
  and resource access.

### Phase 2: Extract context-free panel core seams

- Move component-bound queries behind host context accessors.
- Move context activation and GL resource ownership out of shared panel classes.
- Keep Cycle 1 behavior unchanged by wrapping the panel core in component leaves.
- Preserve Cycle 1 panel visuals and interactions after each extraction step.

### Phase 3: Add Cycle v2 node-editor hosts

- Host trimesh 3D and 2D panel cores inside `NodeCanvas` without child
  `OpenGLPanel` components.
- Route node popup bounds and mouse events into the panel cores.
- Store GL textures and baked surfaces in a node-canvas context-scoped cache.
- Replace `TrimeshWidget` mock/fallback panel drawing with real panel-core
  rendering and texture-backed node previews.

### Phase 4: Retire the temporary Cycle v2 OpenGLPanel bridge

- Remove Cycle v2 use of `OpenGLPanel` / `OpenGLPanel3D` child components.
- Keep Cycle 1 component leaves intact.
- Verify repeated popup open/close, morph edits, 2D vertex edits, and 3D rail
  updates in Cycle v2.
- Verify Cycle 1 still builds and visually matches panel baselines.

## Validation

Cycle 1 validation:

- standalone build succeeds,
- representative panel screenshots match existing baselines closely,
- 2D and 3D mesh editing, morph sliders, vertex selection, and rails still
  behave correctly,
- plugin build remains viable if touched by the refactor.

Cycle v2 validation (not applicable for this branch):

- only the node canvas owns an OpenGL context for the node editor,
- expanded mesh popup can be opened and closed repeatedly without black/flicker
  panel states,
- 3D rails/intercepts appear on first open,
- 2D edits update the 3D view live,
- morph slider changes update the surface without jitter from context churn,
- compact node preview uses the same waveform/surface texture data path as the
  expanded editor.

## Follow-Up Work

- Update `docs/TDD/shared-panel-renderer.md` with the concrete core/host split.
- Replace `TrimeshWidget` mock helpers that duplicate panel drawing.
- Port `CubeDisplay` behavior into a reusable morph-cube core or hostable view.
- Define the node-canvas texture cache API for waveform surface previews.
