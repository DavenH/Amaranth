# Cycle V2 Canvas Chrome Palette

Status: Implemented (2026-08-28)

## Objective

Give Cycle V2 canvas chrome one semantic colour and interaction-state grammar
so the graph remains visually primary while its palette, utility surfaces, and
workspace dock read as one subordinate application shell.

This is the first app-wide canvas-chrome slice. It standardizes presentation
tokens and state contrast without changing component geometry, graph behavior,
domain colours, or the icon family.

## Current Failures

General-purpose canvas colours are repeated in `NodeCanvas`, the CPU and
OpenGL renderers, cable and Guide presentation, the utility dock, workspace
dock, and palette chrome. Those copies have already diverged:

- the CPU grid uses major/minor colours `0x2f5b6370` and `0x182f363f`;
- OpenGL uses `0x365b6370` and `0x1b2f36ff`, changing both alpha and the minor
  grid's blue channel;
- node, utility, palette, and workspace surfaces name equivalent hierarchy
  levels locally and therefore cannot be reviewed as one system;
- palette resting, hover, and selected states use file-local literals while
  workspace buttons define focus through another local palette; and
- muted text `#8793a1` over the workspace dock's `#26313d` surface has about
  4.23:1 contrast, below the 4.5:1 normal-text target.

The 190-pixel dock is consequently brighter than much of the authoring canvas
despite being supporting chrome. The baseline production capture is
`/private/tmp/cycle-v2-canvas-chrome-before.png`.

## Authoritative Implementations And Boundaries

- `NodeCanvasPresentation` remains authoritative for JUCE canvas and node
  painting.
- `NodeCanvasGlRenderer` remains authoritative for the OpenGL canvas underlay.
  It consumes the exact same background and grid tokens as the JUCE fallback;
  the shared palette does not absorb OpenGL drawing behavior.
- `WorkspaceDock` remains authoritative for Guide/Spy dock layout, focus, tile
  state, and interaction. `GuideCurveShelf` and `SignalProbeRail` retain their
  domain presentation while consuming the dock's general chrome palette.
- `CanvasUtilityDock` remains authoritative for minimap, legend, keyboard, and
  status placement.
- `NodeCanvasChromePresentation` remains authoritative for minimap, legend,
  status, and node-palette painting.
- `EffectPlotPalette` retains plot-specific grid and DSP accent semantics while
  aliasing only the canvas-family background, inset, and text tokens.
- Signal-domain, morph-dimension, Guide-resource, meter warning, curve, and DSP
  data colours remain owned by their domains. They are not generic chrome.

The stable end state is a header-only `CanvasChromePalette` containing named
presentation values and a small generic control-state mapping. Callers consume
those values directly. No adapter, node-kind branch, renderer switchboard, or
runtime ownership is introduced.

## Hierarchy And State Contract

The authoring graph is the primary task and receives the darkest uninterrupted
surface. Supporting surfaces rise in small, ordered steps:

| Role | Value | Purpose |
| --- | --- | --- |
| Canvas background | `#101318` | Largest primary authoring field |
| Inset background | `#11171d` | Plots, tiles, and contained previews |
| Node/control surface | `#171d24` | Ordinary bounded content |
| Dock surface | `#18212a` | Supporting persistent chrome |
| Raised surface | `#202833` | Headers and selected controls |
| Border | `#3d4a58` | Resting structure |
| Strong border | `#8290a2` | Hover and selected emphasis |
| Primary text | `#e2e8ef` | Labels requiring normal prominence |
| Muted text | `#8793a1` | Secondary but still readable labels |
| Keyboard focus | `#79b8ff` | Non-domain focus indication |
| Navigation accent | `#35d6d2` | Viewport/navigation position only |

The dock surface deliberately moves closer to the node/control family. Muted
text must retain at least 4.5:1 contrast on every opaque general chrome surface;
primary text and focus must retain at least 7:1 against the brightest general
surface. Resting borders must remain distinguishable from ordinary surfaces
without competing with selected or focused outlines.

Generic compact controls expose four semantic states:

- resting: ordinary control surface, border, and readable text;
- hovered: a brighter surface and stronger border/text;
- selected: the raised surface and strongest non-focus border;
- focused: an ordinary surface with the dedicated focus border.

Focus is not encoded by hue alone: existing two-pixel or inset focus geometry
remains. Domain colours may still identify data and selected resource tiles,
but they do not replace the keyboard focus token.

## Implementation Slice

1. Add palette tests for exact CPU/OpenGL background identity, ordered surface
   luminance, normal/muted text contrast, focus contrast, and distinct generic
   control states.
2. Add `CanvasChromePalette` and make `EffectPlotPalette` reuse its common
   tokens.
3. Replace general chrome literals in canvas, GL underlay, cables, Guide
   relationships, node cards, minimap/legend/palette, utility surfaces, and the
   workspace dock.
4. Use the shared control-state mapping for node-palette buttons/rows and
   workspace icon buttons while preserving their established geometry and
   actual hit targets.
5. Capture the same production graph after implementation and compare canvas
   priority, dock emphasis, grid parity, labels, focus, and domain colour.

## Negative Boundaries

- Do not change layout dimensions, dock persistence, minimization, keyboard
  geometry, node geometry, or hit targets.
- Do not redesign or replace SVG icons in this palette slice.
- Do not flatten signal-domain or morph colours into general chrome tokens.
- Do not change effect transfer functions, plot data, preview rendering, or
  meter thresholds.
- Do not add appearance policy to `NodeCanvas`, generic graph code, or runtime
  modules.
- Do not test implementation-local literal usage. Tests guard the public
  semantic palette relationships and rendered production state.

## Verification

- Focused tests prove palette values, contrast, state distinction, CPU/GL token
  identity, and the existing canvas/dock geometry contracts.
- Production automation verifies the normal workspace, utility dock, keyboard,
  and workspace dock remain visible and non-overlapping.
- Before/after captures use the same graph, window, scale, and macOS appearance.
- The Standalone Debug target and complete Cycle V2 suite build and run.
- `git diff --check`, style review, hot-loop scalar-math review, and production
  diff/branch review pass before commit.

## Deletion Targets

- General canvas background copies in `NodeCanvas`, `NodeCanvasPresentation`,
  `NodeCanvasGlRenderer`, `NodeCableRenderer`, and
  `GuideRelationshipPresentation`.
- General node surface, header, border, text, and muted-text copies in canvas
  presentation.
- General utility surface and border literals in `CanvasUtilityDock`.
- General workspace background, border, text, tile, and focus copies in
  `WorkspaceDock`, `GuideCurveShelf`, and `SignalProbeRail`.
- General background, border, text, and muted-text copies in the shared node
  preview renderer and Spy detail overlay.
- General palette control-state literals in `NodeCanvasChromePresentation`.

## Completion Criteria

- JUCE and OpenGL canvas underlays consume identical background and grid
  tokens; the former OpenGL minor-grid typo is impossible through local data.
- General canvas chrome consumes one named palette with no remaining local
  copies enumerated by the deletion targets.
- Muted and primary text meet their contrast targets on every opaque shared
  chrome surface.
- Resting, hover, selected, and focus states are visibly and semantically
  distinct without changing input behavior.
- The workspace dock is visually subordinate to graph content while Guide and
  Spy state, domain colour, selection, and focus remain clear.
- Focused tests, automation, screenshot review, standalone build, full suite,
  and style checks complete with no new regression.

## Implementation Evidence

- Added the 71-line header-only `CanvasChromePalette`. It owns only general
  canvas presentation values and the four generic compact-control states; it
  contains no graph, node-kind, parameter, domain, rendering, or interaction
  behavior.
- JUCE and OpenGL now consume the same background, major-grid, and minor-grid
  objects. The divergent OpenGL `0x365b6370` major grid and `0x1b2f36ff` minor
  grid are deleted, including the erroneous blue minor-grid channel.
- Canvas, cables, node shells, minimap, legend, node palette, utility surfaces,
  Guide/Spy shelves, shared node previews, Spy detail, and workspace controls
  consume the shared roles. Effect plots alias only background, inset, and
  label roles while retaining plot grid, accent, DSP state, and enabled-state
  behavior.
- The dock surface changed from `#26313d` to `#18212a`. Muted-text contrast
  improves from about 4.23:1 to 5.21:1 there; the brightest shared surface is
  now the raised surface, where muted text remains 4.76:1, primary text 12.05:1,
  and focus 7.16:1.
- Production changes are 229 lines added and 163 removed across fourteen
  files, including the new palette. Almost all caller churn is literal deletion
  and qualified-token substitution. The largest caller diff is
  `NodeCanvasPresentation.cpp` at 65 changed lines; it decreases to 929 total
  lines. There are zero new node-kind branches, parameter IDs, adapters,
  renderer policies, or domain dependencies.
- Palette tests pass 35 assertions in four cases. Focused canvas presentation
  passes 76 assertions in eight cases, Guide/dock presentation passes 56 in six
  cases, probe/detail presentation passes 43 in six cases, and keyboard
  presentation passes 58 in six cases.
- The stable nine-command production fixture passes with the 190-pixel 50/50
  dock, utility chrome, and 276-by-112 keyboard intact:
  `/private/tmp/cycle-v2-canvas-chrome-report.json`. Its native capture is
  `/private/tmp/cycle-v2-canvas-chrome-after-stable.png`; the matching baseline
  is `/private/tmp/cycle-v2-canvas-chrome-before.png`.
- The populated Guide/Spy fixture passes eleven commands and visibly retains
  the focused Guide tile's two-part border, domain-coloured Spy outlines, and
  graph hierarchy. Report and capture:
  `/private/tmp/cycle-v2-canvas-chrome-guide-focus-report.json` and
  `/private/tmp/cycle-v2-canvas-chrome-guide-focus.png`.
- Delay, Reverb, and Equalizer editor fixtures pass 16, 16, and 18 commands
  respectively after the shared canvas-family aliases. Their reports are
  `/private/tmp/cycle-v2-canvas-chrome-{delay,reverb,equalizer}-report.json`;
  filtered logs contain no assertion or warning beyond JUCE startup.
- Standalone Debug builds successfully with `--parallel 10`. The full Cycle V2
  executable runs 533 cases: 532 pass, and the sole failure remains the
  pre-existing `TestNodeCanvasHitRouter.cpp:66` hover-help assertion recorded in
  `docs/TDD/ui-bugs.md`.
- `git diff --check`, JSON fixture parsing, style review, scalar-math hot-loop
  review, deletion-target search, and production diff review pass. No DSP,
  analysis, rasterization, or per-pixel loop changed; the only `std::pow` added
  is the WCAG luminance calculation in the focused test. Focused clang-tidy was
  unavailable because the configured build has no compilation database and
  `clang-tidy` is not installed.
