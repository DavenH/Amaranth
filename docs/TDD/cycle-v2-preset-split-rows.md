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
