# Nodes in the unified sidebar

Status: implemented.

## Contract and ownership

Nodes joins Curves/Presets/Patterns as an occasional authoring mode. Preserve the
compact 40px icons, right-aligned ragged rows and two FX rows; right-align headings
and center the 126px grid within the sidebar. Remove the permanent right palette
and its reserved layout width. Keep Nodes selected after click/drag creation.

InlinePresetBrowser owns tabs and consumes empty sidebar space. Nodes uses the
same canvas-owned presentation pattern as Curves: the browser paints the header,
while NodeCanvasPresentation paints the body. A narrow hit-test callback translates
sidebar-local points to the existing NodePalette hit geometry. Only palette hits
pass through to canvas routing; blank space remains inert. No interaction algorithm,
graph mutation, rendering implementation or gesture state machine is copied.

NodePalette retains catalog, layout, hover and hit ownership with explicit visible
state. Existing NodeCanvas authoring/interaction owns creation, drag and undo;
creation starts to the right of the sidebar. Hidden palette hits/automation targets
are unavailable. WorkspaceDock remains the sidebar layout owner; canvas utility
bounds now consume its available rectangle without subtracting a right toolbar.
All changes are UI-thread layout/routing; fixed catalog work and existing gesture
complexity are unchanged. No adapter is introduced. Delete right-edge clearance
and palette-specific right placement assumptions.

## Verification

Build standalone and tests; run palette/sidebar/presentation/hit-router cases,
including tab activation, empty-space capture and hidden content. Run the native
semantic direct-palette fixture from the Nodes tab through two drag updates,
release, undo, click creation and undo. Inspect rendered Nodes and Presets states
for clear right canvas, aligned headings, tab legibility and hover labels.

## Completion

- Fourth tab routes to existing palette, with coherent visibility and hit testing.
- Right canvas reclaimed; click/drag and undo preserved.
- Right-aligned headings and icons remain compact inside the sidebar.
- Focused tests, native fixture, visual inspection and style/architecture review.

## Results and review

Standalone-debug CycleV2 and tests CycleV2_tests builds pass with parallel 10.
Focused suite: 37 test cases / 781 assertions. Native direct-palette fixture passes
all 19 commands. Rendered `/private/tmp/nodes-tab.png` confirms tab placement,
right-aligned headings, hover label and clear right canvas. Hidden-palette tests
prove hits and automation targets disappear and return with visibility.

Refactor/style review removed the old right margin and right utility exclusion;
new code delegates existing authoring and layout. No DSP/hot-loop math changed.
Architecture audit and diff whitespace check pass. Size review: NodeCanvas
2955 -> 2962 lines adds tab/geometry orchestration only, with stable collaborators
InlinePresetBrowser, NodePalette, WorkspaceDock, existing authoring and interaction.
The existing canvas extraction plan in refactors.md remains applicable; no node
family policy moved into the canvas. InlinePresetBrowser 709 -> 724 retains tab,
preset and pattern presentation ownership. Chrome presentation 256 -> 261;
hit router 238 -> 237; NodePalette 190 -> 200 and header 44 -> 48. No adapter,
new mutation path or duplicated interaction implementation was introduced.
All completion criteria above are satisfied.
