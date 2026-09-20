# Cycle V2 Preset Browser and Output Previews

Status: In progress

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

1. Add the optional preset-presentation model/codec and document round-trip
   tests without changing graph revisions or undo semantics.
2. Add default output-address selection, offline grid capture, Time/Spectrum
   conversion, normalized rendering, JPEG encoding, and semantic tests proving
   Reverb/Delay/EQ exclusion and Waveshaper/IR inclusion.
3. Add the implicit default output spy to the workspace rail and single-click
   mode toggle without serializing a graph probe.
4. Add asynchronous preset indexing/filtering and the card/inspector browser,
   including keyboard sequence and geometry tests.
5. Add automation plus a bulk generation script, run it only against clean or
   explicitly selected Cycle V2 preset files, and verify Base64/JPEG round trips.
6. Capture the production UI, compare it with the approved concept, refactor,
   style-check, run the architecture audit, and complete the TDD only when all
   completion criteria and deletion targets are satisfied.

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
