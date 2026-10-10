# Cycle V2 Preset Browser and Output Previews

Status: Complete

## Product Contract

Cycle V2 needs a keyboard-first preset browser whose primary content is a
scannable grid of embedded output previews. Typing filters the indexed library
with coalesced asynchronous publications, selection survives compatible filter
updates, and Return loads the selected preset. The selected card uses geometry
and surface contrast as well as colour, and the right inspector exposes the
same preview at a larger size.

Every preset may carry one compact JPEG preview in its document envelope. The
preview depicts an implicit default output spy and is not part of `NodeGraph`,
its revision, undo, or realtime state. The implicit spy has two presentation
modes, Time and Spectrum, toggled by a single click. Its signal is the global
chain after meaningful dry effects such as Waveshaper and IR Modeller but before
Reverb, Delay, and EQ. Those excluded effects must not affect the preview.

Preview contrast is normalized only at the image-presentation boundary. It must
not alter captured samples or the ordinary signal-spy renderer.

## Authoritative Implementations and Boundaries

- `GraphCompiler` and `GraphAudioExecutor` remain authoritative for graph
  traversal and DSP. Preview generation must execute a compiled graph and must
  not reproduce effect algorithms.
- `GraphPreviewExecutor` and `NodePreviewRenderer` remain authoritative for
  traversal-grid capture and grid rendering. The preset-preview path adapts one
  derived output address and image size; it does not add another rasterizer.
- `GraphSerializer` remains authoritative for the Cycle V2 JSON envelope.
  A narrow preset-presentation codec owns optional metadata and the Base64 JPEG;
  `NodeGraph` stays unaware of it.
- `GraphDocument` owns loaded/saved preset presentation metadata alongside the
  graph. Graph edits do not copy that metadata into undo entries.
- `PresetBrowserPage` owns browser interaction and presentation. A separate
  asynchronous index owns filesystem reads, JSON metadata extraction, filtering,
  cancellation generations, and message-thread publication.
- Existing app automation remains the authority for opening and saving preset
  files. A narrow preview-generation command reuses the same document/runtime
  services; a script orchestrates the library-wide loop.

## Default Output Address

Global audio graphs are linear at the output boundary. Starting at the Output
input, walk upstream through the single signal predecessor. Skip a trailing run
of Reverb, Delay, and Equalizer nodes and select the first upstream signal
output. This preserves Waveshaper and Impulse Response processing while
excluding the declared wet/boundary effects. If there is no excluded suffix,
the Output input source is selected. Ambiguous or disconnected output chains
produce no preview rather than an approximation.

This selection is O(nodes + edges) once per offline capture. It is not run on
the audio thread or during a live gesture.

## Persistence

The optional root member is:

```json
"presetPresentation": {
    "version": 1,
    "preview": {
        "mediaType": "image/jpeg",
        "width": 320,
        "height": 180,
        "view": "spectrum",
        "data": "...Base64..."
    }
}
```

Readers validate type, dimensions, decoded byte limit, JPEG decoding, and
decoded dimensions. Missing or malformed optional metadata never makes an
otherwise valid graph unloadable; it is ignored and reported to the browser as
unavailable. Saving preserves valid loaded metadata. Regeneration replaces only
the preview member.

## Browser Geometry and States

At 1180 x 760 logical pixels:

- 64 px title/search band; the search field receives initial focus.
- 44 px filter/status band.
- 172 px navigation rail, flexible two/three-column result grid, and 268 px
  detail inspector.
- Cards use a 16:9 image, 8 px internal rhythm, and at least a 44 px selectable
  text/action region.
- The selected card has a 2 px outline and distinct raised surface. Focus is
  additionally visible when keyboard-owned.
- Bottom action band is 52 px. Return loads, Escape closes, arrows move through
  the visual grid, and double-click loads.
- Narrow layouts collapse the inspector before reducing cards below their
  legible minimum width.

Empty, loading, missing-preview, focused-search, selected, hover, and load-error
states require explicit presentation. A preset without an embedded preview uses
a quiet deterministic placeholder, never a fabricated visualization.

## Filtering and Complexity

The index scans files and parses metadata off the message thread. Text changes
increment a generation and restart a short one-shot coalescing timer. Filtering
runs on the worker over immutable indexed records, and only the newest generation
publishes. One text edit is O(number of indexed presets); it does not decode JPEG
bytes, read files again, or block painting. Image decoding is lazy and cached by
file path plus modification time.

### First-Paint Performance Follow-up

The browser must publish filename-backed cards after directory enumeration,
before parsing presentation metadata. Full JSON/Base64 validation remains on a
worker. JPEG decoding belongs to a separate shared thumbnail cache and is
requested only for cards intersecting the viewport (plus the selected detail
preview). Painting, filtering, and selection must never decode an image.

The stable design does not require SQLite: the preset document remains the
authoritative source, the lightweight first publication needs only filenames,
and path-plus-modification-time cache keys provide exact invalidation. A
persistent database would add schema, rebuild, and stale-cache policy without
improving the required first card publication.

The approved visual hierarchy keeps previews dominant, uses Cycle V2 chrome
colours/radii for search and action controls, reserves cyan fill for the primary
Load action, and uses geometry plus a 2 px outline for card selection. The
implicit output spy follows authored spies in signal-flow order and is labelled
only `out`; its Time/Spectrum mode remains evident from the rendered content.

## Architecture Review Baseline

Before implementation:

| File | Lines | Responsibility note |
| --- | ---: | --- |
| `PresetBrowserPage.cpp` | 182 | Current list UI and synchronous filtering |
| `GraphSerializer.cpp` | 1,217 | Full graph JSON codec; already a PLAN trigger |
| `GraphDocument.cpp` | 165 | Graph lifetime, history, and file persistence |
| `PresentationPreviewRenderer.cpp` | 191 | Existing offline preview/probe capture |
| `SignalProbeRail.cpp` | 527 | Spy dock ordering, interaction, and rendering |
| `CycleV2Automation.cpp` | 2,030 | Command routing; already a PLAN trigger |

The implementation must not grow `GraphSerializer.cpp` or
`CycleV2Automation.cpp` with preview-domain mechanics. New codecs, capture
selection/rendering, browser indexing, and card presentation live in focused
files. Existing large files receive only delegation and routing calls.

Stable dependency direction:

`PresetBrowser UI -> PresetLibraryIndex -> PresetPresentation codec`

`Automation / save workflow -> PresetPreviewGenerator -> graph compiler and
runtime -> NodePreviewRenderer -> PresetPresentation codec`

`GraphDocument -> PresetPresentation codec` only for envelope preservation.

## Implementation Slices

1. **Complete.** Add the optional preset-presentation model/codec and document
   round-trip tests without placing presentation data in graph revisions or
   undo snapshots. The focused persistence suite passes 15 assertions across
   valid round-trip, malformed optional data, and document-history cases.
2. **Complete.** Added default output-address selection, offline grid capture,
   Time/Spectrum conversion, normalized rendering, JPEG encoding, and semantic
   tests proving Reverb/Delay/EQ exclusion and Waveshaper/IR inclusion.
3. **Complete.** Added the implicit default output spy to the workspace rail
   and single-click mode toggle without serializing a graph probe.
4. **Complete.** Added asynchronous preset indexing/filtering and the
   card/inspector browser, including the rapid typing, arrow-selection, and
   Return-load component sequence.
5. **Complete.** Added automation plus a bulk generation script. The production
   run embedded 244 valid 320 x 180 spectral JPEGs while initially skipping four
   dirty presets. After explicit approval, a targeted follow-up generated those
   four previews without rewriting their existing graph edits. The only skipped
   file is `empty.cyclegraph`, because it has no renderable source. Every preview
   update is confined to root presentation metadata; graph bodies are unchanged.
6. **Complete.** Captured the production browser with real embedded previews at
   `/private/tmp/cycle-v2-preset-browser-previews-final.png`, completed the
   refactor/style pass, and ran the architecture audit.
7. **Complete.** Publish skeleton cards before metadata parsing, decode only
   visible thumbnails on a worker, converge native widgets on the approved
   Cycle V2 treatment, and move the terse output spy to the end of the rail.

## Final Architecture Review

The production diff adds no new domain switchboard and no C++ file grows by
200 lines. New responsibilities remain in focused collaborators:

- `DefaultOutputProbeResolver` owns the one output-boundary policy;
  `GraphCompiler` only translates its address into compiled step/output indices.
- `DefaultOutputPreview` owns Time/Spectrum conversion and normalized contrast;
  `GraphPreviewExecutor` only captures the authoritative runtime grid.
- `PresetPreviewGenerator` owns the fixed raster/JPEG product;
  `NodeCanvas` only coordinates current presentation state and document save.
- `PresetLibraryIndex` owns two-stage filesystem/metadata publication and
  lightweight search strings. `PresetThumbnailCache` owns path-and-mtime keyed
  worker decoding, and the card grid requests only visible images.
- Browser components own layout and interaction;
  `PresetBrowserLookAndFeel` owns widget chrome without duplicating indexing or
  image-loading policy.
- `CycleV2Automation` remains command routing/transport. The bulk script asks it
  for JPEG bytes with `embed: false` and surgically replaces only the root
  presentation member, avoiding a graph reserialization.

The pre-existing PLAN files remain cohesive for this slice: `GraphCompiler.cpp`
still owns compiled-plan construction, `NodeCanvas.cpp` still owns canvas-level
document/presentation orchestration, `NodeCanvasPresentation.cpp` still owns
canvas painting, and `CycleV2Automation.cpp` still owns automation routing.
No lifecycle, eligibility, normalization, or output-boundary policy is duplicated
between those callers.

Final relevant sizes: `PresetBrowserPage.cpp` 236 lines,
`PresetBrowserComponents.cpp` 390, `PresetLibraryIndex.cpp` 242,
`PresetThumbnailCache.cpp` 119, `PresetBrowserLookAndFeel.cpp` 96,
`SignalProbeRail.cpp` 565, `PresetPresentation.cpp` 172,
`DefaultOutputPreview.cpp` 75, and `PresetPreviewGenerator.cpp` 80. The old
synchronous `ListBox` implementation and filename-filter loop are deleted.

## Verification Evidence

- `[cycle-v2][preset][preview]`: 39 assertions / 7 cases, including direct
  output plus individual EQ, Reverb, and Delay boundaries.
- `[cycle-v2][preset][browser]`: 44 assertions / 3 cases.
- `[cycle-v2][preset][presentation]`: 19 assertions / 4 cases.
- `[cycle-v2][preset][browser][async]`: 10 assertions / 1 case.
- `[cycle-v2][preset][browser][thumbnail]`: 10 assertions / 1 case.
- `[cycle-v2][ui][probe]`: 89 assertions / 10 cases.
- Standalone Debug and test targets build successfully with `--parallel 10`.
- `scripts/cycle_v2_architecture_audit.py` completed; all reported PLAN/REVIEW
  files pre-date this work and the rationale for the touched ones is above.
- Modified visualization files contain no scalar `std::<math>` calls in hot
  loops; `git diff --check` passes.
- A production capture taken about 80 ms after opening already shows the full
  card grid and most visible thumbnails at
  `/private/tmp/cycle-v2-preset-browser-review-first.png`; the settled capture
  is `/private/tmp/cycle-v2-preset-browser-caret-final.png`. The latter also
  verifies the focused search caret is vertically centred.
- A broader existing `[cycle-v2][preset]` selector still contains seven
  unrelated legacy parity failures, recorded in `docs/TDD/audio-bugs.md` with
  log `/private/tmp/cycle-v2-preset-broad-tests.log`.

## Tests and Completion Criteria

- Metadata survives load, graph edit, undo/redo, and save; it does not enter
  `NodeGraph` copies.
- Invalid or oversized preview data is safely ignored.
- Output-address tests cover no FX, included FX, each excluded FX, a mixed
  included/excluded suffix, and an ambiguous/disconnected output.
- Captured time and spectral grids differ, are finite, have fixed dimensions,
  and toggle through the real rail click path.
- Waveshaper and IR parameter changes alter preview content; Reverb, Delay, and
  EQ parameter changes do not.
- Rapid search edits publish only the newest result set. The full sequence
  typing -> coalesced result -> arrow selection -> Return loads the highlighted
  file passes through the real component path.
- Browser screenshot at production size shows aligned bands, scan-legible 16:9
  previews, balanced occupied bounds, visible focus, and no stranded region.
- The bulk tool can generate one selected preset and a library dry run before
  any broad write. Existing unrelated dirty preset files are never overwritten.
- `git diff --check`, focused Catch2 tests, standalone build, architecture audit,
  modified-file line counts, diff stat, and scalar-math hot-loop self-check pass.

## Deletion Targets and Stable End State

- Delete the synchronous filename-only filtering loop from
  `PresetBrowserPage` after the asynchronous index owns filtering.
- Delete the plain `ListBox` row presentation after cards and inspector own
  result display.
- Do not retain a temporary graph probe, duplicate heatmap renderer, sidecar
  image directory, or migration-only metadata bridge.
- The stable state is one optional embedded preview per preset, one implicit
  default output-spy product, and one browser/index implementation.
