# Wave reference assets workflow

Status: Implemented (2026-10-23).

## Waveform publication on expansion (2026-10-26)

Status: Implemented.

Opening a different reference with waveform mode retained loads its source while
the editor is hidden. The waveform's own visibility flag stays true, so its
Component visibility callback misses the ancestor becoming visible. No image
is requested until navigation or a lower-mode toggle changes the view.

Use JUCE's ComponentMovementWatcher in ReferenceWaveformView to observe effective
visibility through its ancestors. Keep image invalidation, cancellation, cached
frame reuse and asynchronous publication in this renderer; delete the incomplete
self-only visibility callback. Verify unchanged reopen, hidden retarget and a
new import expanded in waveform mode without zooming or toggling. Retain the
sequence in the existing waveform UI regression and capture before/after state
and screenshots. No analysis, zoom, document or playback policy changes.

The retained sequence failed before the fix with an empty image and no render
job after hidden retargeting. It now passes unchanged reopen, hidden retarget,
fresh import/expansion and the existing navigation/mode/audition checks. Cached
reopen retains the same image-build count. Before/after state, logs and native
captures are in `.context/waveform-first-open-before/` and
`.context/waveform-first-open-after/`. The app builds and all 27 focused
reference/thumbnail tests pass; no runtime assertions were observed. Architecture,
code-map and whitespace checks pass. Ownership remains in the 185-line renderer
(52-line header), with no new policy owner or source boundary. All criteria and
the self-only callback deletion are complete.

## Expanded waveform / pitch switch (2026-10-25)

Status: Implemented.

Two inlaid icons switch the existing lower plot between pitch envelope and
recording waveform, without adding a heading or scrollbar. Keep the pitch
ZoomPanel as the time-navigation owner in either mode; surface, waveform and
horizontal thumb use the same normalized recording interval. Retain separate
vertical pitch/amplitude framing across mode changes. The lower-view choice is
editor session state and does not change library metadata or the document.

Reuse ReferenceWaveformPreview's globally normalized stereo sum and density
rasterizer. Cache normalized samples and unlimited PeakCounter crossing offsets
once per sample; count visible crossings by binary search. Render a simple line
at <=1 crossing per physical horizontal pixel, density above that threshold.
ReferenceWaveformView owns a bounded latest-request worker and immutable source
snapshot; image preparation is off-thread, stale publications are rejected,
and repeated unchanged frames do not enqueue work. No audio decode, FFT, pitch
analysis or graph work occurs during navigation. Pitch input is hidden and its
GL work skipped while waveform is shown; existing ZoomPanel gestures remain
authoritative, including wheel, thumb drag and framing actions.

Verify density/line threshold and transitions in both directions, stereo/silence,
shared time pan/zoom, preserved independent vertical ranges, no pitch edits from
waveform input, unchanged analysis generations/document state, mode switches,
retargeting, audition and native captures at normal/narrow sizes. Delete the
paint-only pitch marker in favor of the actual mode controls.

Completion: lower icons use the existing pitch and waveform SVGs. The same
ZoomPanel retains its bars and gestures; ReferencePitchEditor translates lower
mode/vertical range and forwards waveform wheel input to the existing panel
input owner. ReferenceWaveformView holds one immutable normalized mono source,
cached zero-crossing offsets and one published frame. Its single worker removes
obsolete queued requests and checks a viewport revision before publication.
While a frame is pending, its predecessor is drawn at its correct time/amplitude
coordinates. Reopening an unchanged waveform reuses the frame. This viewport
renderer does not reuse the file-image cache's key/lifetime policy or duplicate
its catalog lifecycle.

ReferenceWaveformPreview adds viewport-aware density clipping and line rendering;
the line preserves fractional sample coordinates and neighboring samples at deep
zoom. PeakCounter retains its legacy 1001-crossing cap by default and accepts an
explicit unlimited request for this display. Vector operations prepare line
coordinates and density log scaling; no per-sample transcendental math is added.
The hidden pitch surface does not render or receive waveform drag edits.

Both app builds, 27 focused reference/thumbnail tests, the new live waveform
fixture and the previous linked-reference-view fixture pass. Exact-threshold
tests cover 1024 crossings/1024 pixels, 1025 crossings, physical-resolution
changes, legacy/unlimited counting, amplitude clipping and fractional sample
framing. Live checks cover density→line→density, wheel zoom, two wheel pans,
two thumb updates, amplitude fit, pitch-range restoration, cached mode toggles,
source retargeting, dense/deep-line screenshots, both window sizes, Space
audition and unchanged analysis/document revisions. Source preparation stays
at one build per sample; returning to an unchanged mode adds no image job.
Evidence: `.context/reference-waveform-ui/`, `reference-waveform-regression/`
and `.context/reference-waveform-*.log`. No assertions were observed.

Size review: ReferencePitchEditor grows 436→507 lines for composition/routing,
ReferenceWaveformView is 184 lines (49-line header) for latest-frame ownership,
and ReferenceWaveformPreview grows 101→155 for shared presentation algorithms.
No graph/model, serialization, audio callback or independent zoom policy is
introduced. Architecture, code-map and whitespace checks pass. All criteria
and deletion targets above are complete.

## Asset preview scrolling and density experiment (2026-10-25)

Status: Implemented.

Measure the same Debug build, 1000x760 window, baroque-flute graph and 32 smooth
asset-list scrolls in both directions before/after. ReferenceAssetBrowser owns
paint timing/counters through the existing PerformanceDistribution; snapshots
export these separately from canvas paint telemetry. Preserve search, audition,
selection and row-only dragging, with no document changes.

Extract PresetThumbnailCache's existing worker/message-thread publication and
file-revision cache into LibraryThumbnailCache. Presets retain their decoder;
assets supply a five-second PitchedSample/normalized stereo-sum image loader.
ReferenceAssetBrowser requests visible rows first and preloads adjacent pages,
retains bounded image storage and paints cached bitmaps. Visible-range iteration
must not scan the catalog per paint. No audio reader, decode, thumbnail reduction
or heatmap rasterization is permitted in paint. Cache persists through scrolling,
search and resizing; stale file revisions cannot publish over current images.

Experiment: time is horizontal, amplitude vertical, pixel intensity counts the
sample trace passages through each cell. Difference accumulation avoids walking
every crossed vertical pixel for every sample. Use vector log compression and
a neutral luminance palette, preserving silence and stereo cancellation. Render
off-thread at a fixed retina-sized row image; long sources show their first five
seconds. Delete the four-second exclusion, eager whole-library decode queue,
AudioThumbnail paint work and the copied async cache lifecycle in presets.

Verify density geometry/repetition/cancellation, five-second trim, async loading,
revision invalidation, bounded caching, prefetch, warm-scroll reuse and matching
before/after timings. Review native normal/narrow screenshots and row dragging.

Completion: LibraryThumbnailCache now owns the single worker/publication/revision
state machine for preset and asset previews. PresetThumbnailCache is a decoder
adapter; ReferenceAssetBrowser requests visible rows and one page on either side
through its move/resize/visibility hooks. Its 128-image LRU retains previews across
searches and layout changes. Density images are fixed 512x96 software bitmaps.
Audio decoding and density preparation run on two workers. Paint performs only
visible-row framing, labels and cached image drawing; no AudioThumbnail remains.

The same 32-scroll Debug workload measures 26.007→8.593ms mean asset-list paint,
about 3.03x faster. Mean row visits fall from 34 to 8.325 per paint. This measures
the asset list, not whole-window frame latency; rounded frames, text and scaled
bitmap composition remain in paint. All 34 catalog images build once, including
long sources; subsequent warm scrolls enqueue no additional image jobs. Reports
are in `.context/asset-previews-before/`, `asset-previews-after/` and
`.context/asset-previews-comparison.json`.

The exact app build and 24 focused reference/thumbnail cases pass. Tests cover
crossing counts, increased density on repeated passages, subpixel-length traces,
silence, stereo cancellation, quiet-source normalization, exact first-five-second
image equality, async warm reuse, file-revision races and bounded eviction.
Native search, drops on collapsed/open trays, one-row drag capture and audible
playback pass in `.context/asset-previews-native/`, without document changes.
The user reviewed and accepted the density appearance. Architecture audit, map
and whitespace checks pass. All deletion targets above are complete.

Ownership review: ReferenceAssetBrowser shrinks 386→371 lines; the shared cache
is 116 lines and the preset adapter shrinks 119→43. ReferenceWaveformPreview is
101 lines for mono preparation, density accumulation and image loading/rendering.
NodeWorkspace adds only one diagnostic property; it retains workspace routing
and delegates measurement to the asset owner. No worker, invalidation or image
publication mechanics are copied into the browser or renderer.

## Cycle-resolution follow-up (2026-10-25)

Status: Implemented.

Load at most five seconds through PitchedSample's decoder, before its existing
linked-channel peak normalization. Retain the source file. Replace the fixed
160x256 reference grid with one column per derived cycle, evenly selecting at
most 1024 cycles across the recording; keep their actual recording times.
Cyclogram rows follow the next power of two of the longest cycle, capped at
1024. Spectra retain full power-of-two transforms and physical harmonic bins;
log-frequency display resampling caps the presented rows at 1024.

WavePeriodFolding and WavePeriodSpectrum remain authoritative. Extend folding
to accept explicit column times without changing Cycle 1's uniform default.
ReferenceSampleAnalyzer owns cycle selection. ComparisonSurfaceView and the
shared scalar renderer translate irregular column geometry, including fallback;
they do not analyze audio. Cache products by analysis revision, never on zoom.
ReferenceLibrary owns each open reference's session view mode. Expanded editor
and tray use that mode; a neutral 32x22 icon pill on each card toggles it.
No preset/document writes, graph updates, or new DSP implementations.

Verify five-second trim/normalization at multiple rates, uncapped and evenly
decimated cycle selection (including 1536/2048), row caps without harmonic loss,
time alignment, cache reuse, mode synchronization/isolation, both modes and
pill geometry in production-size captures. Delete fixed-grid defaults and the
tray's unconditional time-domain presentation.

Completion: selected cycles retain their timestamps through folding, both cached
products and the presentation adapter. The scalar renderer draws texture strips
at those positions; uniform callers retain a single quad. Its shaderless texture
path shares the geometry, and Panel3D's older surface fallback uses the same
column coordinates. Explicit cycle folding preserves the final complete cycle
by zero-padding missing wrap context, with scratch space sized for raw cycles
even when presented phase rows are capped. Cycle 1's default remains uniform.
Mode changes update session state without saving library metadata or presets.

Review: 308 production C++ lines added, 80 removed. ComparisonDock grows
774→862 lines, retaining card composition, GL lifetime and event routing;
ReferenceLibrary owns mode policy and ReferenceSurfaceCache owns analysis reuse.
ReferencePitchEditor remains 493 lines and ComparisonSurfaceView 404, composing
the existing curve, zoom, material and panel owners. Shared Panel3D (1132 lines)
and GLScalarSurfaceRenderer (911) gain only column geometry; their existing
render/preparation collaborators retain algorithms, upload policy and lifecycle.
No new domain-kind branches, graph mutations or independent DSP core remain.

Evidence: both Cycle/CycleV2 builds pass; 22 focused reference/display cases and
19 shared material cases pass. The cycle fixture uses exact-length recordings
of 32, 1024, 1536 and 2048 irregular cycles, compares selected waveforms against
the complete render and checks a nonblank last column. Decode tests prove a
five-second bound at 44.1/96 kHz, normalization after trimming and linked stereo
gain. The live view script passes linked navigation, unchanged analysis on
zoom/toggles, matching tray/editor times, both pill directions, close/reopen
retention, narrow layout, audition and unchanged document state. Native Space
audition still passes all five focus contexts. Sax-growl presents 431x512 phase
and 431x201 harmonic grids. Logs, captures and reports are under
`.context/reference-cycle-resolution-*`. Architecture audit, map and whitespace
checks pass. Both fixed-grid defaults and the tray's time-only path are removed.

Compact pitch presentation: remove the 28px heading row and paint the existing
EnvelopePurposeIconRenderer pitch glyph inside the plot's top-left corner.
Framing icons retain their top-right placement. Remove the fundamental slider/
readout and the surface's duplicate zoom bars. Pitch zoom bars retain linked
horizontal navigation; the surface drawing width derives from the pitch plot,
keeping the axes aligned without an empty surface scrollbar gutter. Delete the
unused fundamental-control callbacks/parsers and surface scrollbar targets.
The reclaimed height goes to the surface. No new icon, control, analysis or
interaction owner is added; ReferencePitchEditor shrinks 498→436 lines and
ComparisonSurfaceView 404→398. The standalone build, live normal/narrow layout
and linked-navigation/mode/audition checks pass
(`.context/reference-minimal-controls-ui/`). The fixture checks the resulting
6px plot-to-plot gap. Architecture, map and whitespace checks pass.

## Contract

Wave References has a persistent disclosure control below the canvas. Its session
visibility is explicit and independent of which node editor is open. Imports and
explicit reference additions reveal it immediately; browsing and auditioning an
asset do not. An empty tray offers a direct route to Assets and import.

Assets joins the existing sidebar tabs. It reuses LibrarySearchField,
SidebarLibraryToolbar and SidebarMediaRow. Audio rows show a cached waveform
density image of the first five seconds, using a normalized stereo sum.
LibraryThumbnailCache prepares these on background workers through PitchedSample
and Buffer operations; audition audio remains unchanged. Rows support audition, keyboard selection,
explicit Add, double click and drag to the reference tray. Card titles retain names and direct play /
close controls; duration sublabels are removed.

## Owners and complexity

ReferenceLibrary retains catalog, import, analysis, pitch edits and active cards.
ReferenceAssetBrowser owns filtering, list selection and thumbnail presentation;
A shared LibraryThumbnailCache owns async image preparation/publication;
ReferenceWaveformPreview owns density rasterization. PitchedSample owns audio
decoding and StandaloneAudioEngine owns prepared audition playback. No DSP,
pitch tracking, graph commands or mesh gestures are duplicated. NodeWorkspace
owns tray disclosure and routes sidebar actions. Browser rows never select a
reference merely to audition it. Drag movement does not decode, analyze or edit
the graph; a successful drop uses the existing reference-selection boundary.

Delete the Trimesh-dependent visibility gate and duration card layout. Paint and
hit targets for each card action derive from the same geometry. Verify actual
clicks, independent disclosure, search, add/drop and audible output, alongside
focused reference tests and production-size captures.


## Icon and geometry contract

The five sidebar tabs use Curves (Bezier handles), Presets (stacked cards),
Patterns (piano roll), Nodes (connected blocks), and Assets (wave folder).
Reference actions use import-to-tray, waveform-plus, tray and plus.
Preset/pattern New/Edit/Rename/Delete retain their text buttons.
LibraryIconRenderer parses the existing embedded SVG resource pipeline
once, with neutral resting/highlighted variants. No cyan is introduced.

Tabs keep their existing 46 px hit height and receive a centered 24 px icon
canvas. Reference actions are 32x28 px with a centered 24 px canvas, in a compact
centered group with 8 px gaps. All SVGs use a 24-unit viewBox, a 1.7-unit primary
stroke and roughly 3-unit optical insets. The tray disclosure is 30 px tall;
its initial content height is 254 px, preserving the previous 284 px total.
Reference cards are 300 px wide; asset rows are 82 px high. Drag images contain
one row at native scale and stay above the pointer, rather than snapshotting
the list. Icons have accessible names and tooltips; the tray title remains a
section heading with a disclosure chevron, without a boxed text action.


## Completion and review

The production C++ diff adds 1002 lines and removes 86. New presentation owners
are ReferenceAssetBrowser (386 lines), ReferenceTrayBar (121),
LibraryIconRenderer (90), and ReferenceWaveformPreview (22). NodeCanvas grows
3613→3630 lines only for sidebar navigation and attachment. NodeWorkspace grows
1000→1021 for composition/routing and native-pointer diagnostics while deleting the deferred requested state
and timer/editor visibility checks. InlinePresetBrowser grows 798→835 for the
fifth tab and attached page; its preset indexing/editing owners remain stable.
ComparisonDock grows 706→755 for shared action geometry, drop routing and
file-specific audition feedback. These existing large coordinators retain
composition responsibilities; algorithms remain in the named collaborators.

The only new categories are the Assets UI tab and twelve icon identifiers.
There are no new DSP/node kinds, graph mutation paths or copied audio/pitch
algorithms. Imports keep the existing library boundary. Audio decoding remains
PitchedSample-owned; vector channel summation and presentation normalization
are isolated in ReferenceWaveformPreview. Drag motion uses JUCE and changes no
graph, analysis or audio preparation; one row snapshot is made at drag start.
The floating drag image stays above the canvas's native OpenGL view. Cached
waveforms repaint on decode completion and audition state changes, not idle
polls. No temporary compatibility path remains; the automation selection entry
point forwards to the same semantic reference action used by the UI.

Verification artifacts under `.context/` cover the standalone build, 32 focused
reference/browser/audio cases, all twelve SVGs, icon contact-sheet review,
explicit disclosure without an editor, unchanged visibility on editor open,
asset-only audition without tray activation, add-to-tray, real card clicks,
nonzero captured device audio, native search and drops onto both open/collapsed
trays, and unchanged preset revisions. The reference surface-selection fixture
and original empty-comparison fixture pass. Native checks also cover 1000x700
sidebar/playback and revealing Assets from an expanded editor. Icon passes used
the large/exact-size contact sheet, full-window screenshots and the narrow
native layout. Architecture audit, semantic map and whitespace checks pass.

The expanded Trimesh editor's crowded top pane at 1000x700 is recorded separately
in `ui-bugs.md`; reference browsing, disclosure and playback remain operable at
that size. The UI keeps neutral accents, including inherited scrollbars/hover
outlines. Source/synth material colours retain their domain visualization.

## Follow-up: stable cards and expanded pitch view

Status: Implemented (2026-10-24).

Restore the preset/pattern New/Edit/Rename/Delete row to its original text
controls. Asset actions and sidebar tabs keep their semantic icons. Preserve
library insertion order in the tray: selecting or playing a reference changes
selection, never placement.

Double-clicking a reference surface or title opens a canvas editor with that
reference's cyclogram above its pitch envelope. The rail remains visible and
keeps its order. ReferencePitchEditor composes the existing ComparisonSurfaceView,
CurvePanelHost, PropertySliderRow and ExpandedEditorChrome; no second envelope
interaction, pitch codec or surface renderer is permitted. NodeCanvas owns only
attachment, standard editor bounds, input dismissal and occlusion. ReferenceLibrary
retains pitch edits and undo. Delete the dock-wide envelope replacement and the
selected-card rotation. Live pitch movement remains local to the curve panel;
release updates the selected reference and its cached cyclogram once. The
cyclogram and envelope share horizontal bounds, separated by a compact pitch
heading/fundamental row. Escape or the standard close control dismisses the
expanded view. Verify real surface double-click routing, stable positions,
layout geometry, pitch commit/undo and tray preservation.

The empty pitch view also sampled time 0.5 despite EnvelopeMeshState placing
reference cubes in the static time-zero slice. The adapter now supplies time
zero, preserving the existing envelope rasterizer and gestures. Cmd-Z accepts
the native uppercase key code as well as the lowercase automation form.
Remove the four superseded New/Edit/Rename/Delete SVG resources and identifiers.

Review: production changes add 185 lines and remove 87. ReferencePitchEditor
grows 290→342 lines by composing the existing surface view and chrome.
ComparisonDock stays 755→756 while deleting rotation and dock replacement.
NodeWorkspace shrinks 1021→1018 and only translates automation coordinates.
NodeCanvas grows 3630→3674 (header 484→487): attachment, standard bounds,
dismissal and keyboard/occlusion routing remain its cohesive responsibilities.
NodeCanvasEditorCoordinator still owns editor geometry, ReferencePitchEditor
owns the reference layout, and ReferenceLibrary owns edits/history. The canvas
holds a non-owning SafePointer; the dock retains lifetime and GL teardown.
No graph/DSP kinds, mesh gesture implementations or analysis paths are added.
The cyclogram cache is keyed by sample identity and analysis revision; live
curve motion does not copy its grid, publish a document, or rebuild the graph.

The standalone build, 31 reference/browser cases, three fixtures
(`reference-expanded`, `reference-surface-click`, `reference-assets`), architecture
audit, semantic map and whitespace checks pass. Native verification with
`python3 scripts/test_cycle_v2_reference_editor.py --no-build` passes card
selection/placement, double-click, aligned cyclogram/envelope bounds, two live
drag updates without analysis publication, one release revision, changed derived
pitch frames, exact Cmd-Z restoration, close and Escape. Document revisions
and dirty state remain unchanged. The script uses a scoped instance and deletes
only its own UUID import, leaving live user presets/reference metadata alone.
Native replay uses the existing native-edit smoke's two-click sequence and
80 ms keyboard settling; earlier fast click/shortcut attempts missed delivery.
No application assertions or crashes occurred. Evidence is in
`.context/reference-editor-native/`, with full-window captures at 1280x900 and
1000x760, and `.context/final-reference-*-report.json`.

Expanded reference pitch envelopes fit vertically when opened, retargeted or
restored from history. ReferencePitchEditor prepares the static curve slice
before calling the existing fitEnvelopeVerticalRange; fitting never runs during
live pointer movement. The mature zoom panel retains its minimum range/padding
for nearly flat curves. The expanded-reference fixture asserts the resulting
vertical range is smaller than the full domain. Standalone build, fixture,
architecture audit and whitespace checks pass; the sax-growl capture at
`.context/reference-pitch-fit.png` confirms the fitted view without document edits.
The editor remains 343 lines with unchanged ownership and algorithms.

The expanded pitch envelope mounts its existing CurvePanelHost zoom wrapper,
exposing only the vertical bar. ZoomPanel retains range, pan, wheel and clamping
behavior; AmaranthLookAndFeel supplies the neutral track/thumb. ReferencePitchEditor
listens to scrollbar range changes only to request a fresh GL snapshot, including
when the shared wheel handler changes scale. The drawing/input host remains the
wrapper's child, with automation bounds translated from that child. The cyclogram
shares its content width, leaving the bar outside the aligned drawing areas.
The existing automatic fit still runs on open/history restoration, never during
pan or a live edit. There is no new zoom algorithm, document edit or analysis work.

Tray card input forwards wheel events to the existing JUCE Viewport, including
smooth horizontal trackpad events and the reverse direction. Headers and gaps
retain their normal viewport bubbling. The expanded reference surface has no
tray wheel callback and retains its own navigation. PanelInputHostComponent's
wheel override is protected so the read-only host can delegate its fallback;
the shared axis-lock/zoom implementation is unchanged. No scrolling mechanics
are copied. Scrollbar redraw requests are queued after ZoomPanel applies the
range, so GL reads the updated viewport.

Review: these two additions total 54 production lines added and 9 removed.
ReferencePitchEditor grows 343→370 lines and ComparisonDock 756→761, retaining
their composition roles and shared owners. The standalone build, two shared
trackpad tests, expanded-reference fixture, new reference-tray-scroll fixture,
architecture audit, semantic map and whitespace checks pass. The tray fixture
checks scroll positions 0→134→0→134 across card surfaces and headers while
preserving selection/order. The focused zoom check moves the vertical range
from 0.4771 to 0.5297 with unchanged span, analysis revision and document state.
Evidence: `.context/reference-tray-scroll-report.json`,
`.context/reference-pitch-zoom-check/` (including the full-window capture), and
`.context/reference-zoom-scroll-tests.log`. Broader native replay still has the
previously recorded input-delivery limitation; focused semantic gestures pass.

Space in the expanded reference toggles that reference's audition. The editor
emits an audition action and ComparisonDock calls its existing toggleAudition;
decoding, playback, stop behavior and card feedback keep their existing owners.
The canvas's expanded-editor key route takes precedence over preset transport.
The six production lines add no new state, audio path or policy owner. The
standalone build, space-audition fixture, architecture audit, semantic map and
whitespace checks pass. Native Space checks pass with both panel and pitch-curve
focus, including nonzero captured audio, stop, unchanged preset transport and
clean document state. Evidence: `.context/reference-space-report.json` and
`.context/reference-space-native/`.

## Expanded reference navigation

Status: Implemented (2026-10-24).

Retain the existing 160x256 cyclogram: the apparent resolution difference may be
zoom framing. The initial expanded spectrum used the Spy transform; the cycle
spectrum parity slice below supersedes that path with Cycle 1's source analysis.
Rebuild only for sample/pitch revision changes, never pan, zoom, mode changes or
live movement.

Expose Cyclogram/Spectrogram as paired icon controls and show the existing
surface zoom wrapper. Share normalized recording time [0,1] between surface and
pitch envelope; synchronize only horizontal position/span. Phase/frequency and
pitch amplitude retain separate vertical ranges. Retain Space audition and undo.
Reuse the IR overlay icon button by extracting its presentation class; add the
existing attack, vertical-fit and full-view icons inlaid at the pitch plot's top
right. Each control is 24x24 with the existing inset and 4px gap. View icons use a
24-unit canvas, neutral strokes and distinct wave/stacked-frequency silhouettes.
Disable envelope release sections in the reference editor, including the default
release tint and 1.5 time extent; preserve them in ordinary envelope panels.
Delete the hidden horizontal zoom context and the IR-local duplicate button. Verify revision-cache counts, mode FFT
output, linked two-update navigation in either direction, independent vertical
zoom, framing actions, retained edit/undo/audition, and actual-size captures.

The reference GL preparation hook no longer refreshes the rasterizer/zoom ranges
on every frame. Loading and history restoration prepare the static-time path;
the existing EnvelopeCurvePanel interaction owns live rasterization. This removes
the scrollbar overwrite race documented in ui-bugs.md. Navigation only updates
viewport rectangles and render invalidation, with no pitch analysis, graph copy
or history publication. Spectral conversion occurs lazily once per revision.

Responsibility/size review: ReferencePitchEditor grows 374→486 lines, keeping
editor composition and linked presentation; ReferenceSurfaceCache owns the
revision-derived surface and PanelOverlayIconButton owns shared control paint.
ComparisonSurfaceView grows 315→360 lines for the existing wrapper's bars and
camera notification. ComparisonDock remains 761 lines and only translates the
editor's own automation targets. The IR editor shrinks 652→615 lines after
removing its local icon button. EnvelopeCurvePanel grows 1133→1151 lines: its
new default-on release presentation flag controls existing tint/section and
normalized time bounds, with no new DSP or interaction policy. Its existing
rasterizer/interaction/drawing composition remains cohesive; this slice adds no
new responsibility to that large panel.

Verification: the exact standalone target and test target build. All twelve
reference test cases pass, including cached spectra, revision invalidation and
SVG coverage/contact-sheet rendering. The expanded-reference fixture and the
focused reference-views sequence pass: full time on open, no release section,
wheel zoom from either plot, two-way horizontal pan/drag, independent vertical
pitch pan, attack/fit/full actions, mode switching without reanalysis, unchanged
document revisions and Space audition. Screenshots cover both modes at 1280x900
and the smaller 1000x760 layout, with the tray open. The three icon reviews cover
large/native contact sheet, real control legibility and family consistency; the
spectral glyph was simplified to stacked harmonic ridges. Shared IR attack/full
controls and ordinary envelope release defaults pass their regression fixture.
Native pitch drag/commit and Undo restore the exact pitch path and pitch frames;
the native suite's later close click has the existing input-delivery limitation
recorded in ui-bugs.md. Architecture audit, code map and whitespace checks pass.
Evidence: `.context/reference-views/`, `.context/reference-navigation-tests.log`,
`.context/reference-navigation-expanded-report.json`,
`.context/reference-shared-controls-report.json` and
`.context/reference-navigation-native/`. No criteria or deletion targets remain.

## Tray focus and cycle spectrum parity

Status: Implemented (2026-10-25).

Clicking a reference surface or header must establish tray keyboard focus even
when it is already selected. Space from that context toggles the selected
reference through ComparisonDock's existing audition owner; canvas Space keeps
its preset behavior. Verify native click/Space with an ordinary node expanded,
a reference expanded and no reference editor, plus stop and unchanged transport.

Cycle 1 VisualDsp::calcWaveSpectrogram is authoritative: pitch-sized power-of-two
cycle buffers from WavePeriodFolding, Transform's DC-free harmonic magnitudes,
physical harmonic count from LogRegionMapping, and amplitude log tension 500.
The reference's fixed 256-row cyclogram must not feed its spectral analysis.
Share the mature folded-cycle FFT extraction with Cycle 1; cache the spectral
product per analysis revision. Spectrum presentation must use the signal log
mapping with an explicit harmonic-first layout, not mesh coefficients or a DC
row. Verify irregular cycle lengths, fundamental/upper harmonic amplitudes,
DC rejection, grid size and revision caching numerically, then inspect both
surfaces with their linked time zoom. No analysis or allocation on the audio
thread or navigation updates; no duplicated folding/FFT/log algorithms.

Tray slice complete: both surface and header selection explicitly focus the
ComparisonDock, including already-selected references. Its Space handler routes
to the existing toggleAudition. Native Space checks after direct UI click
dispatch pass for canvas, node editor, reference editor, reselection and header
contexts, with captured nonzero audio and unchanged preset transport/revisions.
The physical-pointer baseline attempt missed selection before reaching Space;
this is the existing native event timing limitation, isolated by direct UI click
dispatch in the retained regression. Evidence: `.context/reference-tray-audition/`
and `.context/reference-tray-audition-before/`. ComparisonDock grows 761→774
lines for focus, audition routing and a semantic card automation target; it
continues to delegate playback and source state to their existing owners.

Spectrum slice complete: WavePeriodSpectrum extracts Cycle 1's transform/copy
operations and sizing into the shared audio layer. Cycle 1 still applies its
existing log/phase presentation; ReferenceSurfaceCache folds tracked cycles at
that power-of-two size and caches raw harmonic magnitudes, without DC. It no
longer transforms the display thumbnail or uses Spy peak normalization. The
shared signal display profile applies the same 500-tension amplitude log and
LogRegionMapping, with explicit DC-layout and sample-rate metadata.

Visual review exposed a second bypass: TrimeshPanel3D deliberately uses a stepped
mesh path for magnitude, so its scalar display array was ignored. ComparisonPanel
is a narrow read-only adapter that enables the existing scalar material renderer
and consumes display coordinates uniformly. Both its scalar array and column
fallback now use the same mapped data; it does not copy rendering or analysis.
The reference's standard MIDI note is translated once at the legacy panel/profile
boundary. Non-default-rate harmonic grids are cached at note/rate changes under
the panel render lock; the ordinary default-rate path retains the shared region
bank. Grid publication takes the existing render lock before its source lock.

Both standalone apps build. The 17 focused reference/signal-mapping cases pass,
including analytic FFT magnitude and phase, a strong DC component, irregular
cycles and a 160th harmonic beyond the old thumbnail's resolution at 44.1, 48 and
96 kHz. The focused expanded-view sequence passes both sizes/modes, navigation,
cache counters and an actual spectrogram pixel-contrast assertion, which catches
the raw-amplitude render bypass. Evidence: `.context/reference-harmonic-views/`,
`.context/reference-spectrum-focused-tests.log` and the build logs. The broader
58-case filter has twelve existing fixture/mesh-preview failures recorded in
ui-bugs.md and audio-bugs.md; it is not a full-suite pass.

Size/ownership review: ReferenceSurfaceCache grows 43→70 lines for source-grid
composition. ComparisonSurfaceView grows 360→386 lines after dropping its unused
raw-grid copy; its local adapter only configures shared presentation. The shared
TrimeshPanel3D grows 228→238 lines for source-rate grid coordinates, and its render
profile keeps the existing default contract. VisualDsp shrinks 1728→1716 lines by
removing local FFT extraction; despite its existing large size, this adds no
responsibility or product branch. WavePeriodSpectrum is a 37-line shared core.
No analysis runs during navigation or live pitch movement. Architecture audit,
code-map and whitespace checks pass. All criteria and deletion targets are met.
