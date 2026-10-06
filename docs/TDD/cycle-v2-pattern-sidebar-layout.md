# Cycle V2 pattern sidebar layout

Status: Implemented.

The current 272-pixel sidebar spends two horizontal insets on every list row,
then reserves a second right column for separated tags. The note minimap fills
the card behind a title bubble, truncating the title and wasting space beyond
the list border. The New action is attached to search, while Edit, Rename, and
Delete form a separate row. Favorites occupies a heading-row button rather than
behaving like the tag filter it is.

## Design

Use one 6-pixel content inset for search, action bar, tag cloud, and list.
Group New, Edit, Rename, and Delete in a 28-pixel action bar below a full-width
search field. Put the Favorites star at the start of the shared tag cloud; it
combines with search and selected tags using AND. `SidebarTagCloud` owns that
special chip and its interaction state, so both preset and pattern sidebars
use the same filter behavior.

Keep the 84-pixel media-row rhythm. Pattern cards use a 2-pixel horizontal
inset. A 26-pixel header contains the star and title on the left, with the
tags grouped at the top right. The MIDI minimap starts below the header and
uses the rest of the card. `SidebarMediaRow` owns the card geometry;
`PatternBrowser` supplies the sequence and record labels. Preset row visuals
keep their existing overlay treatment.

## Verification

- `SidebarMediaRow` owns pattern card, star, and preview geometry. The card has
  equal 2-pixel horizontal insets, while the minimap starts below the header.
  `PatternBrowser` lays out all four actions in a single full-width group.
- `SidebarTagCloud` owns the Favorites chip and its toggle state. Both browser
  filters combine it with selected tags and search. Pattern row stars still
  toggle favorites without assigning the pattern.
- A 1728 × 962 application capture at
  `/private/tmp/cycle-v2-pattern-sidebar.png` was inspected, including a
  sidebar crop. It shows the title and tags in one header, minimap below,
  aligned card edges, and only the scrollbar occupying the right strip.
- Focused pattern UI and inline preset browser tests pass. The Cycle V2
  architecture audit and `git diff --check` pass. No touched production file
  crosses an architecture size trigger; `InlinePresetBrowser` remains below
  800 lines and only delegates Favorites state to the shared tag cloud.
