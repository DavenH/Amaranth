# Direct node palette

Status: implemented and verified (2026-10-08).

## Contract and ownership

NodePalette owns the authored item list and fixed screen-space layout. Replace
category hover pullouts with always-visible, left-aligned ragged rows. Remove
Channel authoring choices. Preserve graph types and existing document loading.
NodeCanvas continues to delegate creation to its existing authoring service and
dragging to NodeCanvasInteraction; no graph mutations or gesture algorithms move.
NodeIconRenderer remains the authoritative cached SVG renderer. No new adapter.
Automation enumerates NodePalette rather than maintaining a second item list.
All work is on the message thread; item hit lookup remains O(number of palette
items), independent of graph size. Hover only changes chrome; creation, movement,
commit and undo retain the existing command boundaries and costs.

## Geometry

Six groups: Context (1), Transform (2), Math (2), Source (3), Control (4), FX (6).
Three columns maximum; FX and Control wrap to two rows. Left edge 18, top 74.
Tiles 72 x 58, gaps 4, heading 18, group gap 8 logical pixels. Icon canvas 32 x 32,
horizontally centred (20px sides), 5px top; label occupies bottom 17px.
SVG viewBox 48 square, primary live area 6..42, rounded joins/caps, primary stroke
2.6, secondary 1.6. Sparse math symbols use 12..36 with 3px stroke intentionally.
Group headings align left with their entries; ragged trailing space belongs to
the canvas. Background and hover use CanvasChromePalette. No enclosing panel.

## Semantic brief

| Item | Meaning / silhouette | Distinction / accent |
| --- | --- | --- |
| Voice | oscillator circle with sine | green context |
| FFT / IFFT | wave to spectrum / spectrum to wave | directional arrow |
| Add / Multiply | plain + / × | neutral |
| Mesh | three connected waveform cross sections | surface, not polyhedron |
| Image | image frame / mountains | source |
| Wave | sampled waveform | distinct from oscillator |
| Modulation / Triple | one control / three controls | green / axis colours |
| Envelope | attack, decay, sustain, release | blue |
| Ignore Scratch | crossed scratch envelope | blue, bypass stroke |
| IR | decaying impulse | cyan |
| Waveshaper | saturating S-curve about origin | cyan |
| Unison | three repeated phase-offset waves | cyan, no crossings |
| Reverb | dense decaying reflections | cyan |
| Delay | separated decaying echoes | cyan |
| EQ | frequency response | cyan |

## Deletions and review

Delete pullout geometry/hover corridor, category-only renderer and its resources.
Baseline lines: NodePalette 252/57, chrome presentation 284, hit router 238,
query model 263, automation 959, registry 819. Automation is already oversized:
this change removes duplicated catalog policy; transport/command decomposition
remains outside this slice. Registry owns declarative definitions; wording-only
change adds no responsibilities. Chrome owns drawing, palette owns geometry,
authoring owns edit policy, registry owns semantic node help/labels.

## Completion proof

- Direct hit targets work without hover activation, including second FX row.
- Empty trailing cells do not intercept graph interactions; no Channel entries.
- Real pointer creation/drag and undo verified, icon coverage passes.
- All SVGs parse, build succeeds, three raster review passes including actual UI.
- Architecture audit, touched-file sizes, style and diff checks complete.

## Review and verification

All deletion targets are removed. NodePalette now has 164/41 lines, chrome
presentation 236, icon renderer 96 (was 86), hit router 239, query model 263,
automation 955, registry 819. Production additions are smaller than removals.
No domain algorithms or gesture state machines were copied. Hover no longer
constructs a model just to find its label; descriptions come from the registry.
SVG rendering preserves the authored viewBox instead of stretching the occupied
art bounds. Sparse math glyphs occupy 18px of the 32px canvas intentionally;
other silhouettes retain their full aspect ratios and authored margins.

NodeCanvas grew 2,937 -> 2,944 lines: existing hover routing now observes entry
identity as well as group identity, and fitting delegates palette clearance to
NodePalette. Responsibilities remain event routing, child lifecycle and viewport
composition. Stable collaborators are NodePalette, NodeCanvasHitRouter,
NodeCanvasInteraction, the authoring service and viewport. Palette alone owns
hit geometry; authoring owns edit/undo policy; registry owns semantic help.
The outstanding extraction of canvas hover/overlay coordination remains the
broader architecture-quality deletion target recorded in refactors.md; this
slice adds no node-family policy or independent interaction implementation.

Builds: standalone-debug CycleV2 and tests CycleV2_tests, --parallel 10.
Focused CTest run: 13/13 passed. Palette tests: 295 assertions / 9 cases passed.
The icon test checks occupied raster dimensions at native 32px and can emit a
96px/32px contact sheet with CYCLE_PALETTE_REVIEW_PATH. All 26 node SVGs parse.
The direct-palette fixture proves two drag updates, release, Add creation,
movement/add undo, second-row EQ creation and undo through canvas pointer events.
Architecture audit completed; clang-tidy reports only existing recommendations
to make stateless methods static. Diff whitespace and scalar-math checks pass.

Visual pass A: /private/tmp/sidebar-icons-a.png exposed viewBox scaling and
label encoding; corrected both. Pass B: /private/tmp/sidebar-icons-b.png and
/private/tmp/sidebar-after.png checked the revised surface icon and actual grid.
Pass C: /private/tmp/sidebar-icons-final.png and /private/tmp/sidebar-final.png
checked the family and native hovered Waveshaper tile. /private/tmp/sidebar-before-os.png
is the baseline. Labels fit, graph fitting clears the palette, sparse rows leave
canvas exposed, and the hover border follows the actual tile. No DSP behavior
changed. Reports: sidebar-ctest.log, sidebar-icon-metrics.log,
sidebar-interaction-report.json, sidebar-audit.txt and sidebar-tidy-final.log in
/private/tmp.

## Compact tile refinement (implemented and verified)

User feedback: reduce the large tile margins, show item labels only on hover,
and try restrained calligraphic Fourier F / inverse F symbols while retaining
fft.svg and ifft.svg. Reuse palette hit geometry, cached renderer and existing
creation/undo path. No domain or interaction policy changes.

New geometry: 40 x 40 tiles, 32 x 32 icon canvas centered with 4px insets,
3px tile gaps, 16px headings, 6px group gaps. Rail width falls 224 -> 126px;
height 620 -> 452px. A 24px hover label sits 6px beyond the rail, centered on
the hovered tile; it overlays the canvas without shifting or covering tiles.
FFT alternatives are palette presentation mappings to separate SVG resources;
the existing transform diagrams stay registered. Math glyph live area remains
unchanged. Verify the inverse superscript at 32px, direct hit targets and undo,
plus native resting and hovered screenshots.

The Fourier glyphs are vector outlines of a restrained Chancery F. Forward and
inverse share the same letterform; inverse adds a manually drawn, readable -1
superscript. Native 32px review confirms recognition without ornamental loops.
The original fft.svg/ifft.svg have no diff in this refinement.

Responsibility review: palette remains geometry/catalog (164 lines), palette
entry renderer maps two presentation variants then delegates to the cached
NodeIconRenderer (18 -> 37 lines); chrome owns drawing and hover-label sizing
(236 -> 254), presentation header 164 -> 165. No threshold is crossed, no graph
behavior or creation/undo policy changes, no unrelated kind switching introduced.
Hover labels are painted after the cached rail so they are not clipped to it;
icon tiles remain static and labels never affect hit geometry.

Compact refinement verification: both build targets pass with --parallel 10;
9 palette cases / 295 assertions pass, as does the 18-command direct-palette
creation/drag/undo fixture at the new coordinates. SVG coverage includes the
new pair. Visual reviews: native inverse-hover screenshot
/private/tmp/sidebar-compact-a.png; final forward-hover screenshot
/private/tmp/sidebar-compact-final.png; 96px and 32px family contact sheet
/private/tmp/sidebar-compact-icons.png. The superscript is distinct, hover
labels fit, and unhovered tiles have no labels. Style, scalar-math, diff and
architecture audit checks pass; clang-tidy has only the existing stateless-method
recommendations. No remaining work in this refinement.

## Neutral palette accents

Replace cyan in the eight visible Mesh/Wave/FX icons with the same cool grey
as the Fourier and math glyphs (#d2d9e2). Delay retains decreasing contrast in
grey. Dark canvas blue would lose foreground contrast against the tile surface.
This is SVG colour data only; layout, rendering and interaction are unchanged.
The preserved FFT/IFFT diagram alternatives remain intact. No architecture or
adapter change is required.

Verified neutral accents with the 96px/32px sheet
/private/tmp/sidebar-grey-icons.png and the running Debug app captured by its
specific PID at /private/tmp/sidebar-grey-exact.png. Both targets build and all
9 palette tests / 295 assertions pass. SVG XML and diff checks pass. The first
screenshot helper focused an older Release instance; that capture was rejected.

## Monochrome resting palette (implemented and verified)

All palette entries must be neutral at rest. Reveal the original semantic hues
only on hover, including Mod Triple's axes. NodeIconRenderer remains the SVG
parser/cache owner: cache a desaturated vector variant once beside the semantic
variant, retaining brightness and alpha. Palette renderer selects the variant
from hover state. Shared non-palette consumers keep semantic colour by default.
No SVG geometry duplication, per-pixel paint transforms or runtime reparsing.
Verify every resting icon has equal RGB components and that hover/rest round
trips preserve the cached monochrome variant. Native hover and contact-sheet
reviews must confirm the family remains visually balanced.

Verified: standalone-debug and tests builds, 10 palette cases / 315 assertions,
and 3 focused CTest cases pass. Raster tests check every authored entry for
neutral RGB at rest and Mod Triple for semantic hover colour followed by an
unchanged resting render. Visual passes: 96px/native contact sheet
/private/tmp/sidebar-mono-icons.png, native resting capture sidebar-mono-rest.png,
and native Mod Triple hover capture sidebar-mono-hover.png (all in /private/tmp).
A dedicated preview process was targeted by PID and closed after capture.

Production review: NodeIconRenderer 96 -> 120 lines, header 25 -> 29,
palette renderer 37 -> 38. Colour variant ownership stays in the existing cache;
only palette presentation selects the variant. Architecture and whitespace
checks pass. Clang-tidy's recursion advisory applies to the shallow, trusted SVG
resource tree during one-time cache creation; it is outside the paint/hot path.
No per-sample/per-pixel production math or duplicated interaction logic added.
