# Shared Panel Renderer TDD

## Overview

This document describes the technical design for migrating Cycle's panel rendering from multiple per-panel JUCE `OpenGLContext` instances to a shared compositor model.

It is intended to implement [ADR 003](/Users/daven.hughes/CLionProjects/Amaranth/docs/ADR/003-shared-panel-renderer.md).

## Problem Statement

Cycle currently creates one OpenGL-backed JUCE component per major panel. Each of these components owns its own `juce::OpenGLContext`.

This design causes several problems:

- slow startup when many panels are created,
- thread thrashing from multiple renderer threads,
- duplicated GL resource initialization,
- resize flicker and context churn,
- continued dependence on deprecated OpenGL behavior on macOS.

At the same time, Cycle relies on JUCE components for layout, event routing, focus, child controls, and editor interactions. The rendering refactor must preserve those behaviors while replacing the heavy panel drawing path.

## Goals

- Replace per-panel GPU contexts with a single shared rendering surface.
- Preserve existing JUCE component layout and interaction semantics.
- Decouple panel drawing logic from legacy fixed-function OpenGL.
- Introduce clear invalidation and caching rules.
- Create a backend path that can target Metal on macOS.
- Reduce startup cost, resize flicker, and redundant GPU work.

## Non-Goals

- Rewriting all panel interaction code in the first phase.
- Removing JUCE as the UI framework.
- Implementing the final Metal backend in the first milestone.
- Perfect dirty-region optimization before correctness is established.

## Current Architecture

### Panel Structure

The current rendering flow is centered around `Panel::render()`, which sequences background drawing, curves, overlays, vertices, labels, and selection state.

Current key classes:

- `Panel`: shared render sequencing and panel state
- `Panel2D`: 2D curve and intercept rendering
- `Panel3D`: surface rendering and baked textures
- `OpenGLPanel`: 2D OpenGL-backed component
- `OpenGLPanel3D`: 3D OpenGL-backed component
- `CommonGfx`: graphics abstraction used by panel code
- `CommonGL`: OpenGL implementation of `CommonGfx`
- `MainPanel`: layout and visibility owner for the panel set

### Current Pain Points

- Each `OpenGLPanel` and `OpenGLPanel3D` owns a separate `OpenGLContext`.
- GL resource ownership is implicitly tied to per-panel context lifetime.
- Shared panel code still assumes old GL draw modes and stateful immediate rendering.
- `MainPanel` still thinks in terms of attaching individual GL-backed components.
- Repaint behavior is fragmented across many components.

## Constraints

- Existing panel event and interactor behavior should continue to work during migration.
- Existing child controls such as zoom panels, effect controls, and overlays must remain functional.
- Rendering must continue to work in plugin and standalone builds.
- Some panels are much more expensive than others and need caching.
- The migration must allow incremental conversion rather than a flag day rewrite.

## Target Architecture

The target architecture introduces one shared rendering surface and converts panels into logical render nodes.

### High-Level Model

- `MainPanel` owns one shared canvas component.
- Each panel remains a logical editor unit with bounds, state, interactor, and render logic.
- The shared canvas renders all visible panels into panel-specific clip rectangles.
- JUCE child components remain responsible for controls, layout, keyboard focus, and pointer dispatch.
- Backend-specific rendering is hidden behind a neutral renderer interface.

### Main Concepts

#### Logical Panel

A logical panel is the existing `Panel`/`Panel2D`/`Panel3D` hierarchy with rendering no longer bound to its own GPU component or context.

Responsibilities:

- own state, zoom, and interactor references,
- generate panel visuals through the renderer interface,
- expose dirty-state and cache invalidation needs,
- provide bounds and render metadata to the compositor.

#### Shared Canvas

A single GPU-backed or backend-backed component that draws all visible panels each frame.

Responsibilities:

- own the backend device or context,
- schedule frames,
- iterate visible logical panels,
- set panel clip rects and transforms,
- manage shared GPU resources and panel caches.

#### Renderer Interface

A backend-neutral drawing API used by panel code.

Responsibilities:

- accept panel draw operations without exposing GL-specific state,
- support clipping, transforms, textured draws, lines, points, fills, and gradients,
- provide resource creation and invalidation APIs for cached textures and images,
- record panel draw commands in a backend-neutral form that can be replayed or cached by the compositor.

#### Resource Cache

A renderer-owned cache for GPU resources.

Responsibilities:

- map panel images and baked outputs to backend handles,
- manage lifetime independent of panel-local contexts,
- rebuild only when inputs or dimensions change.

## Proposed Class Changes

The exact names may change, but the design should introduce the following roles.

### New or Refactored Types

- `PanelRenderContext`
  - transient per-panel state for one render pass
  - contains panel bounds, clip rect, DPI scale, and viewport transforms

- `PanelRenderer`
  - backend-neutral recordable drawing interface
  - presents an immediate-style API to callers while producing retained command buffers for the compositor

- `PanelCompositor`
  - orchestrates drawing of visible panels
  - owns frame scheduling and dirty panel tracking

- `SharedPanelCanvas`
  - JUCE component that hosts the compositor
  - the only GPU-backed panel surface

- `RenderResourceCache`
  - maps images and baked panel outputs to backend resources

- `PanelDirtyState`
  - explicit invalidation categories for each panel

### Existing Types To Be Reduced

- `OpenGLPanel`
- `OpenGLPanel3D`
- `OpenGLBase`
- `CommonGL`

These may remain temporarily as adapters during migration, but they should no longer define the long-term panel model.

## Proposed Ownership Graph

The following graph describes the intended steady-state ownership and call direction.

```text
MainPanel
  -> SharedPanelCanvas
    -> PanelCompositor
      -> PanelRenderer
      -> RenderResourceCache
      -> VisiblePanelRegistry
        -> PanelRenderNode (logical view of Panel / Panel2D / Panel3D)

Panel / Panel2D / Panel3D
  -> Interactor / Rasterizer state
  -> ZoomPanel
  -> PanelDirtyState
  -> panel-owned CPU-side images and bake inputs

RenderResourceCache
  -> backend texture handles
  -> baked surface cache entries
  -> static layer cache entries
```

### Ownership Rules

- `MainPanel` owns the JUCE child hierarchy and the single shared canvas.
- `SharedPanelCanvas` owns the compositor lifetime.
- `PanelCompositor` owns backend-facing renderer state and cache state.
- `Panel` owns logical draw content and CPU-side state only.
- `Panel` must not own backend resource handles directly in the target design.
- `ZoomPanel` and existing child controls remain JUCE-owned and are not absorbed into the compositor in the initial migration.

## Proposed Runtime Data Flow

The expected steady-state frame flow is:

1. A panel model or interaction change marks one or more dirty categories on a logical panel.
2. The panel or owning code notifies the compositor that panel content is dirty.
3. The compositor coalesces invalidations and schedules one shared canvas frame.
4. On frame execution, the compositor queries visible panels and their bounds from `MainPanel`.
5. For each visible panel:
   - rebuild invalid caches if required,
   - request fresh draw commands or cached textures from the panel path,
   - execute the panel pass inside its clip rect.
6. The shared canvas presents one composed frame.

This data flow should replace direct panel-local GPU repaint assumptions.

## Proposed Transitional Class Mapping

To keep migration incremental, the existing classes should map into the new architecture as follows.

### Existing To Transitional

- `Panel`
  - keeps draw sequencing
  - emits commands through the new `PanelRenderer`

- `Panel2D`
  - keeps geometry and curve logic
  - stops depending on a panel-local render component

- `Panel3D`
  - keeps surface generation and bake-trigger logic
  - delegates baked output ownership to renderer-managed cache entries

- `OpenGLPanel` and `OpenGLPanel3D`
  - become temporary adapters or scaffolding
  - should no longer be the long-term identity of a renderable panel

- `CommonGL`
  - should be folded into the transitional backend adapter or replaced by it

### Transitional To Final

- temporary GL-backed adapter
  - replaced by shared compositor backend implementation

- panel-local GL components
  - replaced by `SharedPanelCanvas` plus optional lightweight input hosts

- panel-local texture ownership
  - replaced by `RenderResourceCache`

## Renderer Interface Design

The renderer interface should use an immediate-style front-end with a recordable command backend.

Panels should continue to issue simple draw calls during migration because that is the lowest-risk conversion path from the existing code. Internally, those calls should be captured as backend-neutral command buffers or draw lists so that:

- panel output can be cached,
- expensive layers can be replayed without rerunning all panel logic,
- the shared compositor can execute a stable retained representation,
- backend substitution does not require changing panel call sites again later.

### Required Capabilities

- begin and end panel render
- set clip rect
- set transform or viewport
- push and pop render state where needed
- draw line
- draw line strip
- draw points
- draw filled rect
- draw outlined rect
- draw gradient fill
- draw textured quad
- draw sub-texture
- upload or invalidate image-backed resources
- draw cached panel texture
- draw baked surface output for 3D panels

### Explicitly Avoid

- exposing GL enums or GL state toggles,
- exposing fixed-function blend or smoothing flags directly,
- assuming textures are bound globally by caller-managed state,
- panel code mutating backend global state outside the renderer API.

### Transitional Strategy

In Phase 1, implement the new interface with a GL-backed adapter. This preserves behavior while removing direct backend assumptions from panel code.

## Dirty State Model

The current booleans should be replaced by a clearer invalidation model.

### Proposed Dirty Categories

- `LayoutDirty`
  - panel bounds or zoom transform changed

- `StaticVisualDirty`
  - background grid, scales, labels, or static decorations changed

- `OverlayDirty`
  - hover, selection, cursor, or temporary interaction state changed

- `SurfaceCacheDirty`
  - 3D surface or expensive baked content must be regenerated

- `ResourceDirty`
  - backing images or resource handles must be rebuilt

- `FullDirty`
  - conservative fallback for large structural changes

### Dirty Propagation Rules

- panel methods should invalidate only the narrowest category possible,
- compositor decides whether a category requires full panel redraw or cache rebuild,
- panel resize upgrades to `LayoutDirty` and usually `StaticVisualDirty`,
- data model changes affecting surfaces upgrade to `SurfaceCacheDirty`,
- hover and mouse-only changes should not invalidate expensive caches.

## Caching Strategy

Performance depends on not redrawing every expensive layer every frame.

### Cacheable Content

- panel backgrounds and grids,
- scales and static labels,
- baked 3D surfaces,
- infrequently changing panel textures,
- possibly curve fills for panels that change only on model edits.

### Non-Cacheable Content

- hover highlights,
- current selection,
- temporary pencil path,
- drag previews,
- live cursor or scrub overlays.

### Cache Ownership

Caches must be owned by the shared renderer, not by panel-local GPU objects.

Each cache entry should be keyed by:

- panel identifier,
- content type,
- bounds or texture size,
- version or invalidation counter.

### Cache Budget

The shared renderer must enforce explicit cache budgets rather than allowing unbounded growth.

Initial policy:

- maintain separate budgets for lightweight static 2D caches and expensive baked surface caches,
- treat large 3D baked surfaces as the primary memory pressure source,
- evict least-recently-used baked surfaces before smaller static layers,
- prefer rebuilding cheap 2D caches over retaining oversized surfaces,
- log cache allocation and eviction in debug builds.

Exact numbers should be tuned after baseline measurement, but the design must make budget and eviction policy central renderer concerns rather than panel-local decisions.

## Event Routing Model

The migration should preserve current interaction behavior during early phases.

### Phase 1 and Early Phase 2 Policy

- keep per-panel JUCE components or lightweight hosts for input,
- use those components for mouse, keyboard, focus, and existing interactor calls,
- make them visually light or fully transparent once the shared canvas draws the panel content.

This avoids rewriting interactor hit-testing immediately.

### Later Option

After the shared compositor is stable, evaluate whether some hit-testing should move into the shared canvas. This is optional and should not block the rendering refactor.

This decision should be revisited after Phase 2 exit criteria are met, or earlier if lightweight input hosts create measurable cost in component tree size, layout traversal, hit-testing, or editor open and resize time.

## MainPanel Integration

`MainPanel` is the natural owner of the shared canvas because it already owns:

- panel layout,
- visibility state,
- panel grouping,
- resize lifecycle,
- view mode changes.

### Integration Steps

- add `SharedPanelCanvas` as a child of `MainPanel`,
- keep existing controls and non-canvas child components,
- register visible panels and bounds with the compositor,
- remove per-panel GL attachment logic after the canvas is active,
- centralize repaint scheduling through the shared canvas.

### Visible Panel Registry

The compositor should not discover panels by traversing the entire JUCE tree each frame.

Instead, `MainPanel` should publish a compact visible-panel registry containing:

- panel identifier,
- logical panel pointer,
- current bounds in canvas space,
- draw order,
- visibility flag,
- whether the panel currently uses a baked-surface path.

This registry should be updated on:

- layout changes,
- tab or view-mode changes,
- panel visibility toggles,
- canvas resize.

## Phase 1 Detailed Plan

Phase 1 separates panel drawing from the old per-panel GL execution model without yet introducing the shared canvas.

### Phase 1 Deliverable

Panels render through a backend-neutral renderer interface, still using a transitional backend adapter if necessary.

### Phase 1 Technical Challenges

#### 1. Untangling panel logic from backend execution

`Panel::render()` is a good sequencing seam, but many draw helpers still assume backend details. The refactor must preserve ordering while redirecting all primitive draws through the new renderer interface.

#### 2. Replacing GL-flavored abstractions

`CommonGfx` is not neutral enough. It still maps to legacy GL concepts and mutable state. The new interface must preserve needed capabilities but eliminate API concepts that will not survive a Metal backend.

#### 3. Resource ownership

Panel images currently become context-local textures. Phase 1 needs to redefine those as renderer-managed resources.

#### 4. 2D and 3D convergence

The renderer abstraction must support both light 2D panels and baked 3D surfaces. The intended model is:

- 2D panels emit normal draw commands,
- 3D panels generate or refresh renderer-managed baked surface textures,
- the shared compositor composites those baked outputs rather than exposing full general-purpose 3D scene APIs through the common panel interface.

If this split is not made explicit, the codebase will drift into two incompatible rendering systems.

#### 5. Maintaining correctness during transition

A GL adapter should let output remain visually stable while panel code is converted incrementally.

### Phase 1 Implementation Steps

1. Add a new backend-neutral renderer interface in `lib/src/UI/Panels/`.
2. Add a transitional GL adapter implementing that interface.
3. Update `Panel` to depend on the new interface rather than directly on the current GL-flavored helper.
4. Move shared draw helpers to the new renderer surface area.
5. Convert `Panel2D` drawing paths.
6. Convert `Panel3D` baked-surface draw paths.
7. Introduce explicit dirty-state tracking and versioning.
8. Move image and texture invalidation into renderer-managed resource APIs.
9. Verify existing visual behavior panel-by-panel.
10. Remove obsolete GL-specific methods that are no longer referenced by panel code.

### Phase 1 Dependency Notes

- Steps 1 and 2 must land before panel conversion begins.
- Step 7 can begin early, but it should settle before Step 8 so dirty-state semantics and resource invalidation line up.
- Steps 5 and 6 can proceed in parallel once Step 3 is stable.
- Step 10 should wait until all converted panels are validated through the adapter path.

### Suggested PR Breakdown For Phase 1

#### PR 1: Renderer Skeleton

- add `PanelRenderer`, `PanelRenderContext`, and `PanelDirtyState`
- add a no-op or minimal backend adapter shell
- no panel behavior changes yet

Goal:

- land the type system and ownership vocabulary first

#### PR 2: Transitional GL Adapter

- implement the new renderer interface on top of the current GL path
- keep existing rendering behavior unchanged
- add basic resource handle abstractions for images and baked outputs

Goal:

- establish a compatibility layer before panel conversion

#### PR 3: Convert Shared Panel Base

- update `Panel` to render through the new renderer interface
- move common draw helpers off direct GL-flavored assumptions
- preserve current sequencing in `Panel::render()`

Goal:

- convert the shared base once rather than panel-by-panel first

#### PR 4: Convert 2D Panels

- migrate `Panel2D` and one or two low-risk concrete panels
- validate curves, overlays, and hover behavior

Goal:

- prove the renderer contract on simpler panels

#### PR 5: Convert 3D Bake Path

- migrate `Panel3D` so baked outputs go through resource-cache ownership
- keep draw output behavior stable through the GL adapter

Goal:

- make the baked-surface contract explicit before compositor work

#### PR 6: Dirty-State And Resource Cleanup

- wire dirty categories through panel invalidation paths
- remove obsolete GL-specific call sites
- add instrumentation for cache rebuilds and invalidation counts

Goal:

- finish Phase 1 with observability rather than just API conversion

### Phase 1 Exit Criteria

- Panel drawing code compiles without depending on legacy GL concepts.
- Panel image resources are renderer-owned rather than context-owned.
- All major panels render correctly through the adapter backend.
- Dirty categories exist and are used consistently.
- No panel logic requires a panel-local `OpenGLContext`.

## Phase 2 Detailed Plan

Phase 2 introduces the single shared compositor surface.

### Phase 2 Deliverable

All visible panel canvases render through one shared canvas component rather than per-panel GPU components.

### Phase 2 Technical Challenges

#### 1. Creating one canvas without breaking interaction

The shared canvas must replace panel-local rendering while leaving current JUCE event paths intact.

#### 2. Correct per-panel clipping and transforms

Each panel needs accurate bounds, zoom transform, and clip region. Mistakes here cause bleeding, flicker, and wrong hit correspondence.

#### 3. Frame scheduling

Repaint requests must stop targeting individual panel surfaces and instead invalidate compositor state. Coalescing multiple invalidations into a single frame is essential.

#### 4. Cache policy for expensive panels

3D and baked panels should not fully redraw every frame. The compositor needs per-panel offscreen cache support.

#### 5. Z-order and overlays

The team needs an explicit rule for which visuals belong in the shared canvas and which remain JUCE child widgets.

#### 6. Resize lifecycle

Resizing should update panel bounds and caches without the current context churn.

### Phase 2 Implementation Steps

1. Add `SharedPanelCanvas` and `PanelCompositor`.
2. Register visible logical panels from `MainPanel`.
3. Make the compositor iterate visible panels and draw them in layout order.
4. Use full-frame redraw initially for correctness.
5. Keep per-panel JUCE components as input hosts, but stop using them as heavy render surfaces.
6. Route panel invalidation to the compositor instead of panel-local `repaint()` behavior where possible.
7. Add per-panel cache textures for expensive content.
8. Remove per-panel GL attach and detach lifecycle from `MainPanel`.
9. Delete or stub old panel-local GPU components once all visible drawing is supplied by the shared canvas.

### Phase 2 Dependency Notes

- Step 1 must establish ownership between the JUCE component and the compositor before repaint migration begins.
- Steps 2 through 4 establish correctness and should be complete before cache optimization work.
- Step 6 is not complete until panel-local repaint traffic is materially reduced.
- Steps 7 through 9 should be staged so there is always one known-good rendering path available during migration.

### Suggested PR Breakdown For Phase 2

#### PR 7: Shared Canvas Skeleton

- add `SharedPanelCanvas`
- add `PanelCompositor`
- register visible logical panels from `MainPanel`
- no cache optimization yet

Goal:

- create one place for future composition while preserving a fallback path

#### PR 8: Full-Frame Shared Composition

- make the shared canvas render visible panels in order
- use full-frame redraw for correctness
- keep lightweight input hosts

Goal:

- achieve the first working single-canvas path

#### PR 9: Repaint Routing Migration

- redirect panel invalidation toward the compositor
- reduce panel-local repaint usage
- instrument frame scheduling and invalidation coalescing

Goal:

- make shared composition the operational path rather than an auxiliary one

#### PR 10: Panel Cache Introduction

- add static-layer caches
- add baked-surface cache entries
- add cache budgeting and eviction

Goal:

- recover performance after correctness is established

#### PR 11: Remove Per-Panel GPU Surface Lifecycle

- remove old panel-local GL attach and detach logic
- delete or stub obsolete component-backed renderer paths

Goal:

- cross the architectural boundary only after the shared path is validated

#### PR 12: Backend Preparation For Metal

- isolate GL-specific compositor assumptions
- ensure resource-cache and command interfaces are backend-neutral

Goal:

- leave the branch ready for backend substitution work

### Phase 2 Exit Criteria

- There is exactly one GPU-backed panel canvas.
- Panel visuals render correctly in the shared surface.
- Existing panel interactions still work.
- Resize flicker from multi-context churn is eliminated.
- Startup no longer creates one renderer context per major panel.

## Backend Strategy

### Transitional Backend

Use a shared OpenGL compositor only as a bridge if it accelerates migration and reduces immediate risk.

### Long-Term Backend

Metal should be the macOS target backend for the shared canvas.

### Cross-Platform Considerations

The renderer interface should allow:

- Metal on macOS,
- a fallback backend on other platforms,
- a development fallback for plugin-host compatibility issues.

The backend choice outside macOS can remain open until the interface and compositor are stable.

## Threading Model

The shared compositor should begin with a conservative threading model:

- JUCE component state, layout, and event routing remain on the message thread,
- panel scene generation and invalidation bookkeeping also begin on the message thread,
- backend draw submission for the shared canvas may occur on the backend's render thread if required by the chosen implementation,
- no panel should own a dedicated renderer thread.

This is the initial design rule, not an open question. It intentionally trades some peak throughput for a simpler and safer migration away from the current many-context model.

### Threading Rules

- panel dirty flags must be safe to mark from existing UI-driven code paths,
- backend resource lifetime must be owned by the compositor rather than individual panels,
- expensive cache rebuilds may move off-thread later only behind explicit synchronization and snapshot boundaries,
- do not introduce background scene mutation until the single-canvas path is correct and profiled.

If profiling later shows message-thread scene generation is too expensive, off-thread preparation can be introduced as a secondary optimization rather than as an initial architectural dependency.

## Success Measurement

Implementation should capture before-and-after measurements for:

- startup time with the normal full panel set,
- number of active renderer contexts or render threads,
- editor resize stability and latency,
- cache hit and miss rates,
- GPU memory retained by panel caches,
- show and hide stability in standalone and plugin builds.

## Testing Strategy

### Correctness Tests

- verify panel bounds map correctly into compositor clip rectangles,
- verify zoom transforms match previous behavior,
- verify overlays and selection visuals appear in the correct z-order,
- verify panel switching and visibility changes do not lose content.

### Performance Tests

- startup time with all panels constructed,
- resize behavior during rapid window drags,
- frame pacing under simultaneous panel updates,
- GPU resource count before and after migration,
- cache rebuild frequency during common editing workflows.

### Regression Areas

- panel hover state,
- zoom panel interaction,
- selection and drag visuals,
- 3D surface rebakes,
- plugin editor show and hide lifecycle,
- panel switching in collapsed and expanded views.

## Risks

- abstraction may be too thin and remain GL-shaped,
- compositor may redraw too much at first and hide expected gains,
- cached panel textures may become stale under edge-case invalidation paths,
- plugin host behavior may expose backend lifecycle bugs,
- preserving old event hosts too long may delay simplification.

## Rollout Strategy

Use staged migration behind implementation toggles during development.

Recommended progression:

1. new renderer interface with GL adapter,
2. one or two low-risk panels through the new path,
3. all panels through the new path,
4. shared canvas introduced with full redraw,
5. panel caches for expensive content,
6. backend swap experimentation on macOS.

## Implementation Toggles

During migration, use narrow development toggles so there is always a way to compare old and new paths.

Suggested toggles:

- `UseSharedPanelRenderer`
  - routes panel drawing through the new renderer interface

- `UseSharedPanelCanvas`
  - enables the compositor canvas path

- `UsePanelCacheDebug`
  - logs cache builds, hits, misses, and evictions

- `ForceFullPanelRedraw`
  - disables cache reuse for debugging correctness

- `UseLegacyPanelGLComponents`
  - temporary fallback path until the shared canvas is stable

These can be compile-time, runtime, or debug-only toggles depending on how invasive the migration becomes.

## Suggested File Layout

Possible landing points for new code:

- `lib/src/UI/Panels/PanelRenderer.h`
- `lib/src/UI/Panels/PanelRenderContext.h`
- `lib/src/UI/Panels/PanelCompositor.h`
- `lib/src/UI/Panels/PanelCompositor.cpp`
- `lib/src/UI/Panels/SharedPanelCanvas.h`
- `lib/src/UI/Panels/SharedPanelCanvas.cpp`
- `lib/src/UI/Panels/RenderResourceCache.h`

Names can change, but the separation of responsibilities should remain.

## Open Questions

- Which non-macOS backend is preferred after the Metal path is defined?
- Should per-panel input hosts remain permanently, or should hit-testing later move into the shared canvas?
- What is the minimum cache granularity needed for the 3D panels to hit performance targets?

## Definition Of Done

This project is complete when:

- panel rendering no longer depends on per-panel `OpenGLContext` instances,
- panel logic is backend-neutral,
- `MainPanel` owns a single shared panel canvas,
- JUCE interaction behavior remains intact,
- resize flicker and startup context thrash are materially reduced,
- the architecture is ready for a Metal backend on macOS.
