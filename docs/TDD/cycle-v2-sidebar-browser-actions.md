# Cycle V2 sidebar browser actions

Status: Implemented.

The Patterns tab has a full-width search field and grouped New, Edit, Rename,
and Delete controls. The Presets tab still puts New beside search and Delete
beside the filter heading. These sibling browsers must share one control
implementation and geometry.

## Design

`SidebarLibraryToolbar` owns the search field and four action buttons. It owns
their sizing, colors, group background, and pointer target bounds. The two
browser components provide callbacks and enablement; the toolbar does not
decide which library records can be edited or how an edit is committed. The
shared component replaces each browser's separate search and action layout.

Preset Edit opens the existing tag editor behavior through `PresetMetadataStore`.
Preset Rename changes a display title stored in `presetPresentation`, leaving
the preset file path stable for favorites, references, and the current document.
`PresetPresentationCodec` is authoritative for the title field;
`PresetLibraryIndex` reads the title for display and search. The file metadata
write uses the same atomic read/write boundary as tag edits. When the edited
file is open, the command dispatcher synchronizes the in-memory title so a
later graph save retains it. UI components never mutate `NodeGraph`.

The title edit is one metadata field update, with no audio preparation or
graph copy. Both browser lists keep their current selection and scroll during
search and tag filtering.

## Completion

- The same toolbar renders in Presets and Patterns at the same x positions,
  height, spacing, and style. The preset tab now has Edit and Rename alongside
  New and Delete. Pattern actions retain their existing behavior.
- Editing preset tags and title updates the browser without cloning presets or
  changing their file identity. `PresetMetadataEditor` owns both dialogs;
  `PresetMetadataStore` owns the atomic file update. The title survives graph
  serialization and subsequent saves of an open document.
- Focused tests cover exact cross-tab action bounds, the button sequence,
  title search, metadata preservation, and dispatcher synchronization. The
  title command leaves graph revision and undo unchanged.
- Production-size captures at `/private/tmp/cycle-v2-preset-toolbar.png` and
  `/private/tmp/cycle-v2-pattern-toolbar.png` were inspected side by side.
  The app fixture is `scripts/fixtures/cycle-v2-agent-library-toolbar.json`.
- `standalone-debug` builds the app and tests. Focused browser, pattern,
  title, and tag suites pass; `ctest -R` for the three new interaction/title
  cases reports 3/3 passed. The architecture audit and `git diff --check`
  pass. `clang-tidy` is unavailable in this environment.

## Architecture review

`SidebarLibraryToolbar` is the sole owner of library search/action layout and
styling. The browsers own record eligibility and callbacks, and the index owns
the `metadataReady` fact. The metadata store owns JSON validation and atomic
write. The codec owns serialization. `NodeCanvas` translates the sidebar's
file-specific callback to a dispatcher title/tag update only when that file is
open; it does not contain metadata or UI algorithms.

Baseline → after: `PatternBrowser.cpp` 412 → 386 lines,
`InlinePresetBrowser.cpp` 659 → 707, `PresetBrowserPage.cpp` 338 → 318,
`NodeCanvas.cpp` 2851 → 2862. The existing oversized `NodeCanvas` receives
only callback wiring. Its stable collaborators are the sidebar, document, and
command dispatcher; this change leaves all metadata policy below that
orchestration boundary. The removed decision sites are both browsers' local
button/search layout and styling. The popup's separate tag dialog is also
deleted in favor of `PresetMetadataEditor`. There is one metadata file write
per edit and no graph copy, audio preparation, or graph revision change for a
title edit.
