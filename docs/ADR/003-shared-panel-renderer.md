# ADR 003: Shared Renderer For Panels

## Status

Proposed

## Context

Cycle currently renders multiple editor panels with per-panel JUCE `OpenGLContext` instances. Each `Panel2D` and `Panel3D` creates its own OpenGL-backed component and attaches its own context.

This has several costs:

- Startup is slow when many panels are created at once.
- Multiple OpenGL render threads increase contention and context-switch overhead.
- GPU resources such as textures and state are duplicated across contexts.
- Resizing and visibility changes can trigger context churn and visible flicker.
- The current rendering code relies on fixed-function OpenGL patterns, which are deprecated on macOS.

At the same time, the application depends heavily on JUCE `Component` behavior for:

- layout and bounds management,
- keyboard and mouse routing,
- focus handling,
- child controls such as zoom panels and effect controls,
- existing editor interaction models.

The goal is to improve graphics performance without discarding JUCE's component system or rewriting all interaction code at once.

## Decision

We will move panel rendering toward a single shared compositor surface, with panels treated as logical render nodes rather than separate native OpenGL contexts.

JUCE will remain the UI framework for component layout and event handling. Heavy panel drawing will move behind a renderer abstraction that can support multiple backends.

The target architecture is:

- one shared renderer owned near `MainPanel`,
- one GPU-backed surface for the panel canvas,
- per-panel logical viewports or cached panel textures within that surface,
- panel interaction and controls still managed through JUCE components,
- backend-neutral rendering interfaces above the platform graphics API.

On macOS, the long-term backend target is Metal. A shared OpenGL compositor may be used as an intermediate migration step if it reduces risk, but per-panel OpenGL contexts will be removed.

## Consequences

### Positive

- Lower startup overhead by reducing context creation and attachment work.
- Less thread thrashing than the current per-panel context model.
- Less duplicated GPU state and texture setup.
- Reduced resize flicker because one surface is resized and repainted coherently.
- Cleaner separation between panel scene generation and backend drawing.
- Preserves JUCE event handling, layout, and child control behavior.
- Creates a path to Metal on macOS without rewriting panel logic a second time.

### Negative

- Requires a non-trivial refactor of the panel rendering stack.
- Some current rendering helpers are too close to legacy OpenGL and will need redesign.
- Shared rendering introduces central scheduling and dirty-region management.
- Offscreen caching must be designed carefully to avoid stale content and excess memory use.
- Plugin-host edge cases may require a fallback renderer path.

## Alternatives Considered

### Keep the current per-panel `OpenGLContext` design

Rejected.

This preserves the current flicker, startup cost, and thread contention problems. It also deepens reliance on deprecated OpenGL paths.

### One large OpenGL context with "virtual child contexts"

Rejected as the primary design.

The useful part of this idea is a single shared surface with per-panel viewports. The "virtual context" abstraction is not needed and would add conceptual complexity without matching how JUCE or graphics APIs actually work.

### Use JUCE software painting for all panel graphics

Rejected for the main canvases.

This would simplify integration but is unlikely to provide the best performance for dense, continuously updated waveform and surface rendering.

### Immediate full rewrite to Metal with no intermediate abstraction

Rejected.

This is higher risk because current panel code is tightly coupled to rendering calls. A renderer abstraction is needed first to make the migration tractable.

## Implementation Plan

### Phase 1: Separate scene generation from backend drawing

- Keep `Panel` as the owner of drawing logic and interaction state.
- Refactor rendering code so panels emit backend-neutral draw data or call a backend-neutral renderer interface.
- Reduce direct dependence on fixed-function OpenGL primitives in shared panel code.

### Phase 2: Introduce a shared panel compositor

- Add a single canvas component under `MainPanel`.
- Render all visible panel canvases through that component.
- Use per-panel clip rectangles and viewport transforms.
- Keep existing JUCE controls, overlays, and non-canvas widgets as child components.

### Phase 3: Add caching and dirty-region tracking

- Cache static background layers, scales, labels, and other infrequently changing panel content.
- Cache 3D surfaces and other expensive panel outputs into per-panel textures.
- Rebuild caches only on data, style, or size changes.

### Phase 4: Move macOS to Metal

- Implement a Metal backend for the shared compositor.
- Keep the renderer interface stable so panel code does not depend on Metal directly.
- Retain a fallback path where needed for development or unsupported environments.

## Notes For This Repository

The current seams that make this migration practical are:

- `Panel::render()` already centralizes most panel draw sequencing.
- `CommonGfx` is an existing graphics abstraction, but it is still too close to legacy OpenGL and should be lifted to a more backend-neutral model.
- `MainPanel` already manages the visible panel set and is the natural owner for a shared compositor.

## Follow-Up Work

- Define the renderer interface to replace direct fixed-function OpenGL assumptions.
- Add a shared canvas component and route panel paint scheduling through it.
- Add dirty flags for panel background, overlays, and surface content.
- Evaluate whether panel hit-testing should remain in lightweight overlay components or move fully into the shared canvas.
- Decide the exact fallback backend strategy for non-macOS platforms and plugin-host compatibility.
