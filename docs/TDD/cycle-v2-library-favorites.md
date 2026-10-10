# Cycle V2 library favorites

Status: Implemented.

## Design

`MainWindow` already owns the canonical per-user JUCE `PropertiesFile` used by
`GraphFileHistory`. A `LibraryFavorites` preference owner will use that same
file, without changing preset or pattern documents. Factory preset keys are
based on filenames relative to the factory directory so app relocation keeps
them stable. User preset keys use absolute paths; pattern keys use the pattern
library's stable IDs. A deleted or renamed user preset leaves an inert key and
can be removed when it reappears; this avoids silently transferring a favorite
to unrelated content.

The inline preset and pattern browsers share the row star and filter geometry.
The expanded preset browser uses the same preference owner and a matching
favorites filter. Search, tags, and favorites combine with AND semantics.
Clicking a row star toggles the preference without loading the item; clicking
elsewhere retains existing selection/load behavior. Favoriting is per user and
does not enter graph undo or graph mutation.

## Ownership and complexity

`LibraryFavorites` owns persistence and identity. Browser classes own only
filter state and event routing; `SidebarMediaRow` owns shared star painting and
hit geometry. Filter application scans visible records, matching the current
search/tag filter complexity. Toggle changes one setting and refreshes the
visible browser; no graph, audio, or preset serialization work occurs.

## Completion

- Favorites survive constructing a new preference owner over the saved file.
- Preset and pattern row stars toggle without activating rows.
- Favorites filter composes with existing search and tags in both libraries.
- Expanded browser shows and toggles the same preset favorites.
- Production-size UI review, focused interaction tests, architecture audit,
  style check, app build, and commit.

## Verification and architecture review

The preference persistence and three browser interaction tests pass in the
standalone Debug build on macOS. The app built and the preset/pattern sidebar
screenshots were reviewed at its 1728 by 962 production window size. The
architecture audit passes; the only touched size-triggered class is
`NodeCanvas` (2848 lines), which gains only narrow preference forwarding and
sidebar refresh calls. Its existing responsibilities are canvas orchestration
and workspace input routing. Browser filter policy stays in the browsers,
favorite identity and persistence stay in `LibraryFavorites`, and star geometry
stays in `SidebarMediaRow`. No second eligibility or persistence policy was
introduced in the graph, runtime, or node layers. Further `NodeCanvas`
extraction is outside this slice; its new code is forwarding only.
