# Cycle V2 Inline Preset Sidebar

Status: Complete

## Product Contract

The right workspace sidebar has two first-class views: Curves and Presets. The
Presets view reuses the full browser's asynchronous index, thumbnail cache,
preview imagery, search semantics, cyan focus language, and keyboard loading.
It presents those elements as a compact selected preview plus a scan-friendly
vertical list. `Browse Files...` opens the full preset-browser editor.

The unified Curves/Presets rail uses 80% of its original expanded width. Preset
rows use a compact 76 px rhythm. Inert overflow glyphs are absent; the selected
hero instead exposes one real destructive action that confirms before moving
the source file to the operating-system Trash.

The selected hero is pinned beneath the filters. Scrolling moves only the
compact result rows; keyboard, pointer, and filter selection update the fixed
hero in place.

The Presets view temporarily occupies the minimap region. The minimap
implementation remains present and is painted again when the view no longer
occludes it; this slice does not delete or relocate minimap behavior.

## Technical Design

- `PresetLibraryIndex` remains authoritative for staged directory enumeration,
  metadata parsing, coalesced filtering, and latest-generation publication.
- `PresetThumbnailCache` remains authoritative for path-and-modification-time
  keyed worker decoding. The compact list requests only clipped, visible rows.
- Shared preset preview/chip/overflow painting is extracted from the full
  browser card implementation and used unchanged by both presentations.
- `InlinePresetBrowser` owns compact geometry, selection, scrolling, search,
  tabs, and Browse/Open callbacks. It does not load documents directly.
- `SelectedPresetCard` is a fixed sibling of the rows viewport and presents the
  authoritative selection owned by `CompactList`; it does not own selection or
  filtering policy.
- `NodeWorkspace` supplies preset directories and application callbacks.
  `NodeCanvas` hosts the overlay and owns only its visibility state.
- Graph loading continues through `NodeWorkspace::loadGraphFromFile`, preserving
  audio-plan publication and document-presentation updates.
- `InlinePresetBrowser` owns delete confirmation and refresh orchestration. The
  filesystem boundary is an injected callback for semantic testing; production
  uses JUCE's recoverable `File::moveToTrash` unchanged.

Directory scanning and JPEG decoding never occur on the message thread. A
search edit remains O(number of presets), independent of graph size and image
bytes. Selecting or loading a preset is O(1) aside from the existing document
load.

## Implementation Slices

1. **Complete.** Extract shared browser painting and enlarge full-browser
   search typography.
2. **Complete.** Add the compact inline browser with tabs, search, selected
   preview, filters, visible-row thumbnails, and Browse/Open actions.
3. **Complete.** Host it in the workspace sidebar, suppress the minimap while
   active, and retain semantic document loading through the workspace.
4. **Complete.** Add focused component/geometry tests, capture the production UI,
   complete architecture/style review, and commit.
5. **Complete.** Centre compact search text optically, use dark text on cyan
   controls, overlay hero metadata on the preview, and remove the asymmetric
   outer bottom radius.
6. **Complete.** Reduce unified rail width by 20%, tighten compact row height
   and gaps, remove inert overflow glyphs, and add confirmed hero-card Trash.
7. **Complete.** Extract the selected hero from the scrolling content and pin it
   above the rows viewport while retaining live selection updates.
8. **Complete.** Reduce the hero contrast scrim from 68 px to 52 px while
   retaining two compact metadata rows and the existing Trash hit target.

## Architecture Baseline

Before implementation:

| File | Lines | Responsibilities |
| --- | ---: | --- |
| `NodeCanvas.cpp` | 2,636 | Canvas lifecycle, routing, and orchestration; PLAN file |
| `NodeCanvas.h` | 347 | Canvas public surface and collaborators; REVIEW file |
| `NodeWorkspace.cpp` | 540 | Canvas/audio/keyboard orchestration |
| `NodeCanvasPresentation.cpp` | 1,484 | Canvas paint orchestration; PLAN file |
| `PresetBrowserComponents.cpp` | 390 | Full-browser card/detail/sidebar presentation |

The feature must not place filesystem parsing, image decoding, or compact-card
painting in `NodeCanvas`. Changes to existing PLAN files are limited to child
component lifecycle and a single presentation eligibility fact. The new inline
component is the deletion target for no temporary bridge; it is the stable
compact presentation over the existing index/cache services.

## Completion Criteria

- Curves and Presets switch within one aligned right-sidebar header.
- Activating Presets immediately shows filename-backed rows, then enriches them
  asynchronously without selection loss.
- Search uses the existing coalescing/latest-result contract; Return and double
  click load the highlighted preset.
- `Browse Files...` opens the full browser editor.
- Only visible compact thumbnails decode, and the message thread never parses
  preview JPEG data.
- The minimap source remains intact and is not painted beneath the active
  Presets view.
- The unified expanded guide/preset rail is 25.6% of the workspace up to 272 px;
  compact rows are 76 px high with 2 px between them.
- Canceling Trash leaves the preset untouched. Confirming invokes exactly one
  recoverable deletion and refreshes the asynchronous index.
- Scrolling the rows does not move the selected hero; changing selection updates
  the pinned hero without resetting the rows viewport.
- Focused tests, standalone build, screenshot review, architecture audit,
  modified-file line counts, scalar-math self-check, and `git diff --check`
  pass.

## Final Architecture Review

The stable ownership matches the design. `InlinePresetBrowser` owns compact
layout, tabs, search, pack filtering, selection, and viewport painting.
`PresetLibraryIndex` is still the only filesystem/metadata/filter policy owner,
now with a second worker so a query cannot queue behind metadata enrichment.
`PresetThumbnailCache` remains the only JPEG decode/cache owner. Shared preview
and overflow painting moved behind `PresetBrowserPainting`, so the full and
inline browser do not duplicate image-state behavior.

`NodeCanvas.cpp` grew from 2,636 to 2,678 lines. Its added responsibility is
limited to child-component lifecycle, bounds, and translating one active-tab
fact into existing paint eligibility. It does not inspect records, filter,
decode, or load presets. `NodeCanvasPresentation.cpp` grew by six lines of
policy application: the minimap and guide renderer consume the same visibility
fact. `NodeWorkspace` remains the owner of document load/audio publication and
only supplies callbacks. There is one tab/minimap eligibility decision site.

Final relevant sizes: `InlinePresetBrowser.cpp` 715 lines and header 107,
`PresetBrowserComponents.cpp` 386, `NodeCanvas.cpp` 2,678 and header 355,
`NodeWorkspace.cpp` 548, `NodeCanvasPresentation.cpp` 1,490, and
`WorkspaceDock.cpp` 370. No existing file grew by 200 lines, and the new
component remains below the architecture review threshold.

Slice 6 grows `InlinePresetBrowser.cpp` by 98 lines to keep the compact action,
its confirmation lifecycle, and index refresh with the presentation that owns
the selection. It remains below the 800-line review trigger. `WorkspaceDock`
continues to be the single owner of unified rail width; callers consume its
layout without a second preset-specific width policy. The injected delete and
confirmation callbacks translate side effects for tests and do not duplicate
filesystem or index behavior.

Slice 7 replaces the mixed scrolling component with two single-purpose
presentations: `SelectedPresetCard` paints the fixed hero and `CompactList`
paints scrollable rows. `CompactList` remains the sole selection owner, and a
single callback updates the hero. This removes scroll-offset coupling rather
than adding a sticky-position compatibility path.

## Verification Evidence

- `[cycle-v2][preset][browser][inline]`: 37 assertions / 1 case, including
  fixed hero bounds after a 100 px row scroll, live keyboard-selection updates,
  and cancel/confirm branches through the real trash-button event.
- `[cycle-v2][preset][browser]`: 81 assertions / 4 cases.
- `[cycle-v2][preset][browser][async]`: 10 assertions / 1 case.
- `[cycle-v2][canvas][guide-dock]`: 77 assertions / 6 cases.
- The focused automation fixture switches Curves -> Presets through real button
  events; all four commands pass. It remains at
  `scripts/fixtures/cycle-v2-agent-inline-preset-sidebar.json`.
- Production screenshot:
  `/private/tmp/cycle-v2-inline-sidebar-short-scrim.png`. It shows immediate
  filename-backed content, visible thumbnails, no minimap beneath Presets, the
  metadata scrim, 20% narrower rail, compact scrolling rows beneath the pinned
  hero, the shortened 52 px contrast region, hero-card Trash action,
  dark-on-cyan filter text, and the flush rail aligned to the workspace edge.
- Standalone Debug and test targets build with `--parallel 10`.
- `scripts/cycle_v2_architecture_audit.py` reports only the documented existing
  PLAN/REVIEW files. `git diff --check` passes.
- `clang-tidy` is not installed in the local environment; both affected build
  targets compile cleanly with the configured toolchain.
- The modified visualization code adds no scalar math in a per-bin, per-sample,
  or per-pixel loop. The two reported `std::abs` calls are existing scalar UI
  decisions outside hot loops.
