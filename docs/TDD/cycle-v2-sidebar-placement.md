# Browser left, node palette right

Status: implemented.

## Design and ownership

Move the unified Curves/Presets/Patterns column to the left through WorkspaceDock,
its authoritative layout owner. Keep the Spy rail at the bottom, after the left
column. GuideCurveShelf retains its mature scrolling, hit-testing and preview
behavior; mirror its edge extension, collapse arrows and guide tether origin.
NodePalette owns right-edge positioning inside the canvas workspace and retains
its fixed 40px direct tiles. Resize updates that workspace before fitting.
Hover labels open to the left and new nodes appear inside the canvas to the left
of the palette. Creation uses GraphNodeFactory geometry, existing authoring and
existing drag/undo behavior. No graph commands or domain algorithms are copied.

The graph content still spans the workspace for panning/expanded editors. A
separate utility rectangle, composed from WorkspaceDock's editor-available area
and the palette edge, positions keyboard/status/legend and fits the graph between
the two sidebars. Chrome cache validity must include its absolute bounds after
resize. Full editor occlusion and sidebar visibility keep existing policy.

Automation exposes actual palette tile bounds by semantic IDs, avoiding fixed
screen X coordinates. Existing pointer routing then tests drag/creation/undo
without viewport-size assumptions. No adapter or compatibility layer is added.
Layout and tile hit-testing scale only with the fixed palette/catalog. All work
is on the UI thread; movement retains its existing complexity and transactions.

## Geometry and proof

Browser widths, palette geometry, headings and vertical rhythm stay unchanged.
Palette has 18px right inset; labels have a 6px gap on its left. Browser joins the
left window edge, and Spies begin after its width plus the existing dock gap.
Check nonzero workspace origins and multiple sizes, sidebar minimization, graph
fit, native hover and direct palette drag/undo. Verify preset target routing and
Curves/Patterns tab placement. Update direction-sensitive existing tests.

Review trigger: NodeCanvas (large existing orchestrator) receives only layout
composition and automation delegation. Stable collaborators remain WorkspaceDock,
NodePalette, hit router, authoring and viewport. The pre-existing canvas extraction
plan remains outside this placement slice; no new node-family policy belongs here.

## Completion

- Browser/Curves/Patterns left, palette right on load and resize.
- Keyboard/help and fitted graph clear both sides; Spy rail clears browser.
- Hover labels, guide arrows/tethers and node placement face into the canvas.
- Native screenshots and semantic layout/interaction tests pass.
- Refactor/style review, architecture audit, sizes and focused builds complete.

## Verification and responsibility review

- Standalone-debug CycleV2 and tests CycleV2_tests builds pass (`--parallel 10`).
- Focused palette/guide-dock/hit-router/presentation suite: 36 cases, 740 assertions.
- Native direct-palette fixture: all 18 commands pass, including distinct drag
  updates, release, undo, click creation and undo.
- Native screenshot `/private/tmp/sidebar-swap.png` confirms left browser,
  right monochrome palette, fitted graph and keyboard clearance. Curves native
  capture confirms left tab placement and inward collapse arrow. Layout tests
  cover two sizes, a nonzero workspace origin and minimized dock states.
- Architecture audit and `git diff --check` pass. No DSP or scalar math hot loops
  changed. Production diff reviewed for direction assumptions and duplicate policy.
- NodeCanvas.cpp 2,944 -> 2,955 and header 382 -> 384: UI orchestration only;
  WorkspaceDock owns browser/Spy layout, NodePalette owns tile bounds, viewport
  owns transforms, factory owns node geometry, authoring owns creation/undo.
  NodeWorkspace 790 -> 793 delegates automation bounds. Existing canvas extraction
  remains tracked in refactors.md; this change adds no domain branch or adapter.
- Other touched production files: WorkspaceDock 376, GuideCurveShelf 459,
  GuideRelationshipPresentation 191, NodePalette 187/44, chrome presentation 256,
  presentation header 166 and hit router 238 lines. No duplicate lifecycle policy
  or deleted implementation remains in this placement slice.

## Right-justified palette follow-up

All icon rows now end at the same right edge, including short groups and the
single icon on Control's second row. NodePalette remains the sole geometry owner;
rendering, hover and automation reuse its bounds. Rail width stays three columns.
Reviewed the rendered result and updated the existing alignment/hit-test contract.
Both builds pass; palette suite passes 11 cases / 419 assertions. Architecture
audit and diff whitespace check pass; NodePalette.cpp is 190 lines. No new
responsibility, domain policy, or hot-loop math was introduced.
