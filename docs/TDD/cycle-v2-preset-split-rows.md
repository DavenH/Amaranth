# Preset sidebar split rows

Status: implemented.

Use SidebarMediaRow as the authoritative preset row geometry/rendering owner.
Split the inset content 40/60: left metadata, right preview. Keep selection around
the whole card, preserve favorite and selection callbacks, and reduce preset row
height from 84 to 78. Patterns keep their existing layout. Title gets two lines;
up to two tags wrap below it. The existing preview painter and thumbnail cache
remain unchanged. Favorite hit testing and automation use shared layout geometry.
No new graph, lifecycle or interaction behavior. Fixed work per painted row.

Completion: inspect actual sidebar render, test split containment/no overlap and
existing selection/favorite/scroll interactions; build, style and diff review.

## Verification and review

Standalone-debug and tests builds pass. Seven inline-browser cases pass 144
assertions, including preset opening, filtering, favorites, row height and split
layout at 250/290px widths. The populated sidebar was rendered after index startup
(`/private/tmp/preset-split-rows.png`); metadata and thumbnails are separated,
selection remains around the card and favorites retain their existing hit target.
Two-line titles and two wrapped tag chips have dedicated space in the left column.

Refactor/style review keeps all geometry in SidebarMediaRow, sharing it with hit
and automation routes. Pattern rendering and preview caching remain authoritative
and unchanged. No DSP, hot-loop math or interaction implementation introduced.
Architecture audit and whitespace checks pass. SidebarMediaRow.cpp is 206 lines,
header 62; InlinePresetBrowser stays 724 and changes only preset row-height uses.
All completion criteria above are satisfied.

Tag readability follow-up: preset chip text is 10.5px instead of 8.5px, with 15px
chips and room for two wrapped lines. Preserve authored tag capitalization in
both sidebar row renderers instead of forcing uppercase. Measurement and painting
use the same text/font. Both builds, seven browser cases / 144 assertions, visual
review, architecture audit and whitespace/style checks pass. SidebarMediaRow is
207 lines; no new ownership or interaction behavior.

Metadata alignment follow-up: right-align the two-line title against the image
divider and give it the full metadata-column width. Move the 22px favorite button
to the bottom-left; tags now have matching 22px height, bottom-align beside it,
and align as a group to the divider. Long tag pairs wrap in the same right-hand
column. Preset rows are 90px to accommodate two tag rows below the title without
overlap. Shared favorite hit/automation geometry follows the new position.
Both builds and seven browser cases / 156 assertions pass, including title/tag/
favorite containment and separation. Architecture audit and diff/style checks
pass. SidebarMediaRow is 212 lines; no new behavior or policy boundary.

Compact-row follow-up: reduce preset height from 90 to 68px (24%); preserve the
40/60 split and title typography. Favorite and tags share one 22px line, 4px
below the title region. Tag pairs share the available width instead of wrapping
into the title; long labels use the existing fitted-text truncation. Pattern rows
remain unchanged. Shared geometry still owns drawing, favorite hits and automation.
Native capture `/private/tmp/compact-preset-rows.png` reviewed with one/two tags,
selected rows and favorites. Geometry assertions cover the common control baseline
and title-to-control gap at 250/290px widths. Architecture and style/diff review:
SidebarMediaRow is 217/62 lines, with unchanged collaborators/ownership and fixed
per-row work. No extraction or deletion targets. Standalone build passes.
The browser integration test now checks filter narrowing rather than exact counts
in the actively edited user preset library; temporary-file tag editing coverage
continues to verify exact filter outcomes.
Tests build passes; all nine inline cases pass (209 assertions).

Title/favorite follow-up: use Helvetica Neue Medium at 11.5px for macOS preset
titles (default sans serif elsewhere). Move the shared 22px favorite hit/draw
bounds to the metadata top-left; reserve a 4px gap before the title. The bottom
22px tag line uses the entire metadata width and admits up to four chips, requiring
26px minimum available space per additional chip. Existing fitted text handles
long final labels. Pattern rows retain their two-chip limit and typography.
Native screenshot `/private/tmp/title-preset-rows.png` reviewed: three-tag rows,
single-tag rows, selected and favorite states. Both builds pass; nine inline
browser tests pass 215 assertions, including title/favorite separation and three
chips at 250/290px widths. Refactor/style/diff and architecture review pass:
SidebarMediaRow 220/62 lines; same geometry/rendering ownership and collaborators,
no domain behavior, new adapters or outstanding deletion targets.

Typography refinement: increase the preset title from 11.5 to 12.5px and reserve
28px for its two-line region. Preset chips shrink from 22 to 18px; sidebar cloud
chips shrink from 21 to 18px. Sidebar tags render lowercase, with measurement using
the same lowercase text; stored tag values and matching remain unchanged. The
favorite stays 22px and the 68px row height is retained. Geometry tests confirm
smaller chips, bottom alignment and a 6px title-region gap. Both builds and nine
inline tests pass (215 assertions). Architecture/style/diff review passes;
SidebarMediaRow/SidebarTagCloud stay 220/225 lines with unchanged responsibilities.
Native capture `/private/tmp/lowercase-preset-tags.png` reviewed for title clarity,
shorter lowercase chips, three-tag rows and cloud selection state.

Row-chip contrast refinement: replace the light grey chip fill with the shared
inset background (#11171d), darker than both resting (#171d24) and selected
(#202833) rows. Preserve text and geometry. Standalone build and diff check pass.
Native capture `/private/tmp/dark-row-tags.png` confirms subdued chip backgrounds
on selected and resting rows with legible labels.

Favorite refinement: remove the rounded background from the shared sidebar star
painter. Preserve outlined/filled states, colour, position and 22px hit area.
Standalone build and whitespace review pass; no interaction logic changed.
Visual review `/private/tmp/bare-favorites.png` confirms background-free stars in
selected, resting and favorite rows.

Full-width revision: supersede the 40/60 split with a full-width spectrogram below
a 22px metadata header and 4px gap. Increase rows from 68 to 82px (21%). Keep
Helvetica Neue Medium 12.5px, lowercase 18px dark chips and the background-free
22px favorite. Title is left-aligned; tags occupy the right side before the star
at the far right. Reserve at least 94px for the title, admitting tags within the
remaining header budget. Favorite drawing/hit/automation still share presetLayout.
Patterns retain their existing geometry. Review confirms the shared row painter
owns geometry; no new lifecycle, graph or persistence behavior. Architecture
review: SidebarMediaRow 217/62 lines, no new dependencies or extraction targets.
Both builds and nine inline tests pass (215 assertions), covering full-width
geometry, shared header baselines, separation and favorite interaction. Native
capture `/private/tmp/full-width-presets.png` reviewed with selected rows,
favorites and multiple tags. Diff/style checks pass; no remaining work.
