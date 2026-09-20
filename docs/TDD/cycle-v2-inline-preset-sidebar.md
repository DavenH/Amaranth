# Cycle V2 Inline Preset Sidebar

Status: Complete

## Product Contract

The right workspace sidebar has two first-class views: Curves and Presets. The
Presets view reuses the full browser's asynchronous index, thumbnail cache,
preview imagery, search semantics, cyan focus language, and keyboard loading.
It presents those elements as a compact selected preview plus a scan-friendly
vertical list. `Browse Files...` opens the full preset-browser editor.

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
- `NodeWorkspace` supplies preset directories and application callbacks.
  `NodeCanvas` hosts the overlay and owns only its visibility state.
- Graph loading continues through `NodeWorkspace::loadGraphFromFile`, preserving
  audio-plan publication and document-presentation updates.

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

Final relevant sizes: `InlinePresetBrowser.cpp` 583 lines and header 94,
`PresetBrowserComponents.cpp` 400, `NodeCanvas.cpp` 2,678 and header 355,
`NodeWorkspace.cpp` 548, `NodeCanvasPresentation.cpp` 1,490, and
`WorkspaceDock.cpp` 370. No existing file grew by 200 lines, and the new
component remains below the architecture review threshold.

## Verification Evidence

- `[cycle-v2][preset][browser][inline]`: 20 assertions / 1 case.
- `[cycle-v2][preset][browser]`: 64 assertions / 4 cases.
- `[cycle-v2][preset][browser][async]`: 10 assertions / 1 case.
- `[cycle-v2][canvas][guide-dock]`: 77 assertions / 6 cases.
- The focused automation fixture switches Curves -> Presets through real button
  events; all four commands pass. It remains at
  `scripts/fixtures/cycle-v2-agent-inline-preset-sidebar.json`.
- Production screenshot:
  `/private/tmp/cycle-v2-inline-sidebar-polish.png`. It shows immediate
  filename-backed content, visible thumbnails, no minimap beneath Presets, the
  metadata scrim, corrected search alignment, dark-on-cyan filter text, and the
  flush inline rail aligned to the workspace edge.
- Standalone Debug and test targets build with `--parallel 10`.
- `scripts/cycle_v2_architecture_audit.py` reports only the documented existing
  PLAN/REVIEW files. `git diff --check` passes.
- The modified visualization code adds no scalar math in a per-bin, per-sample,
  or per-pixel loop. The two reported `std::abs` calls are existing scalar UI
  decisions outside hot loops.
