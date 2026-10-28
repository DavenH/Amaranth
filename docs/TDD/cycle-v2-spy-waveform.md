# Spy waveform presentation

## Status

Complete: continuous capture, three-way view selection, stable heat colouring,
zoom/scroll and density-to-path rendering are implemented and verified.

## Contract and ownership

Time-signal spies offer Cyclogram, Spectrogram and Waveform. Waveform shows a
continuous mono sum over the existing one-second voice axis plus measured tail,
with the same note-off position. Physical pixels retain trace coverage. A fixed
384 by 192 crossing grid over the full normalized source supplies heat colours
from the existing Burnt Alum gradient. Bilinear colour sampling smooths bucket
boundaries without broadening or filling the fine waveform. The full-source
normalization and colour field remain fixed through resizing, zooming, scrolling
and the path transition. Silence remains empty. Compact and expanded
presentations share this encoding. Saved presets retain the selected
view; legacy frequencyView booleans retain their old meaning.

Authoritative sources: ReferenceWaveformPreview owns passage counting;
ScalarSurfaceMaterial owns Burnt Alum; GraphAudioExecutor and its processors own
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
- Density and Burnt Alum mapping distinguish passage counts and empty pixels.
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
  waveform, release-audition, cache, observer and continuity run passes 27 cases
  with 55,417 assertions. Evidence: `.context/spy-waveform-final-tests.log`.
- The 65-command `cycle-v2-agent-spy-waveform.json` fixture verifies authored
  and default-output modes, save/reload, density-to-path transition, horizontal
  scrolling, amplitude zoom, one source/index and heat-field build throughout zooming, and pinned
  comparison mode changes. Evidence: `.context/spy-waveform-final-ui-report.json`.
- At Retina size, the full view reports 7,861 crossings over 1,714 physical
  columns and uses density. Deep zoom reports 46 crossings and uses a path.
  The prepared source/index and full-source heat field each remain at one build. Screenshots are
  `.context/spy-waveform-detail.png` and `.context/spy-waveform-zoomed.png`.
- Fine-grid counting uses O(samples + pixels) work and O(height) difference
  scratch. The bounded heat grid is computed once per source. Premultiplied
  palette lookup, cached horizontal interpolation positions and row-wise
  vertical blending keep colour sampling allocation-free inside pixel loops.
  All expanded image work runs on the single worker; paint reuses cached pixels.
  The final 1714-by-1054 Debug full-view build measured approximately 43 ms,
  including first heat-field preparation; zoomed coloured paths measured 16–18 ms.
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
blue streaks. Burnt Alum waveforms use two resolutions: physical-pixel counting
supplies exact occupied pixels, and a source-space grid supplies heat colours.
The original 256 by 128 nearest-bucket version was visibly blocky. The final
384 by 192 field is finer, is normalized once for the complete source, and is
sampled smoothly with JUCE PixelARGB interpolation. It never fills background
pixels. Paths sample the same field while preserving their antialiased alpha.

Regression coverage checks the palette against full-source passage counts,
compares matching signal coordinates across sizes and zoom levels, verifies
colour continuity into path rendering, and checks that no occupied density
pixel disappears and no unvisited pixel is filled. The UI fixture also asserts
that heat statistics are built only once through horizontal and amplitude zoom.
Monochrome references and the indexed density-to-path transition retain their
established behavior.
