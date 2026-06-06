# Panel Core Single-Context Hosting TDD

## Overview

This document turns [ADR 009](../ADR/009-panel-core-single-context-hosting.md)
into an implementation plan.

ADR 009 narrows the shared panel renderer work from
[ADR 003](../ADR/003-shared-panel-renderer.md): the immediate goal is not a
full backend rewrite, but a core/host split that lets Cycle 1 keep its current
JUCE/OpenGL panel leaves while Cycle v2 can reuse the same panel logic without
depending on Cycle 1 component globals or nested OpenGL context lifetime.

Implementation experience in the Trilinear Mesh popup refined the practical
direction: Cycle v2 should use normal JUCE child components where that gives
natural focus, hit testing, cursor, and slider semantics. The thing to avoid is
mounting Cycle 1 `OpenGLPanel` / `OpenGLPanel3D` leaves with their own context
and hidden update routing. Real Cycle v2 child controls and host components are
acceptable when their dependencies are explicit and node-local.

## Problem Statement

Cycle panel classes currently mix several responsibilities:

- panel model and zoom state,
- panel draw sequencing,
- `Interactor2D` / `Interactor3D` editing behavior,
- JUCE component bounds and repaint routing,
- per-panel `juce::OpenGLContext` ownership,
- GL texture and baked-surface lifetime.

That works for the Cycle 1 editor because each visible panel is mounted as a
component with its own `OpenGLPanel` or `OpenGLPanel3D` child. It is fragile for
Cycle v2 when expanded node panels are bridged by embedding those OpenGL leaves
inside another OpenGL-backed node canvas. Repeated node popup open/close can
therefore churn nested contexts and leave 3D panels black, stale, or partially
initialized.

The reusable behavior is in the panel and interactor logic. The component and
context leaves are host concerns and must be made replaceable.

## Goals

- Extract a context-free panel core API for 2D and 3D panel behavior.
- Keep Cycle 1 visual and interaction behavior intact during migration.
- Let Cycle v2 host expanded node panels and controls as normal JUCE children
  where useful, while keeping OpenGL context ownership and backend resources out
  of Cycle 1 `OpenGLPanel` leaves.
- Make panel bounds, input coordinates, repaint, undo, and update routing
  explicit host-provided services.
- Move GL texture and surface handles into host/context-scoped caches.
- Preserve the current `CommonGfx` / `PanelRenderer` plateau until a broader
  backend-neutral renderer is ready.
- Treat `SingletonRepo` as reusable Amaranth infrastructure, but keep Cycle v2
  mesh node state and invalidation explicit instead of rediscovering it through
  Cycle 1 globals.

## Non-Goals

- Replacing all panel drawing with Metal in this work.
- Rewriting mesh editing semantics from scratch for Cycle v2.
- Removing Cycle 1 `OpenGLPanel` and `OpenGLPanel3D` leaves before Cycle v2 has
  a stable host path.
- Forcing all controls into a single hand-painted shared canvas.
- Changing rendered visuals intentionally.

## Current Architecture

### Core-Like Classes

- `lib/src/UI/Panels/Panel.h`
- `lib/src/UI/Panels/Panel2D.h`
- `lib/src/UI/Panels/Panel3D.h`
- `lib/src/Inter/Interactor.h`
- `lib/src/Inter/Interactor2D.h`
- `lib/src/Inter/Interactor3D.h`
- `cycle/src/UI/Panels/Morphing/CubeDisplay.h`

These classes own useful panel state, transforms, draw sequencing, selection,
mesh editing, rasterizer access, and morph/vertex visual idioms. They also
still reach through component/context assumptions.

### Host/Context Leaves

- `lib/src/UI/Panels/OpenGLPanel.h`
- `lib/src/UI/Panels/OpenGLPanel3D.h`
- `lib/src/UI/Panels/OpenGLBase.h`
- `lib/src/UI/Panels/CommonGL.h`

These are the Cycle 1-compatible leaves. They own or adapt per-panel OpenGL
contexts, component bounds, repaint, GL callbacks, and `CommonGL`.

### Existing Transitional Shared Renderer Types

- `lib/src/UI/Panels/PanelRenderContext.h`
- `lib/src/UI/Panels/PanelRenderer.h`
- `lib/src/UI/Panels/PanelCompositor.h`
- `lib/src/UI/Panels/SharedPanelCanvas.h`
- `lib/src/UI/Panels/PanelDirtyState.h`
- `lib/src/UI/Panels/RenderResourceCache.h`

These are useful scaffolding, but they do not yet complete the ADR 009
core/host split. The implementation should extend these where they are already
the right abstraction rather than introduce duplicate concepts.

## Target Architecture

### Panel Core

`PanelCore` is the role currently occupied by much of `Panel`, `Panel2D`, and
`Panel3D`. The exact class names can remain incremental, but the final shared
panel layer must:

- own CPU-side panel state, zoom state, rasterizer references, and interactor
  coordination,
- render through a host-supplied `PanelRenderer` or `CommonGfx` adapter,
- receive explicit bounds, clip, scale, and input coordinates,
- request redraw/update/undo through host callbacks,
- avoid owning `juce::OpenGLContext`, `OpenGLBase`, or backend texture handles,
- avoid querying `juce::Component::getBounds()`, `getWidth()`, `getHeight()`,
  or `repaint()` directly from shared logic.

### Panel Host

`PanelHost` is the replaceable leaf around a panel core. There are two first
hosts:

- Cycle 1 component host: owns the JUCE component and the transitional
  per-panel OpenGL context, then passes component bounds and mouse events into
  the core.
- Cycle v2 node-editor host: may be a JUCE child component for natural focus
  and hit testing, but it does not inherit Cycle 1 `OpenGLPanel` behavior or
  hide Cycle 1 update routing. It computes panel bounds from node layout,
  forwards local input, and renders through either a Cycle v2 host renderer or
  an explicit transitional panel adapter.

Cycle v2 control leaves such as morph-cube views, horizontal sliders,
primary-axis buttons, and vertex-parameter rows should be copied or adapted
from Cycle 1 when their visual language is useful. Those copies must bind to
explicit node-local state and callbacks rather than to Cycle 1 scratch-channel,
console-message, settings, or fixed-update-source globals.

### Host Context Types

The implementation should converge on small context structs instead of broad
inheritance hooks:

```text
PanelHostContext
  bounds
  clip
  scale
  panelId
  visibility
  renderer
  resourceCache
  callbacks

PanelPointerEvent
  localPosition
  bounds
  modifiers
  buttonState
  clickCount

PanelHostCallbacks
  requestRepaint(panel, dirtyFlag)
  beginUndoTransaction(name)
  notifyMeshEdited(updateType)
  notifyZoomChanged(updateSource)
  optionalCycle1UpdateHook(updateSource)
```

`PanelRenderContext` should either become the render subset of
`PanelHostContext` or be embedded by it. Avoid growing two unrelated context
families.

### Resource Ownership

Panel cores may own CPU images, baked inputs, and dirty state. They must not own
backend texture handles as long-lived shared state. GL handles belong to the
active host renderer context:

```text
Cycle 1 OpenGLPanel host
  -> CommonGL
  -> context-local RenderResourceCache
  -> Panel core CPU state

Cycle v2 NodeCanvas host
  -> node-canvas CommonGfx / PanelRenderer adapter
  -> node-canvas RenderResourceCache
  -> Panel core CPU state
```

## Implementation Plan

### Phase 0: Inventory and Safety Rails

Output: a checked-in audit section or issue notes before broad edits.

- Classify `Panel`, `Panel2D`, `Panel3D`, `OpenGLPanel`, `OpenGLPanel3D`,
  `Interactor2D`, `Interactor3D`, and `CubeDisplay` methods as core, render,
  input, host/component, GL resource, or Cycle 1 update routing.
- Identify all direct shared-layer calls to component bounds, visibility,
  repaint, context activation, and GL resources.
- Identify existing v2 bridge code that mounts Cycle 1 OpenGL panel components
  inside node editor UI.
- Add narrow tests around `PanelCompositor`, `PanelDirtyState`, and any new
  context coordinate conversion helpers before moving behavior behind them.

Exit criteria:

- The migration hotspots are listed by file.
- The first host context API is named.
- Existing standalone tests still pass.

### Phase 1: Make Bounds Explicit

Output: panels can be rendered and hit-tested with supplied bounds while Cycle 1
still derives those bounds from components.

- Extend `PanelRenderContext` or introduce `PanelHostContext` with explicit
  bounds, clip, scale, and host callbacks.
- Change render paths that only need dimensions to consume supplied bounds.
- Keep legacy `Panel::getWidth()`, `Panel::getHeight()`, and `Panel::getBounds()`
  as compatibility wrappers during the transition.
- Add coordinate conversion helpers for host-to-panel local input.
- Route Cycle 1 component resize through context updates instead of letting core
  code rediscover bounds opportunistically.

Exit criteria:

- Cycle 1 panels build and behave unchanged.
- Unit tests cover local coordinate conversion and dirty-bound collection.
- New panel-core code paths do not require a mounted JUCE component to compute
  bounds.

### Phase 2: Split Input Routing From JUCE Events

Output: interactors can consume host-neutral pointer events, with JUCE mouse
events adapted at the leaves.

- Introduce `PanelPointerEvent` or equivalent for local position, modifiers,
  buttons, and click count.
- Add adapter functions from `juce::MouseEvent` in Cycle 1 host leaves.
- Move interactor methods that only use event position/modifiers onto
  host-neutral overloads.
- Keep existing `juce::MouseEvent` overloads as wrappers until callers are
  drained.
- Route repaint, mesh edit notifications, and undo begin/end through host
  callbacks rather than direct Cycle 1 globals where possible.

Exit criteria:

- Existing Cycle 1 mouse workflows still work.
- New unit tests cover 2D and 3D local input conversion for representative
  panel bounds.
- Cycle v2 code can invoke core input methods without constructing child JUCE
  panel components.

### Phase 3: Separate Context Activation and Render Backend

Output: panel cores render through a supplied renderer; only host leaves own or
activate OpenGL contexts.

- Keep `CommonGfx` / `PanelRenderer` as the transitional drawing API.
- Move calls to `activateContext()`, `deactivateContext()`, and `clear()` out of
  shared panel logic and into host leaves.
- Make `Panel::render()` and the 2D/3D draw sequences accept a render context or
  an explicitly supplied renderer.
- Keep Cycle 1 `OpenGLPanel` and `OpenGLPanel3D` as adapters that bind
  `CommonGL`, create the render context, and call the core.
- Add a test renderer or spy renderer for core render sequencing that does not
  require GL.

Exit criteria:

- No shared panel core method owns or activates a `juce::OpenGLContext`.
- Cycle 1 standalone build succeeds.
- A non-GL test can exercise at least one 2D render sequence and one 3D render
  setup path.

### Phase 4: Context-Scoped Resources

Output: GL resources are owned by the active host context cache, not by shared
panel state.

- Audit `Texture`, baked surface, name texture, scale texture, background
  texture, and surface cache ownership.
- Move backend handle creation/update into `RenderResourceCache` or a
  context-owned successor.
- Keep panel-owned CPU images and dirty flags in the core.
- Add cache invalidation categories for static visuals, surface data, transform
  changes, and full rebuilds.
- Ensure context close destroys only host cache resources and leaves panel CPU
  state reusable.

Exit criteria:

- Context close/open does not require recreating panel core objects.
- Reopening a Cycle 1 panel recreates GL handles from CPU state.
- Tests cover cache versioning and dirty flag clearing after cache rebuild.

### Phase 5: Add Cycle v2 Node-Editor Panel Hosts And Controls

Output: expanded node panels and controls use natural component semantics while
still avoiding Cycle 1 OpenGL leaves and hidden global state.

- Add a node-editor host adapter for the 3D trimesh panel core.
- Add a node-editor host adapter for the 2D waveform/curve panel core.
- Add copied/adapted Cycle v2 leaves for morph cube display, horizontal morph
  sliders, primary-axis controls, selected vertex parameters, and domain-profile
  controls where the Cycle 1 widgets are the right interaction reference.
- Compute panel bounds from expanded node layout and pass local coordinates into
  the core.
- Bind the node canvas renderer, Cycle v2 host renderer, or explicit
  transitional `CommonGfx` adapter as the host renderer.
- Store panel textures and baked surfaces in the node-canvas resource cache.
- Route node edits through the node graph model and node undo stack.
- Route scratch-channel, guide attachment, console/status, and update
  notifications through Cycle v2 services or callbacks rather than Cycle 1
  globals.
- Replace mock/fallback `TrimeshWidget` panel drawing with real panel-core
  rendering once parity is good enough.

Exit criteria:

- Cycle v2 expanded mesh node uses no Cycle 1 `OpenGLPanel` or
  `OpenGLPanel3D` child leaves.
- Cycle v2 child components have correct focus, hit testing, cursors, and
  slider behavior before and after popup open/close.
- OpenGL context ownership is explicit and does not depend on nested Cycle 1
  panel leaves.
- Opening the same expanded node repeatedly shows initialized 2D and 3D panels
  on first paint.

### Phase 6: Introduce Cycle v2 Update-Graph Invalidation

Output: bridge-local repaint and rasterizer special cases are replaced by
declared update dependencies.

- Add update nodes or revision categories for mesh content, morph position,
  primary view axis, 2D slice rasterization, 3D surface grid, intercept/rail
  overlays, compact preview, expanded panel textures, selected controls, and
  prepared DSP state.
- Encode invalidation edges for 2D edits, 3D edits, morph slider edits,
  primary-axis edits, traversal-domain changes, connection changes, scratch or
  guide input changes, and preview viewport changes.
- Preserve the known optimization declaratively: a morph-slider change along
  the current primary view axis does not rebuild the 3D surface grid, but it
  can still update 2D slice state, controls, and downstream previews.
- Route scratch-channel, console/status, and edit notifications through Cycle 2
  graph/update services.
- Remove bridge-specific manual repaint/update branches once equivalent update
  graph edges exist.

Exit criteria:

- The Trilinear Mesh popup and compact preview stay coherent after 2D edits,
  3D edits, morph slider changes, primary-axis changes, and traversal-domain
  changes without bridge-local special cases.
- Tests cover the invalidation decision for primary-axis slider changes.
- Cycle 1 standalone still builds and retains existing panel behavior.
- Remaining bridge code is either host plumbing or marked as explicit fallback.

### Phase 7: Retire The Temporary Bridge

Output: Cycle v2 no longer depends on the temporary Trimesh bridge for core
rendering or invalidation.

- Remove v2 construction or mounting of Cycle 1 `OpenGLPanel` /
  `OpenGLPanel3D` leaves if any remain.
- Keep Cycle 1 component leaves intact.
- Delete bridge-specific fallback drawing only after real panel-core rendering
  and update-graph invalidation cover the same workflows.
- Update ADR/TDD notes with any remaining shared-renderer follow-up that belongs
  to ADR 003 rather than ADR 009.

Exit criteria:

- Cycle v2 node editor does not create nested Cycle 1 GL contexts for expanded
  panels.
- Cycle 1 standalone still builds and retains existing panel behavior.
- The old v2 bridge path is removed or disabled behind an explicit fallback.

## Testing Plan

### Unit Tests

Add or extend tests under `lib/tests/` or `cycle/tests/` for:

- host-to-panel coordinate conversion,
- `PanelCompositor` visible entries and dirty bounds,
- `PanelDirtyState` category transitions,
- render-cache versioning and invalidation,
- host callback routing for repaint and edit notifications,
- non-GL render sequencing using a spy `PanelRenderer`.

### Integration Checks

- `cmake --preset tests && cmake --build --preset tests --parallel 10`
- `ctest --test-dir build/tests -V`
- `cmake --preset standalone-debug && cmake --build --preset standalone-debug --parallel 10`

Use plugin-debug builds when touched files affect plugin editor lifetime or
shared JUCE/OpenGL code.

### UI Regression Checks

For Cycle 1 visual changes:

- Capture before/after screenshots with
  `scripts/capture_cycle_ui.sh /tmp/cycle-ui.png /tmp/cycle-logs.txt`.
- Inspect `/tmp/cycle-logs.txt` first.
- Verify 2D/3D mesh editing, morph sliders, vertex selection, rails, intercepts,
  and zoom still behave normally.

For Cycle v2 node-editor changes:

- Prefer focused fixtures with `scripts/run_cycle_agent.sh`.
- Add stable fixtures under `scripts/fixtures/` for repeated expanded-node
  open/close and real panel-core rendering.
- Verify first-open 3D rails/intercepts, live 2D-to-3D updates, morph slider
  updates, compact preview updates, and repeated popup open/close without
  black/flicker states.
- Verify copied/adapted component controls have correct focus, hover cursors,
  drag behavior, and no event leakage to the canvas behind the popup.
- Verify domain profiles visually distinguish Waveform2D/Waveform3D backgrounds
  from Spectrum2D/Spectrum3D backgrounds, including spectral surface
  low-amplitude transparency.

## Migration Rules

- Keep each phase shippable.
- Preserve Cycle 1 behavior while extracting shared core APIs.
- Do not let panel cores gain new knowledge of v2 node-editor classes.
- Do not let host leaves reach into panel internals except through explicit
  transitional friend access already present in the code.
- Prefer adapting existing `PanelRenderContext`, `PanelRenderer`, and
  `RenderResourceCache` scaffolding over adding parallel abstractions.
- Prefer real Cycle v2 child components for controls where JUCE focus,
  hit-testing, and cursor semantics solve problems directly.
- Do not copy Cycle 1 leaf widgets together with hidden Cycle 1 globals; copy
  the interaction and drawing language, then bind it to explicit Cycle v2
  services, node state, and update-graph callbacks.
- Record incidental UI or audio runtime failures in the appropriate
  `docs/TDD/*-bugs.md` file if they are discovered but not fixed in the same
  change.

## Open Questions

- Should the final class be named `PanelCore`, or should existing `Panel`,
  `Panel2D`, and `Panel3D` become the core classes after host concerns are
  drained?
- Should `CommonGfx` survive as the primary transitional interface, or should
  all new host work target `PanelRenderer` directly?
- Which Cycle v2 node editor class should own the panel-core host registry and
  resource cache?
- How much Cycle 1 global update routing must remain as callbacks before the
  node graph model can provide equivalent hooks?
- Should Trimesh render profiles live under the Trimesh node module first, or
  should they immediately become generic waveform/spectrum panel profiles for
  reuse by spy nodes and future source nodes?
