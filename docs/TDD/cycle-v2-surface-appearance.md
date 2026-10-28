# Surface appearance configurator

## Status

Complete: modal selectors, full gradient choices, preset persistence and cached
recolouring are implemented and verified. Native popup focus limits are recorded
below.

## Contract and ownership

Replace Cycle V2's separate text-only time and bipolar spectral style submenus
with View > Surface Appearance…, a modal dialog containing three labelled
selectors: Time surface, Bipolar spectrum, and Waveform density. Selected values
and menu choices show actual gradient strips. Choices preview immediately,
retain keyboard navigation and Escape/Done dismissal, and save with the preset.

TimeSurfaceStyles and ScalarSurfaceMaterial retain their existing style and
rendering policy. A waveform colour catalogue reuses those materials and the
existing Burnt Alum/Magenta resources. Full gradients retain density differences;
the rejected truncated Magenta ramp is removed. Burnt Alum is the default.
GraphCommandDispatcher owns presentation metadata; no palette selection changes
sound, graph revision, compilation, undo history, or audio-plan publication.

ReferenceWaveformPreview remains the only crossing counter. Source-space density
statistics remain fixed at 768 by 384. Palette changes recolour cached statistics
without recapturing audio, rebuilding the source/index, or recounting crossings.
Compact, expanded and comparison caches must observe palette changes. The modal
is workspace-owned and closed before its canvas is destroyed.

## Acceptance and deletion targets

- Remove old native text submenus; one modal opens through the View command.
- Each selector shows named strips, selection indication and routed input.
- Persist all three settings, including legacy/missing waveform values.
- Tests cover preset round trip, cache invalidation and palette-only recolouring.
- Actual-app checks cover opening, menu navigation, choices, dismissal,
  save/reload and unchanged audio/source/density work. Review dialog and open
  menu screenshots at production size.
- Build exact app/tests; run focused tests, architecture and code-map checks.

## Responsibility review

NodeCanvas retains presentation command routing and style synchronization only.
The new dialog owns controls, menus and layout; the waveform catalogue owns
choice identity and gradient lookup. Existing large coordinators gain no new
rasterization or interaction algorithms. Existing time-style menu automation IDs
remain compatibility routes to the same setters.


## Size and reuse review

NodeCanvas (3785→3799) and its header (495→498) retain presentation command
routing and shared style synchronization. NodeWorkspace (1021→1051) retains
workspace lifetime and composes the modal with its canvas; palette changes also
repaint comparison children. GraphCommandDispatcher (858→862) delegates to its
existing metadata edit helper. NodePreviewRenderer (1025→1026) adds palette
identity to the existing sprite key. Automation (1106→1130) exposes the same
View action, existing semantic setters, and named modal components.

ScalarSurfaceMaterial (1055→1072) extracts its dedicated default bipolar
material so preview generation cannot depend on the active global override.
Program gradient strips run the existing ScalarSurfaceProgram renderer on a
constant-height ramp; they do not copy its palette or compositing algorithm.
The prior helper silently rendered these programs using their base material.
The new dialog (238 lines) owns its three concrete selectors and menu rows.
WaveformColourStyles owns only preset IDs, labels and shared gradient lookup.
No editor state machine, capture, graph publication or rasterization enters the
modal, catalogue, or workspace adapters.


## Verification route

`test_cycle_v2_surface_appearance.py` uses the coordinator’s isolated instance
and the real dialog selector handler shared by popup callbacks. It checks
unchanged graph/document/preview revisions, raw signal fingerprints, source
preparation, density statistics and geometry. Expanded and pinned comparisons
both recolour without rebuilding those products. Tile keys also include the
existing authoritative material-style signature, so cyclograms update on time
or bipolar spectral style changes.

Initial 26- and 56-command runs exercised keyboard menu selection successfully.
Repeated isolated launches exposed native foreground focus failures: JUCE
correctly dismisses background-app popups; AXRaise/foreground activation was
unavailable in that session. No production focus override or launcher change is
retained. Stable acceptance drives the shared UI selector callback, native
Done/Escape handlers, persisted state and screenshots. Native popup navigation
should be rechecked in a foreground session if changing JUCE menu mechanics.


## Accepted evidence

- Exact Standalone Debug app and CycleV2_tests build successfully; final app
  evidence: `.context/surface-appearance-final-app-build.log`.
- The focused waveform, gradient, presentation, cache and gesture suite passes
  37 cases with 51,078 assertions. Evidence:
  `.context/surface-appearance-accepted-tests.log`. Existing surface-program
  parity and comparison checks also pass (4 cases, 102 assertions) in
  `.context/surface-appearance-material-tests.log`.
- `python3 scripts/test_cycle_v2_surface_appearance.py --no-build` passes its
  50-command actual-app fixture and cross-snapshot operation-count assertions.
  `presentationOnly` is true in
  `.context/surface-appearance-accepted-ui-report.json`.
- The existing 85-command waveform fixture also passes, covering zoom, scroll,
  density-to-path switching, output mode changes and pinned comparison behavior.
  Evidence: `.context/surface-appearance-final-waveform-report.json`.
- Production-size dialog and waveform captures were reviewed:
  `.context/surface-appearance-selected.png` and
  `.context/surface-appearance-waveform.png`. The selectors share 24-pixel side
  insets, 38-pixel control heights, 84-pixel row spacing and 16-pixel gradient
  strips. The Done button aligns to the same right inset.
- Code-map, architecture review, Python compilation, scalar-hot-loop/style
  review and whitespace checks pass. No native focus workaround is retained.
