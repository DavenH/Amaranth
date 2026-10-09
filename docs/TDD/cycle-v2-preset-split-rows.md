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
