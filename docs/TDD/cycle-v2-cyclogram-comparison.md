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

The comparison shelf is a session-level, elective View. Opening a Trimesh
editor alone leaves it closed and does not capture pinned Spies. The View menu
toggles it while a Trimesh editor is open. An explicit reference import or
selection also requests it; a request made before opening Trimesh takes effect
when the editor opens. Closing the editor hides the shelf without clearing the
session choice. Preset loading changes neither the shelf choice nor the
reference catalog. `NodeWorkspace` owns this visibility policy; the dock only
reports a successful import and renders its sources. No graph command or
comparison capture is needed to open an editor with the shelf closed.

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

For the elective shelf correction, the affected large files retain their
existing responsibilities: `NodeWorkspace.cpp` owns workspace layout and
visibility, while `CycleV2Automation.cpp` translates agent menu commands.
The dock reports import success without deciding workspace visibility, and
the menu and automation call the same workspace toggle. Pin eligibility and
capture scheduling remain in the dispatcher and canvas respectively. This
adds no duplicate visibility policy in the dock or graph layer.

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

### Reference pitch boundary (2026-10-22)

`PitchedSample` and its reduced pitch paths retain the mature Cycle note
convention (A440 = 81). Cycle V2 preview voices and visible note labels use
standard MIDI (A440 = 69). `ReferenceSampleAnalysis::standardMidiNote` owns
that boundary translation using the existing `LogRegionMapping` pitch bias;
the comparison dock passes the translated note to its existing Spy capture.
The pitch editor translates its label and text input while retaining legacy
slider/model values, undo and persisted metadata. No tracking, folding,
oscillator or library migration changes are needed. Schema 1 analysis exports
retain their existing legacy-valued fields and add explicit convention metadata
and `standardMidiNote` values. These constant-time translations add no analysis
or graph capture work. The affected presentation files remain below 800 lines.

The generated 293.665 Hz reference test verifies legacy note 74, standard note
62, the reconstructed period, the existing preview oscillator clock at 62,
unchanged pitch path, semitone edit, exact undo and persisted reload. All nine
focused reference cases pass. The comparison fixture additionally checks that
both the reference card and pinned Spy receive standard note 45 for the factory
sax-growl reference, while the performance keyboard's note remains 73.
The rebuilt native fixture passes. Four imported D3 source/synth cards report
note 62, and the pitch editor visibly labels it D4/62 rather than interpreting
legacy 74 as standard MIDI. The canonical pitch frames and every folded grid
value remain identical; only the analysis revision changes on reload. Native
state, screenshots and fixture evidence are in `.context/sax-wave-comparison/`.

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

The elective-shelf correction passes the focused empty-comparison fixture:
opening Trimesh leaves the shelf hidden with zero captures, the View command
shows it, and a second command hides it without discarding the selected
reference. The reference-surface click fixture still passes. Frontmost OS
captures in `.context/shelf-hidden-os.png` and `.context/shelf-os.png` show
the editor at full height before the choice and rendered sample/Spy cards
after it. The background cyclogram fixture passes capture-count and card
assertions; its first-GL-context assertion is tracked in `ui-bugs.md`.
The release-preparation test scales from zero to 128 unrelated nodes and
mesh cubes while asserting zero graph copies, mesh copies, serializations,
and linear graph/assignment scans. The mesh-copy counter is recorded at the
authoritative guide-preparation deep-copy site, so the old release path fails
that assertion.
The focused 1000x700 gesture fixture passes after the parameter-only change;
a vertex edit fixture confirms the model-changing path captures after the
downstream refresh. The full-size screenshot and architecture audit were
rechecked after both paths passed.

## UI revision (2026-10-18)

The comparison dock now uses the existing Cycle icon-button treatment and a
single source menu. File > Import Wave Reference provides a global entry point;
the dock menu lists factory and user references, existing Spies, the implicit
pre-wet output, and eligible Trimesh-output probe creation. A wave reference
remains library-owned because it is an authoring target shared across presets,
with no audio edge or graph lifecycle to represent as a node. Per-card audition
and context actions replace the permanent text-button row. The sample's note
and pitch path live in an expanded view backed by the existing envelope curve
panel, not a second curve interaction implementation. The panel commits a path
once on gesture release and the reference library owns undo, persistence, and
cyclogram refresh. Valid simplified paths may contain equal adjacent times;
the point-path boundary preserves their order.

The time surface's scalar renderer expects unit material values. The dock now
uses `TrimeshRenderProfile::mapGridToDisplay` to convert signed time values
from `[-1, 1]` to `[0, 1]`, with source display gain applied before the
conversion. Raw sample and Spy values remain unchanged. The sample's display
gain compensates for Cycle 1 period folding's half gain; the implicit output
Spy's display gain compensates for its existing threefold preview boost.
Spectral cards continue through the profile's spectral mapping. Per-card
scrollbars are hidden while camera and zoom remain linked; the rail's neutral
scrollbar appears only when cards overflow.

The separate Compare header was removed after a space audit. The source menu
occupies a fixed left gutter beside the scrollable cards, so it remains
available when the rail overflows. A 6 px strip retains the resize grip above
the cards. At 1000x700, the surface view height increased from 199 to 227 px;
the focused gesture fixture still reports one comparison capture on release.
The production-size dock screenshot is
`docs/TDD/figures/cycle-v2-cyclogram-comparison/comparison-dock.png`.

Opening a Trimesh editor must leave the comparison rail empty when the current
session has no explicitly opened samples and the preset has no Spy pins.
Factory and imported references remain available in the source menu. Open
sample cards survive preset switches in the current session, but the library
does not reopen them on the next launch. Its catalog, imported audio, and
pitch edits still persist. The dock does not select the first factory record
during capture refresh.

The comparison 3D panel uses the mature Trimesh renderer and `ZoomPanel`, but
its input host is read-only. `PanelInputHostComponent` currently forwards a
surface click to `TrimeshInteractor3D::mouseDown`; that reaches
`Interactor::findClosestVertex` with no mesh and crashes at `Mesh::getNumVerts`.
The read-only host rejects editor pointer gestures and forwards a simple
sample selection to the dock. Camera wheel behavior remains with the existing
host and `ZoomPanel`; no duplicate camera or mesh interaction is introduced.
The old `library.json` open/selected restore path is deleted. Its persistence
test now asserts session-only activation while the reference catalog reloads.
A focused fixture verifies explicit selection and a surface click without mesh
editing or a crash. A second run seeds an old saved-open list and asserts an
empty dock before explicit selection. A native macOS click selects the other
reference while the app remains alive; the screenshot is
`.context/reference-native-click.png`.

The focused `ctest` run passes both reference-library cases. The saved-open
UI run reports zero cards before explicit selection, and the surface-click
fixture reports the comparison panel host as its pointer target and changes
the selected reference. `ReferenceLibrary` owns active reference lifetime,
`ComparisonInputHost` rejects mesh-edit events, and `ComparisonDock` translates
a click into selection.

The focused reference test covers pitch path commit, undo/redo, cyclogram
change, and reload using a real Cycle WAV whose simplified path contains equal
adjacent times. The surface test covers signed input and implicit-output
scaling without source mutation. The
`scripts/fixtures/cycle-v2-agent-reference-pitch-edit.json` fixture opens the
expanded view, performs a real move/down/drag/drag/up curve gesture, and
asserts one cyclogram revision after release. Run it with
`CYCLE_APP_ARGS="--reference-library-dir=$PWD/.context/reference-pitch-fixture-library-<fresh-id>"`
with a new directory each run to isolate persistent edits from the normal
reference library. The full-size
and smaller-window captures are review artifacts under `.context/`.

The responsibility review found the dock at 696 lines after this revision,
below the 800-line review trigger. It owns card placement, the source menu,
and presentation routing; `ReferencePitchEditor` owns the curve view,
`ReferenceLibrary` owns pitch state, and `TrimeshRenderProfile` owns display
mapping. `NodeWorkspace.cpp` remains a layout and automation target owner at
906 lines; `CycleV2Automation.cpp` remains command routing at 1,014 lines.
Their new calls forward facts and actions without repeating import, pitch,
capture, or eligibility policy. No graph, audio, mesh, or renderer policy moved
into these high-level files.
