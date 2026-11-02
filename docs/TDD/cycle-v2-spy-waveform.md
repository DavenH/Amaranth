# Spy waveform presentation

## Status

Complete: black waveform/time-slice plots, shared panel grids, GPU heat
composition, earlier constant fill, popup presentation selector and inline zoom
controls are implemented and verified.

## Contract and ownership

Time-signal spies offer Cyclogram, Spectrogram and Waveform. Waveform shows a
continuous mono sum over the existing one-second voice axis plus measured tail,
with the same note-off position. Physical pixels retain trace coverage. A fixed
768 by 384 crossing grid over the full normalized source supplies heat colours
from the selected waveform gradient (Burnt Alum by default). The modal
Surface Appearance configurator owns palette selection; see
`cycle-v2-surface-appearance.md`. Bilinear colour sampling smooths bucket
boundaries without broadening or filling the fine waveform. The full-source
normalization and colour field remain fixed through resizing, zooming, scrolling
and the path transition. Silence remains empty. Compact and expanded
presentations share this encoding. Saved presets retain the selected
view; legacy frequencyView booleans retain their old meaning.

Authoritative sources: ReferenceWaveformPreview owns passage counting;
WaveformColourStyles supplies the selected shared gradient; GraphAudioExecutor and its processors own
continuous signal generation and lifecycle. Presentation adapters must not
flatten cycle-folded grids or synthesize approximate audio. Raw capture uses a
private prepared executor, with controllers, pitch and note lifecycle supplied
through the existing runtime context. It does not touch the live audio engine.

GraphCommandDispatcher owns view edits and undo for authored spies. Default
output view stays presentation metadata. UI paints immutable capture products;
unchanged repaints reuse pixels. Expanded and pinned waveform views reuse
ReferenceWaveformView with one prepared zero-crossing index and one lazily
prepared heat field. The view’s single worker alone accesses that field; stale
jobs retain their source but cannot publish over newer revisions. Zoom/scroll uses
the existing ZoomPanel/PanelInputHostComponent behavior. At no more than one
visible zero crossing per physical pixel, rendering switches to the shared
fractional-sample path. Density uses a one-column difference buffer, keeping
O(samples + pixels) work and O(height) scratch beyond its output. Waveform captures run only for requested time
spies, outside realtime execution. Cost is proportional to captured duration and
required graph processing; pointer motion and repaint perform no audio capture.

Deletion targets: replace two-state Spy view policy with one three-state enum,
retain legacy JSON only at the codec boundary, and reuse the reference passage
counter rather than introducing another density algorithm.

## Acceptance

- Saved view selection, authored-spy undo/redo and old JSON migration work.
- Continuous capture preserves sample order, channels and release timing; it
  is independent of traversal row reduction and rejects spectral-only sources.
- Density and heat mapping distinguish passage counts and empty pixels.
- Matching signal coordinates retain identical colour across compact, expanded,
  zoomed and path views; geometry retains its independent fine coverage.
- Compact/detail use physical pixel dimensions and retain cached presentation.
- Focused tests, exact-target app build, UI fixture and production-size capture
  pass; existing cyclogram/spectrogram behavior remains available.

## Size review

NodeCanvas remains the existing oversized canvas coordinator; this change adds
only menu routing and view selection. Waveform generation and painting remain
in dedicated runtime and presentation owners. GraphPreviewExecutor retains
extraction/fingerprinting, with no waveform generation algorithm added to it.

## Verification evidence

- Standalone Debug app and CycleV2_tests build successfully. The focused
  waveform, release-audition, cache, observer, continuity and causal-update run
  passes 63 cases with 55,944 assertions. Presentation-policy, gesture-session
  and controller coverage passes another 19 cases with 531 assertions. Evidence:
  `.context/spy-waveform-contrast-tests.log` and
  `.context/spy-waveform-mode-scheduling-tests.log`.
- The 85-command `cycle-v2-agent-spy-waveform.json` fixture verifies authored
  and default-output modes (including repeated changes without a graph edit),
  save/reload, density-to-path transition, horizontal
  scrolling, amplitude zoom, one source/index and heat-field build throughout zooming, and pinned
  comparison mode changes. Evidence: `.context/spy-waveform-contrast-ui-report.json`.
- At Retina size, the full view reports 7,861 crossings over 1,714 physical
  columns and uses density. Deep zoom reports 46 crossings and uses a path.
  The prepared source/index and full-source heat field each remain at one build. Screenshots are
  `.context/spy-waveform-detail.png` and `.context/spy-waveform-zoomed.png`.
- Fine-grid counting uses O(samples + pixels) work and O(height) difference
  scratch. The bounded heat grid is computed once per source. Premultiplied
  palette lookup, cached horizontal interpolation positions and row-wise
  vertical blending keep colour sampling allocation-free inside pixel loops.
  All expanded image work runs on the single worker; paint reuses cached pixels.
  The final 1714-by-1054 Debug full-view build measured approximately 53 ms,
  including first heat-field preparation; zoomed coloured paths measured 20–22 ms.
  These are single-run worker diagnostics, not Release benchmarks or paint
  timings. Non-waveform surfaces create no waveform component or worker until needed.
- Code-map, architecture, hot-loop/style and whitespace checks pass. A broader
  run reproduces three previously documented fixture failures; see the Spy
  waveform verification entry in `ui-bugs.md`.

## Responsibility review

The existing large owners retain their roles: NodeCanvas (3724→3785) and its
header (492→495) coordinate menu edits, child visibility and event routing;
zoom mechanics remain in ZoomPanel and rendering in ReferenceWaveformView.
GraphCommandDispatcher (849→858) owns semantic view edits; NodeGraph.h
(488→489) replaces a boolean with the shared three-state enum. GraphSerializer
(1386→1388) only migrates and persists that enum. The long-term canvas split
remains tracked in `cycle-v2-node-canvas-orchestrator.md`.

GraphAudioExecutor (1304→1305) retains all audio generation; its observer path
can inspect internal compiled steps, with normal observer-free execution
unchanged. NodePreviewRenderer (1004→1025) delegates waveform pixels to the
shared reference producer and retains existing sprite ownership.
ComparisonDock (862→865) passes the immutable waveform to its shared surface
host. Automation registration (1099→1106) and inspection (1344→1348) expose
that same behavior and diagnostics. No new algorithm, graph publication,
Envelope state machine or scalar sample-wise math is introduced in these
coordinators. The legacy frequency setter APIs remain narrow translations to
the enum; legacy booleans remain only in JSON decoding.

## Expanded heat refinement

The compact Spy is the visual reference for overlap richness. Enlarging the
colour-counting grid to Retina pixels dispersed crossings into low-contrast
blue streaks. Heat waveforms use two resolutions: physical-pixel counting
supplies exact occupied pixels, and a source-space grid supplies heat colours.
The original 256 by 128 nearest-bucket version was visibly blocky. The interpolated
field is now 768 by 384, is normalized once for the complete source, and is
sampled smoothly with JUCE PixelARGB interpolation. It never fills background
pixels. Paths sample the same field while preserving their antialiased alpha.

Regression coverage checks the palette against full-source passage counts,
compares matching signal coordinates across sizes and zoom levels, verifies
colour continuity into path rendering, and checks that no occupied density
pixel disappears and no unvisited pixel is filled. The UI fixture also asserts
that heat statistics are built only once through horizontal and amplitude zoom.
Monochrome references and the indexed density-to-path transition retain their
established behavior.

## Contrast and mode-selection follow-up (palette superseded)

The 2026-10-27 screenshot shows that sparse coloured paths remain difficult to
read against the grey plot. The heat grid doubles on each axis to 768 by 384.
The existing Magenta resource supplies its brighter 45–100% range: low crossing
counts retain visible saturated pink, and dense regions approach pale pink.
The complete opaque colour field must exceed 3.5:1 relative-luminance contrast
against the actual plot surface; interpolated trace coverage stays independent.

The output Spy’s view is presentation metadata and does not change graph or
document revision. Its request fingerprint previously contained graph revision,
pitch and controllers, so repeated mode changes were incorrectly deduplicated.
The before-fix fixture changes Spectrum → Time → Waveform: the Waveform selection
is recorded but `defaultOutputPreviewStats.waveformSamples` remains zero.
Evidence: `.context/waveform-mode-reproduced-report.json`.

PresentationRequestContext now carries the output view into the authoritative
request fingerprint. Both synchronous and asynchronous model requests supply
that value. No fake graph revision, cache bypass, recompilation or audio-plan
publication is introduced. The model regression checks repeated mode changes
with stable document, graph, compilation and audio-plan revisions. The actual
UI fixture exercises the same sequence before any graph edit.

The before-fix failing assertion now passes in the expanded 85-command fixture.
A separate model test cycles Time → Waveform → Spectrum → Waveform → Time and
checks raw waveform presence as well as unchanged graph/document/audio-plan
revisions and compilation count. The palette regression checks 768 by 384
source dimensions, visible contrast and retained high-density highlights.
Production-size dense and sparse-path screenshots were reviewed together;
contrast improves while geometry and zoom-stable colouring are preserved.


The fixed bright Magenta ramp was subsequently rejected because its density
bands were too similar. The active configurator TDD supersedes that palette and
its minimum-contrast acceptance rule with full-range, user-selectable gradients.
Fixed source-space statistics and geometry are retained across recolouring.


## Black plots and GPU heat composition (2026-11-01)

The expanded waveform and time-domain mesh slice use black plot backgrounds.
Waveforms reuse the owning Panel's major/minor grid coordinates (grey levels
0.14 / 0.085) through a narrow Graphics adapter; no second spacing or zoom
algorithm is introduced. Plot bounds also clip the grid, coverage, heat and
existing overlays.

The existing canvas OpenGLContext already renders JUCE component Graphics via
createOpenGLGraphicsContext. Previously, waveform workers rebuilt fully coloured
physical-size images for every viewport. Workers now return coverage only using
the shared density/path producer, plus the unchanged fixed source heat image.
WaveformHeatRenderer uses JUCE's OpenGLGraphicsContextCustomShader to sample the
coverage and fixed heat textures in one fragment shader, preserving premultiplied
alpha and stable source coordinates. Views without a GL paint backend retain worker-coloured, cached bitmaps.
Software snapshots lazily recolour the same coverage once, then reuse that image;
neither path builds CPU image-alpha edge tables during live zoom. JUCE image-alpha clipping was measured and rejected for the live
path: its shared edge-table clipper scans/resamples masks on the CPU even in the
GL Graphics backend. The final shader avoids that hidden CPU path.

The canvas retains its existing GL context and lifecycle. A context-associated,
waveform-specific cache holds at most eight image pairs; unchanged paint/zoom
transforms reuse uploads, palette changes reuse coverage, and replacing a source
reuses/evicts a bounded slot. JUCE OpenGLTexture handles upload/storage and context
teardown releases resources on the GL thread. RenderResourceCache tracks existing
CommonGfx Texture pointer versions and does not supply shader texture ownership;
no duplicate generic cache facility is added. There is no new GL host or readback.
Source/index, heat statistics and geometry counters remain stable on palette edits.

The deterministic Stengah Debug workload is
`cycle-v2-agent-waveform-zoom-performance.json`: warmed expanded waveform,
eighteen wheel zoom steps with six intermediate settlements, final sparse path.
Inspect worker times, canvas paint/repaint distributions and actual GPU paint
counts; compare identical app sizes and workload. Before report:
`.context/waveform-zoom-before.json`. These are local Debug measurements, not
Release frame-rate guarantees.

User screenshot also shows phase-dependent striping against bucket spacing.
Follow-up options: filter source-space counts before nonlinear palette lookup,
and fractional/supersampled trace coverage. Filtering the coloured image would
mix palette bands. Any filter should remain source-anchored to preserve colour
under zoom. This change does not claim to eliminate that aliasing.


Size/responsibility review: Panel.cpp grows 1307→1325 and Panel.h 297→298.
Panel remains the owner of zoom mapping and calculated grid positions; its new
18-line Graphics adapter translates those coordinates without adding spacing,
interaction, or cache policy. Extracting this single adapter would require
exposing the same protected grid storage without removing ownership. The new
WaveformHeatRenderer is a cohesive 128-line GPU compositor with bounded
context-owned textures. ReferenceWaveformView coordinates immutable worker
coverage/heat products and selects live GPU or cached software painting.
ComparisonSurfaceView and ReferencePitchEditor add only their existing panel
grid callbacks; no graph/DSP work enters a gesture or paint operation.


Verification evidence for the black/GPU slice:

- Exact Standalone Debug app and test target build: `.context/waveform-grid-accepted-build.log`.
- Focused waveform/gradient/presentation/cache/gesture coverage: 40 cases,
  69,736 assertions in `.context/waveform-filled-path-accepted-tests.log`. Bitmap
  fallback coverage and source-anchored colours are checked alongside the
  existing density/path and palette parity tests.
- The native screen captures `.context/waveform-black-grid-full-native.png`,
  `.context/waveform-gpu-native.png`, and `.context/time-surface-black-native.png`
  establish actual shader output, full/sparse traces, shared grid and black
  time slice. App-side screenshots alone would not verify native GPU output.
- Identical pre/post Stengah Debug workload: warmed GPU redraws average about
  3.1 ms, maximum 9.1 ms in `.context/waveform-zoom-gpu-final.json`. Worker
  density preparations at the intermediate zoom samples fall from about
  45 ms to 27–28 ms; final sparse path preparation falls from 22 ms to 8 ms.
  The first sparse-path sample remains about 84 ms (previously 99 ms): path
  rasterization is still a worker bottleneck, while cached texture transforms
  update independently. These sampled timings are descriptive local results,
  not a general frame-rate claim. Initial shader compilation/upload can cost
  more than warmed drawing.
- Context recreation retains CPU source/index, fixed heat statistics and
  geometry while recreating shader resources under the existing context owner.
  GPU heat paint counters continue increasing after context create count reaches
  two. Software snapshots no longer change the GL view's physical resolution
  or trigger needless coverage jobs.


## Filled sparse-path detail

Once the visible signal has at least six logical pixels per zero crossing,
the shared resolution classifier enables filled path detail. This threshold is
independent of Retina scale and distinct from the earlier one-crossing-per-physical
pixel switch to a stroke. The same reference path closes to the bipolar zero
line with a constant neutral fill at 20% opacity, following Panel2D's existing
centre-baseline convention; its original edge remains opaque with a
1.5-logical-pixel stroke. The edge retains its source-anchored heat colour.
GPU composition needs no extra geometry pass or source statistics. Monochrome references share the same
geometry. Density and intermediate stroke-only views remain unchanged.

Focused tests cover both polarities, translucent interior, opaque/wider edge,
no fill outside the trace, a zero line outside the cropped viewport, and equal
classification at normal and Retina scales. The zoom fixture asserts filled
detail at its final zoom, with one source/index and one heat-statistics build.

Final filled-detail evidence: `.context/waveform-filled-path-native.json` passes
the zoom, filled-detail, source-statistics and context-recreation assertions.
The native Burnt Alum capture is `.context/waveform-filled-path-native.png`.
The complete 85-command Spy fixture passes again in
`.context/waveform-filled-path-spy-regression.json`; palette persistence and
unchanged source/heat/geometry work pass in
`.context/waveform-filled-path-palette-regression.json`. Final exact app/tests
build: `.context/waveform-filled-path-accepted-build.log`. With the filled detail
enabled, deep-zoom worker preparation is about 22 ms, while warmed GPU redraws
average about 3.15 ms. The extra fill rasterization stays on the worker and does
not enter the live texture-transform path. Density-to-stroke and stroke-to-fill
are separate transitions; image-cache identity includes physical scale so the
logical stroke width remains correct after display changes.

Palette review also captures the filled detail with Blues in
`.context/waveform-filled-blues-native.png`. The corresponding report confirms
that switching the filled view's palette increments colour builds while source,
heat statistics and geometry counts remain unchanged. Native Burnt Alum and
Blues detail were reviewed together with the unzoomed density and time-slice
plots.


## Popup controls and earlier constant fill

The detail popup uses the shared PropertySegmentedSelector for Cyclogram,
Spectrogram and Waveform, routed through the existing setSpyView command service.
SignalProbeDetailPanel owns the popup's child controls and waveform host; the
canvas retains probe lifecycle, capture and model routing. This replaces the
canvas-owned waveform child setup and overlay callback. The header reserves
32 additional logical pixels above the existing note-off timeline; the selector
is centred in the popup and remains separate from release drag hit targets.
Attack/full buttons reuse PanelOverlayIconButton and ZoomPanel's existing actions,
aligned 8px inside the plot's upper right. Non-time sources omit the mode selector.
Camera state survives presentation changes. Attack/full and live zoom keep the
source/index and heat statistics cached.

Interactor owns wheel axis/modifier behaviour and now accepts a default-1 zoom
sensitivity. Spy waveform hosts and waveform comparison cards select 1.5;
other views retain 1. Pinch ratios use the same exponent in the shared
comparison host. No wheel or scroll state machine is copied. One unsmoothed step
retains 70.71% of the horizontal interval instead of 79.37%.

The fill/wider-edge threshold halves from twelve to six logical pixels per
crossing, enabling it one zoom octave sooner. Stroke coverage is encoded in RGB,
combined stroke/fill coverage in alpha using a black fill plus white stroke.
Software and GPU colour adapters recover stroke = red and fill = alpha-red;
only the edge samples the heat palette. This preserves antialiasing with one
coverage image and the existing heat texture; no extra texture, counting pass,
or source preparation is added. The fill is uniformly neutral at 20% opacity.

Completion criteria: routed pill switching on authored and default-output Spies,
attack/full actions, retained viewport across mode switches, 50% wheel strength,
earlier fill, palette-independent fill on both polarities, native GPU screenshot,
existing zoom/palette/release regressions and architecture/style checks.

Size/responsibility review for popup controls: NodeCanvas.cpp 3799→3800,
NodeCanvas.h 498→498 and NodeCanvasAutomationInspector.cpp 1348→1351.
SignalProbeDetailPanel (110 lines) now owns child construction, layout, input
forwarding and waveform overlay; the canvas only supplies source facts, model
view and command callbacks. The inspector only translates supplied target bounds.
This removes the former canvas-owned waveform setup rather than adding selector
and zoom-button lifecycle there. Existing broader canvas/editor/presentation
extraction plans remain applicable; this slice adds no new graph policy owner.
ComparisonSurfaceView 472→504 stays the shared ZoomPanel host; ReferenceWaveformView
332→333 still coordinates cached worker products and composition. Interactor.cpp
2543→2542 remains the mature wheel/axis interaction owner with one configurable
ratio; unrelated mesh interaction and gesture rules are untouched. The renderer
adds uniforms and coverage decoding to its existing texture composition boundary.

Verification: exact CycleV2 and CycleV2_tests builds pass. The focused waveform,
palette, presentation/cache and release-audition suite passes 47 cases with
141,344 assertions (`.context/spy-popup-final-tests.log`). New routed popup
fixture `cycle-v2-agent-spy-popup-controls.json` passes 83 commands covering mode changes on authored
and default-output Spies, retained camera, attack/full, wheel ratio, cached source
statistics, Escape and timeline handle routing. Existing waveform regression
passes 85 commands and the appearance/persistence suite passes 55 checks.
Software composition tests prove uniform fill across red/blue palettes on both
polarities with preserved coverage alpha. Native GPU captures are
`.context/spy-popup-native.png` and `.context/spy-popup-preview.png`; diagnostics
confirm filled paths, GPU heat paints and one source/index/heat-statistics build.
Architecture audit, semantic code-map validation and diff whitespace checks pass.

Final routed release check exposed an existing waveform hit-test gap: rejecting
mesh pointer-down did not let it reach the canvas. The detail panel now disables
plot interception in the shared host and its ZoomPanel container using JUCE's
existing child interception flags. Scrollbars retain their own targets, and
wheel/pinch still use canvas forwarding. Comparison cards keep their click
callbacks. The focused fixture asserts release-line down/up at the plot centre;
this replaces the prior state-only proof for that path.
