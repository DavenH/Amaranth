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

## Full-width labeled cards

User review found the 40px toolbar grid too small within a dedicated browser.
Replaced it with three columns spanning the sidebar, approximately 14px outer
insets and 6px gaps. At 290 x 962 the cards are 83 x 78 with a 52px icon canvas,
5px top inset and 20px persistent label band. Section headings are larger and
remain right-aligned. Heights adapt to available vertical space (52–78px), keeping
all eight rows visible at the tested 700px height. Existing ragged ordering and two
FX rows remain. Removed hover-only label rendering and its declaration.

NodePalette remains the sole layout owner; rendering uses those hit bounds.
No gesture or mutation changes. Reviewed rendered cards, including long labels
and Fourier symbols, in `/private/tmp/nodes-tab.png`. Both builds pass, as do
31 focused cases / 663 assertions and all 19 native drag/create/undo commands.
Architecture audit, diff and style review pass. NodePalette 200 -> 210 lines,
header 48 -> 52; chrome presentation 261 -> 244, presentation header 166 -> 165.
No scalar math hot-loop changes or new policy boundaries.

## Size and alignment adjustment

User requested a 20% reduction and left alignment. Card width/height and icon
canvas are now 0.8 times the previous values (66.4 x 62.4 cards, 41.6px icons at
the reference size). Rows and headings start at the existing 14px left inset.
Persistent labels retain readable type and fit their one-line label bands.
Reviewed the rendered sidebar; updated geometry/hit assertions for fractional
sizes and left-aligned partial rows. Both builds and palette tests pass. Diff/style
and architecture checks pass; layout and paint ownership are unchanged, with no
new domain logic. NodePalette is 209 lines and chrome presentation 244 lines.

## Group identity and spacing

Increased group separation from 12px to 24px. Each group now carries a muted
3px vertical accent beside its heading and cards: gold Context, blue Transform,
lavender Math, sage Source, rose Control and copper FX. The section catalog owns
these presentation accents; the cached palette rail includes their bounds.
Icons remain monochrome at rest and labels remain visible. Tile/hit geometry
continues to share NodePalette layout. Existing resize and hit tests plus explicit
24px group-gap assertions pass (12 cases, 437 assertions). Both builds, architecture
audit and diff/style checks pass. NodePalette remains 209 lines, header 53,
chrome presentation 247. No new gesture, domain policy or hot-loop math.

## Center the grid, retain left alignment

The three-column card grid is now centered in the sidebar. Short rows and section
headings remain left-aligned to the shared grid origin; category bars sit beside
that origin. Labels increased from 10.5 to 12px. Geometry regression checks the
shared grid center at both workspace sizes. Both builds and 12 palette cases / 437
assertions pass; architecture audit and diff/style checks pass. Production file
sizes remain 209 and 247 lines, with existing layout/render ownership unchanged.
