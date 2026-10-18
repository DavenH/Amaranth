# Cycle V2 Cyclogram Comparison

## Status

Implemented (2026-10-18).

## Product contract

The workspace has a resizable comparison dock below the graph. It keeps several
sample references and preset-owned pinned Spy views visible alongside an
expanded Trimesh editor. Reference selection sets the comparison pitch without
changing live MIDI input. Time-domain cards use the same phase, normalized
time, signed-amplitude mapping, and camera. A pinned Spy is another view of an
existing passive probe; the graph's canvas card remains present. An implicit
output Spy is identified as pre-wet. Spectral probes retain their own domain.

The reference library is independent of presets. It discovers factory WAVs in
Cycle content and stores user imports, edited pitch envelopes, and their source
audio under user application data. Presets store only ordered Spy pin IDs.
Imports and analysis run outside realtime processing. Reference audition is
mixed from a predecoded, prepared buffer in the standalone audio callback.

## Authoritative implementations and translation boundaries

- `PitchedSample::load` owns decode and peak normalization; add FLAC to this
  path and have Cycle 1 and V2 use it unchanged. `PitchTracker` owns fundamental
  and period analysis. `SampleUtils::processWav` owns Cycle 1 confidence and
  fallback policy.
- `PitchedSample::createEnvFromPeriods` owns reduction via
  `AutoModeller::reducePath`, and `createPeriodsFromEnv` owns the reverse
  period walk. Extract their data operations into a shared library core and
  keep mesh/library lookup as a Cycle 1 boundary translation.
- `VisualDsp::calcWaveSpectrogram` owns phase-folded sample time columns.
  Extract the time-column operation into a shared core; Cycle 1 continues to
  call that core before its FFT/phase work. No copied folding loop in V2.
- `GraphPresentationModel` and `PresentationPreviewRenderer` own targeted
  diagnostic probe capture. Extend their selected-probe path to capture the
  union of pinned upstream closures at the selected reference pitch, then
  publish an immutable comparison result. `GraphPreviewExecutor` remains the
  sole probe-grid conversion owner. Trimesh movement remains local; commit
  schedules one comparison refresh.
- `NodePreviewRenderer`, `TrimeshRenderProfile`, and the shared scalar-surface
  renderer own Spy/material display. A grid-backed surface host translates
  reference or probe columns into that presentation, without copying shader,
  camera, interaction, or DSP policy.
- The dock surface host uses the existing canvas OpenGL context. Each card
  renders with `TrimeshPanel3D`, `GLPanelRenderer`, and `GLSurfaceCache`;
  the cache's JUCE snapshot paints in the sibling dock. The canvas callback
  only schedules this render and closes its resources with the context.
- `GraphCommandDispatcher` owns any new probe creation and preset pin
  publication. The reference library owns reference undo and persistence; UI
  translates file selection and pointer events only.

## Complexity and negative boundaries

An envelope point movement changes only the selected reference's small pitch
state. A Trimesh movement does not copy the graph or regenerate the comparison
grid. Commit captures all pinned probes in one bounded selected-path
diagnostic request at one MIDI note; unchanged cards keep cached surfaces.
Scroll and camera gestures do not rerun analysis or graph capture. Audio
callbacks only index and mix already prepared samples, with no allocation or
decoding. No sample source becomes a graph node, and no domain algorithm lives
in a canvas or dock coordinator.

The compatibility code is limited to translating `PitchFrame`/mesh points,
probe previews, and prepared audio buffers. It may not duplicate tracking,
envelope sampling, phase folding, probe traversal, or material rendering.
The baseline at the start of this implementation was 3,403 lines for
`NodeCanvas.cpp`, 1,218 for `GraphAudioExecutor.cpp`, and 1,924 for
`VisualDsp.cpp`. The comparison coordinator's pin/capture translation lives in
`NodeCanvasComparison.cpp`; the algorithms stay in the existing graph, audio,
and surface owners. The old period-folding body in `VisualDsp.cpp` is deleted;
Cycle 1 and Cycle V2 call `WavePeriodFolding`. The old pitch reduction body is
extracted from `PitchedSample::createEnvFromPeriods` into a shared method.

After extraction, `NodeCanvas.cpp` is about 3,440 lines, up less than 50 from
baseline; `GraphAudioExecutor.cpp` is 1,256 lines, up 38; and
`VisualDsp.cpp` is 1,728 lines, down 196. `NodeWorkspace.cpp` remains a
layout and lifecycle owner: it creates the library/dock, routes callbacks,
and allocates space below the canvas. Pin eligibility stays in the dispatcher,
DSP preparation stays in `GraphPresentationModel`, and the reference library
alone owns pitch edits and persistence. No second policy decision site or
copied renderer remains.

`GraphCommandDispatcher` alone decides pin eligibility and presentation
revision publication. `GraphPresentationModel` alone prepares the changed
Trimesh step before a targeted comparison capture; the existing configuration
factory supplies the DSP policy. `PresentationPreviewRenderer` owns one batched
capture request. The dock supplies reference note and card placement facts.
No second graph traversal or surface rasterizer is introduced. The 3D surface
host translates columns into `Panel3D::DataRetriever` and uses the mature
`TrimeshPanel3D` renderer. A growing generic bridge or a second preview
pipeline would be a design failure.

For a parameter-only Trimesh gesture, the presentation model confirms that
the committed model pointer matches the compiled step. The DSP configuration
factory then copies only the small parameter/configuration record, sharing its
already prepared mesh and guide provider. It does not scan guide assignments
or prepare a mesh. If the model pointer differs, comparison capture waits for
the existing downstream refresh and reuses that prepared plan. This adds no
second mesh preparation to a vertex commit.

The Cycle 1 pitch fallback remains at tracker insecurity above 50. The
reference dock flags insecurity above 30 for manual correction without
changing that fallback.

## Completion and verification

- WAV, AIFF/AIF, MP3, FLAC, and Ogg imports produce a detected note, editable
  simplified pitch mesh, and phase-folded cyclogram. Short/silent/invalid input
  fails without partial persistence. Factory and user references reload.
- Pitch note/point correction changes only the associated cyclogram, supports
  undo/redo, and survives restart. Audition uses the normal output with one
  reference at a time and remains allocation-free in the callback.
- Pinning existing and implicit output Spies preserves canvas cards and saves
  ordered pins per preset. A connected Trimesh output can create and pin a
  probe through the dispatcher. Removed probes show a clear disconnected state.
- The selected sample and time Spy have matching axes/camera at the sample's
  note. Pinned Spy values match the authoritative targeted probe capture;
  spectral cards retain domain labels. During a Trimesh gesture there are zero
  comparison graph requests, then one after commit.
- Focused semantic tests, UI automation, production-size screenshots, operation
  counters, architecture audit, style/hot-loop review, and `git diff --check`
  pass. The dock and editor remain usable at 1280x760 and 1000x700.

## Verification

The focused CTest set covers all five decoders, short/silent/unsupported and
low-confidence input, sample reload and pitch undo/redo, ordered preset pins,
disconnected probes, and one-probe creation before pinning. The batched
capture test compares every value with a separate targeted capture at MIDI
57 and asserts that batch step visits do not exceed the sum of individual
visits. The 1000x700, 1280x760, and full-size agent runs keep the Trimesh
editor responsive, capture only after release, and show rendered sample and
Spy cards. The context creation assertion guards against blank dock surfaces.
The Cycle 1 UI capture and filtered logs show no incidental assertion.
The release-preparation test scales from zero to 128 unrelated nodes and
mesh cubes while asserting zero graph copies, mesh copies, serializations,
and linear graph/assignment scans. The mesh-copy counter is recorded at the
authoritative guide-preparation deep-copy site, so the old release path fails
that assertion.
The focused 1000x700 gesture fixture passes after the parameter-only change;
a vertex edit fixture confirms the model-changing path captures after the
downstream refresh. The full-size screenshot and architecture audit were
rechecked after both paths passed.
