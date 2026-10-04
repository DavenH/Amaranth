# Cycle V2 sidebar browser alignment

Status: Complete (2026-10-04).

## Design

The Presets and Patterns tabs share a two-row toolbar geometry: a search field
and New action, then a type filter and secondary action. Their list viewports
start at the same vertical position. `LibrarySearchField` owns the same icon,
height, font scale, outline, corner radius, and empty-search playback key
behavior in both tabs and the full preset browser. Pattern New opens a name
prompt. List counts and the selected-preset hero card are removed.

`InlinePresetBrowser` owns preset list interaction, query, filtering, and
sidebar layout. `PatternBrowser` owns pattern list interaction and layout.
`PatternLibrary` remains the source of pattern type tags. Preset type filtering
uses explicit preset tags first, then the type of the referenced pattern; an
unclassified preset is Other. Type options are one shared UI vocabulary.
The preset index and thumbnail renderer remain authoritative. No graph or
audio model behavior is duplicated.

The preset row's single click invokes the existing MainWindow graph replacement
request, which owns the save/discard/cancel prompt. The New action invokes the
existing Save As flow with a new-file suggestion, then refreshes the preset
index. Delete remains available as the second-row action, with its existing
confirmation path. The list selection and viewport are not automatically
scrolled to an item on selection.

Per row selection is O(1) after a hit lookup; query and type changes filter
the current result set once. Type lookup uses an ID map built when the pattern
library publishes records. No graph copy, serialization, or audio preparation
occurs during list interaction.

## Completion criteria

- Preset and pattern toolbars align at production sidebar size, with compact
  search fields and no count labels or top preset hero.
- Presets filter by meaningful type; New saves the current sound as a new
  preset; Delete and Browse remain available.
- A single preset row click opens through the existing replacement prompt.
- Focused semantic tests, agent fixture, screenshot inspection, architecture
  audit, style review, and commit pass.

## Verification and architecture review

The `CycleV2` and `CycleV2_tests` targets build with the standalone-debug
preset. The focused preset and pattern tests pass (748 assertions in 13 cases).
The sidebar agent fixture passes all nine commands, including a single-click
preset load, and its production-size captures show matching search icons,
toolbar geometry, and row alignment. The preset preview now fills the card
behind the title and type chips; the pattern preview remains below its header.

`LibrarySearchField` owns the search icon and Space behavior for both sidebar
tabs and the full preset browser. A whitespace-only field is treated as empty
for playback. The icon is painted by the field itself; the preset look and
feel no longer has a search-specific icon branch. The pattern name entry is
now a search field, and New opens a name prompt.

`InlinePresetBrowser.cpp` is 600 lines after deleting the hero card and scope
button logic; `PatternBrowser.cpp` is 217 lines. `NodeCanvas.cpp` remains at
2,831 lines and only relays browser callbacks and row targets. Its existing
sidebar-host extraction plan in `cycle-v2-pattern-library.md` remains the
deletion target. `NodeWorkspace` coordinates callbacks; MainWindow retains
file-save and graph-replacement policy. The architecture audit reports no new
size trigger. `git diff --check` passes.
