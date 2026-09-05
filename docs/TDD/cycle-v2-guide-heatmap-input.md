# Cycle V2 Guide Heatmap Input

## Status

Implemented (2026-09-05).

Depends on `cycle-v2-guide-resource-dock.md` and
`cycle-v2-trimesh-guide-curve-parity.md`.

## Product Contract

An optional embedded image turns a Guide Curve into a live two-dimensional
sampling path. Guide progress maps uniformly across image X, the existing
authored curve supplies image Y, and perceptual luminance multiplied by alpha
becomes the Guide value. The full image stretches over the normalized Guide
domain. Clearing the image restores the existing curve-as-signal behavior.

The expanded editor shows the scalar heatmap, editable path, and a distinct
non-interactive output trace. Shelf previews use the same native Guide panel
and prepared data. PNG and JPEG inputs are embedded in the graph; external file
paths, drag-and-drop, channel selection, aspect controls, and automatic ridge
following are deferred.

The displayed image uses the exact scalar intensity plane. Its texture draw
explicitly uses an opaque white colour because the fixed-function OpenGL path
otherwise modulates texture pixels by stale panel colour state. Native previews
prepare 512 output points and render the derived trace as a 6 px black
under-stroke followed by a 3 px cyan stroke, keeping it legible over both bright
and dark image regions.

## Authoritative Implementations And Boundary

- `FlatCurveModel`, `FlatCurvePreparation`, and the mature curve panel continue
  to own curve points, curve evaluation, and interaction.
- `GuideCurveTableDsp` continues to own bipolar table lookup, noise, DC offset,
  phase, and downsampling.
- `GuideCurveSnapshotProvider`, `TrimeshGuidePreparation`, and the mature Guide
  policies remain the only downstream provider/deformation path.
- The new Guide-domain heatmap preparation owns only image decoding, scalar
  conversion, coordinate translation, and bicubic sampling. It must not copy
  curve evaluation, Guide parameter processing, or Trimesh behavior.
- The scaffolded Image Source node is unrelated graph-signal behavior and is
  not reused as Guide resource ownership.

The stable flow is:

```text
embedded GuideHeatmapAsset + FlatCurveModel
  -> authoritative unipolar curve/path table
  -> clamped Catmull-Rom sample of luminance * alpha
  -> existing bipolar Guide table and GuideCurveTableDsp parameters
  -> existing provider, Trimesh preparation, DSP, and panel consumers
```

## Document And History Design

`NodeGraph` owns immutable, SHA-256-addressed Guide heatmap assets. A Guide
stores an optional asset ID and a resource revision. Identical encoded images
are deduplicated; duplicated Guides share the immutable asset until one is
replaced. Removing the last reference removes the asset in the same semantic
command.

Graph format 4 adds a required `guideHeatmaps` array containing the asset ID,
original filename, detected media type, dimensions, and Base64 encoded bytes.
All shipped Cycle V2 graphs are rewritten directly because Cycle V2 remains
undeployed. There is no legacy reader.

Graph undo/redo stores `NodeGraph` value snapshots instead of serialized JSON.
Immutable model and image payloads therefore remain shared across history;
portable JSON remains confined to file and explicit snapshot boundaries.

All UI mutations pass through `GraphCommandDispatcher`. A Guide gesture
captures the durable resource revision once; every transient publication keeps
that base while the dispatcher advances transient resource and model revisions.
Image load, replace, clear, orphan cleanup, consumer invalidation, and undo are
one consolidated semantic edit.

## Sampling Contract

- Convert normalized sRGB bytes with Rec.709 coefficients and multiply by
  normalized alpha; store the immutable scalar plane as 8-bit values.
- Map Guide progress `u` to the complete image width and authored curve output
  `v` to image height, reversing the image row so Guide Y increases upward.
- Use separable Catmull-Rom bicubic interpolation. Clamp all taps at image
  borders and clamp the interpolated result to `[0, 1]`.
- Prepare 8192 samples outside the audio callback, subtract `0.5`, then apply
  the unchanged Guide table processing.
- Reject inputs larger than 16 MiB encoded, 4096 pixels on either side, or
  8,388,608 decoded pixels. Failed or stale asynchronous loads leave the
  previous Guide unchanged and report an editor status.

## Completion Criteria

- Image-backed Guides affect every existing attached Trimesh consumer without
  a second deformation or table path.
- Path edits update the derived trace and downstream output live; two updates,
  commit, visible/effect refresh, and undo are covered in one sequence test.
- Load, replace, clear, duplication, deletion, serialization, deduplication,
  stale completion, undo, and redo have semantic coverage.
- Expanded and shelf views render the heatmap/path/output relationship through
  the native panel, with a focused automation fixture and OS capture.
- Existing non-image Guide table tests remain unchanged and pass bit-for-bit.
- Production diff/refactor review, style review, `git diff --check`, the
  scalar-math hot-loop audit, applicable clang-tidy, focused tests, full Cycle
  V2 tests, and the standalone build complete before this status is Implemented.

## UI Integration Contract

- Preserve the expanded editor's established 336 px property rail and shared
  precision-slider rows; image support must not restore narrow tracks or
  slider-owned value boxes.
- Place the Guide enablement power toggle in the shared top-right header action
  position. Do not retain a labeled checkbox row in the property rail.
- Present image resource actions as a named `Guide image` group with one compact
  24 px action row. Use `Load` when empty, `Replace` when bound, and `Clear` only
  when an image exists.
- Do not reserve panel space for persistent state narration such as `No image
  loaded`. Loading progress, stale completions, and decode errors belong in the
  editor's existing transient status surface.
- Shelf previews retain the shared canvas colour and corner-radius tokens while
  resolving the image asset through the graph.

## Implementation Evidence

- `GuideHeatmapAsset`, `GuideHeatmapSampler`, and `GuideCurvePreparation`
  provide immutable validated PNG/JPEG data, luminance-alpha conversion,
  clamped Catmull-Rom sampling, and one shared curve/heatmap preparation path.
- Graph format 4 embeds content-addressed assets. Graph commands cover load,
  replace, clear, stale completion, deduplication, orphan cleanup, and snapshot
  history without repeated payload serialization.
- The snapshot provider feeds the existing Guide table DSP and Trimesh
  consumers. The native Guide panel adds an optional image texture and sampled
  output trace while retaining the authoritative flat-curve interaction.
- The expanded editor loads on a worker thread, applies results through
  `GraphCommandDispatcher`, exposes compact `Load`/`Replace` and `Clear`
  actions, and routes transient messages through the existing editor status
  surface. The Guide power action occupies the shared top-right header slot.
  Shelf previews, undo/redo, graph reload, and editor rebinding consume the
  same graph asset.
- The merge retains the branch's IR/audio-resource behavior alongside the
  image-backed Guide behavior. Content identity delegates SHA-256 calculation
  to JUCE's cryptography module rather than maintaining a local implementation.
- Focused post-merge verification passed: 65 Guide editor assertions in 3
  cases, 66 heatmap assertions in 5 cases, 8 serialization assertions in 2
  cases, and 41 audio-resource assertions in 3 cases.
- `CycleV2` and `CycleV2_tests` built with 10-way parallelism. The full test
  run completed with 11,277 of 11,278 assertions passing; its sole failure is
  the pre-existing hit-router copy mismatch recorded in `ui-bugs.md`.
- The post-merge automation fixture passed all 21 commands, including
  clear/undo and embedded save/reload. Native rendering was inspected in
  `/private/tmp/cycle-v2-guide-dock-native-final.png`; the filtered launch log
  for the post-merge fixture is
  `/private/tmp/cycle-v2-guide-heatmap-header-logs.txt`.
- The production diff and hot-loop scalar-math audit were clean, as was
  `git diff --check`. `clang-tidy` was not available in the local environment.

## Deferred Post-filter Contract

The existing IR modeller prefilter is not suitable for suppressing dominant
high-frequency heatmap detail: `buildIrPrefilterLevels` removes low FFT bins,
which increases the relative contribution of high frequencies. Reusing it
would therefore violate the requested behavior.

A Guide post-filter should be designed as a separately controlled low-pass or
anti-aliasing stage over the prepared 8192-sample table. Its cutoff mapping,
default value, phase behavior, and placement relative to Guide noise, offset,
phase, and downsampling need an explicit product contract before it is added.
