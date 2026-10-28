# Spy waveform presentation

## Status

Complete: the finer high-contrast heat field and output Spy mode refresh fix
are implemented and verified.

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
