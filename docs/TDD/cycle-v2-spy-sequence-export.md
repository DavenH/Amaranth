# Automated Spy sequence export

Status: Implemented (2026-11-02).

## Contract and ownership

Export opaque, plot-only Cyclogram PNGs at `index / fps < pattern duration`.
The preview note and saved Spy release position stay fixed; only pattern MIDI
controllers advance. This is a new offline presentation workflow, not live
performance recording. Defaults: default-output, 30 fps, 1280x720, preset note
and preset time-surface style. Explicit named styles include Bullion.

Python owns frame iteration, source snapshots, manifests and failure reporting.
Optional `--mp4` runs installed ffmpeg after all PNGs finish, creating
`sequence.mp4` (H.264/libx264, CRF 18, yuv420p, faststart) at the same exact
rational frame rate. Pad odd dimensions on the right/bottom to even dimensions.
Keep all PNGs; publish the video from a temporary MP4 only after successful
encoding. Manifest video status/dimensions distinguish encoding failures from a
failed frame, and partial videos are removed on failure or interruption.
PatternPlaybackSchedule/PatternControlScheduler remain the interpolation owners;
exportPatternSchedule retains its existing validation and event-capacity limit.
The coordinator owns isolated exact-PID launch/cleanup. Extend clean exec to use
its existing scoped lifecycle rather than implementing a second launcher.

GraphPresentationModel owns synchronous batched controller refresh, using cached
dependency roots. Single-controller callers delegate to the batch implementation.
SignalProbeDetailState owns shared expanded capture preparation, extracted from
NodeCanvas::openProbeDetail. SpyFrameRenderer coordinates that capture and the
existing NodePreviewRenderer/ScalarSurfaceMaterialEvaluator production renderer.
TimeSurfaceStyles owns all gradient IDs, labels and recipes. An explicit optional
render-profile override selects the material without changing global selection
or preset metadata; normalization and material cache identity use that override.
Automation adapters only decode requests and write PNGs.

## Boundaries and deletion targets

No duplicated scheduler interpolation, DSP, normalization or recipe algorithms.
Remove duplicated expanded-capture preparation from NodeCanvas. No graph/model
copy, serialization, compilation, document/history edit, or audio-plan publication
per frame. Controller refresh scales with changed roots and required downstream
products, independently of unrelated graph content. Image rendering scales with
the requested plot and existing bounded recipe filtering. No realtime allocations.
No new UI controls, live capture, audio output or other Spy modes.

NodeCanvas and GraphPresentationModel are pre-existing large orchestration roots;
the former becomes smaller through capture extraction, and the latter retains
controller invalidation ownership. New export policy belongs to focused services.

## Acceptance

- [x] CLI exports exact timestamps, consecutive filenames and recoverable manifest.
- [x] Catalog listing and case-insensitive ID/label selection, including Bullion.
- [x] Saved/default style inheritance and explicit override without mutation.
- [x] Shared expanded capture and plot parity for authored/default-output Spies.
- [x] Batch refresh, history/publication stability and unrelated-state cost proof.
- [x] Invalid targets/domains/arguments and output failures report cleanly.
- [x] Two real app sweeps have identical corresponding PNGs and changing content.
- [x] Focused tests, exact-target build, code-map and architecture checks pass.
- [x] Optional ffmpeg MP4 preserves frames, rate/count, and handles odd sizes/failures.

## Evidence

- Exact CycleV2 and CycleV2_tests targets build with `--parallel 10` in
  standalone-debug and tests presets respectively.
- Focused C++ selection `[spy-export],[preview-controller],[surface-program],
  [pattern-schedule]`: 14 cases, 818 assertions. Covers all catalog IDs/labels,
  material parity, note override, saved release timing, disconnected/spectral
  targets, PNG publication failures, document/history/publication stability,
  no graph/mesh copies or serialization, and two changed controller roots with
  1 versus 80 unrelated nodes. Log: `.context/spy-export-cpp-tests-final.log`.
- Host/export/coordinator/session Python suites: 29 tests pass. Include exact
  boundaries, multiple lanes, nonintegral frame rates, failure/interruption
  artifacts, and isolated coordinator arguments. Log:
  `.context/spy-export-mp4-all-tests.log`. MP4 coverage includes worker flag
  forwarding, success/failure/interruption reporting, partial-file cleanup, and
  real ffmpeg/ffprobe verification of fractional rate, frame count and odd sizes.
- Real `--mp4` export: `.context/spy-export-mp4/sequence.mp4`, H.264/yuv420p,
  six frames at 6 fps, one second; 321x201 PNGs retained and padded to 322x202
  for video. Encoding status and dimensions are recorded in its manifest.
- Two isolated honerism-3/Bullion sweeps at 6 fps, 640x400: all six corresponding
  PNGs are byte-identical and all six frames within each sweep differ. Source
  files retain their original hashes. Proof: `.context/spy-export-repeatability.json`;
  outputs `.context/spy-export-bullion-a2` and `.context/spy-export-bullion-b`.
- Real authored probe2/Greyscale export, preset-inherited Bullion export, full
  15-entry catalog listing, and Stengah Spy/B0 export complete. Example:
  `.context/spy-export-stengah-bullion/frame-000001.png`.
- Existing opened/pinned Spy continuity regression passes all 53 commands:
  `.context/spy-export-refresh-regression.json`. Expanded waveform, spectrum,
  root-note and undo/redo behavior continue through the shared capture path.
- Code-map validation, architecture audit and diff whitespace checks pass.
  Runtime export logs contain no Cycle assertion or crash.

Size/responsibility review: NodeCanvas.cpp 3776 -> 3745 lines removes expanded
capture mechanics; NodeCanvas.h 493 -> 496 adds only typed service routing.
CycleV2Automation.cpp 1130 -> 1147 adds command delegation only; its existing
legacy command surface remains outside this slice. GraphPresentationModel.cpp
716 -> 734 retains cached-root invalidation and shared synchronous refresh.
SignalProbeDetailView.cpp 265 -> 301 owns expanded capture/result preparation
alongside its existing detail state. New SpyFrameRenderer (67 lines) owns plot
composition; SpyFrameAutomation (133 lines) owns decoding/catalog/PNG transport.
No duplicated DSP, interpolation, material evaluation or normalization remains.
