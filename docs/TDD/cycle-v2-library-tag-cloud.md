# Cycle V2 library tag clouds

Status: Complete (2026-10-04).

## Design

The existing `PresetPresentationCodec` owns preset tag serialization and
`PatternLibrary` owns pattern file serialization. Preset files will receive
curated metadata tags without changing graph, preview, or pattern IDs. Pattern
files retain their legacy primary `tag` while an optional `tags` array carries
additional searchable labels. The library reader supplies the primary tag for
older files. Pattern editing keeps the tags attached to the stable pattern ID.

Both sidebar browsers use one `SidebarTagCloud` component. It owns chip layout,
hit testing, toggled state, and the OR match rule across selected tags. Search
and selected tags combine with AND. Available chips come from the full library,
so selecting one does not make other chips disappear. Selection and scroll
position remain stable when a filter changes. Pattern creation accepts tags in
its name prompt; filtering does not silently assign a tag to a new pattern.

`SidebarMediaRow` remains the shared card painter. The MIDI minimap uses its
whole preview rectangle behind the title and tags, with the same dark rounded
label backgrounds as the preset spectrogram. The authoritative note projection
remains `MidiPatternMiniMap`; the row only changes its destination rectangle.

## Complexity and boundaries

Chip toggling costs O(number of records times tags per record). Row painting
costs only the visible MIDI notes or thumbnail pixels, as before. There is no
graph mutation or audio-thread work. The sidebar component owns filtering;
the index and pattern library remain the source of records and metadata.
`SidebarTypeFilter` and its ComboBox decision path are deleted after the shared
chip cloud is integrated.

## Completion criteria

- Factory presets have reviewed family and character tags in saved metadata.
- Patterns retain meaningful tags when read and edited; older single-tag files
  remain readable.
- Both tabs use the same toggled chip cloud and filtering semantics, including
  search combined with tags and stable scroll selection.
- Pattern note previews fill the row behind legible title and tag chips.
- Focused semantic tests and production-size screenshots cover both browsers.
- Architecture audit, style review, proportionate build/tests, and a coherent
  commit pass.

## Verification and review

All 246 shipped preset files have tags and remain valid JSON. The reviewed
taxonomy has 13 primary families and nine character tags, for 22 filter chips
in the preset sidebar. The tagging script changes only the metadata tag line
and is idempotent. Pattern files have primary and secondary tags; the six basic
patterns remain the curated templates, while eight additional packaged
patterns are referenced by presets. The focused factory test checks this
relationship instead of counting all packaged pattern assets as templates.

The standalone-debug app and test targets build. The focused browser and
pattern tests pass (1,045 assertions in 14 cases), including chip toggling,
inclusive multi-selection, search combinations, pattern tag round-tripping,
and stable list scrolling. The sidebar fixture passes all ten commands,
including chip hover and selection in both tabs. Production-size screenshots
show the notes filling each pattern row behind the title and tag chips, and
confirm readable selected and unselected filter chips.

`InlinePresetBrowser.cpp` changes from 600 to 614 lines, `PatternBrowser.cpp`
from 217 to 253, `PatternLibrary.cpp` from 155 to 180, and `NodeWorkspace.cpp`
from 723 to 725. `SidebarTagCloud` owns the only chip layout and selection
decision; the browsers supply record tags and search results. `PatternLibrary`
alone reads and saves pattern tags, while `PresetPresentationCodec` remains the
source of preset metadata. No graph command or audio path changes. The old
`SidebarTypeFilter` is deleted. The architecture audit reports no new size
trigger; `git diff --check` passes.
