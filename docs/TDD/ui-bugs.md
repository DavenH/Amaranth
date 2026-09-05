# UI Bug Notes

## P1: Waveshaper has a doubled centre gutter

Context:

- The square plot and fixed control rail leave 52 pixels of unassigned width
  inside the expanded editor.
- This makes the visible plot-to-controls gap 76 pixels, compared with the IR
  modeller's correct 24-pixel spacing unit.

Acceptance:

- The plot remains square and at least 380 pixels wide.
- The control rail remains 336 pixels wide.
- The visible centre gap is 24 pixels, using the IR panel's 12-pixel inset and
  a container width that does not redistribute the unused space elsewhere.

Current status: fixed in `cycle-v2-waveshaper-centre-gutter.md` (2026-09-05).

## P1: Delay slider landmarks do not align with stops

Context:

- Delay Time's beat ticks and Pan Cycle's integer ticks are painted from the
  component bounds while JUCE maps values through a separately inset slider
  region.
- Endpoint ticks consequently appear detached beyond the visible track, and
  interior ticks miss the corresponding indicator positions by several pixels.

Acceptance:

- Ticks, snapped stops, fill boundaries, and indicators share one authoritative
  value-to-pixel mapping.
- Time's 2-beat tick and all twelve Pan Cycle ticks align at production size.
- No landmark is painted outside the visible track.

Current status: fixed in `property-slider-landmark-alignment.md` (2026-09-05).

## P0: IR High Pass leaves a DC component

Context:

- The post-high-pass impulse retains a vertical/DC offset that becomes more
  obvious when Post Gain is raised.
- Real FFT DC is stored outside the ordinary magnitude array currently
  multiplied by the IR prefilter levels.

Acceptance:

- High Pass at exactly 0 Hz remains an identity.
- Every value above 0 Hz removes DC from both the audible kernel and OpenGL
  diagnostic trace without suppressing the first non-DC bin prematurely.
- Post Gain scales the zero-mean result and cannot reintroduce DC.

Current status: fixed in `ir-prefilter-dc-removal.md` (2026-09-05).

## P1: IR expanded editor needs more curve width

Context:

- The IR expanded editor should be 20 percent wider.
- The complete increase belongs to the editable curve region; the property
  rail and its slider tracks must not grow.

Acceptance:

- Preferred width grows from 900 to 1080 pixels while height remains 430.
- At the preferred size, the property rail retains its current 348-pixel
  allocation and the IR panel gains the full 180 pixels.
- Compact/clamped placement remains on-screen.

Current status: fixed in `cycle-v2-ir-editor-width.md` (2026-09-05).

## 2026-08-31 UI intake: recommended order

Address correctness and containment before the broader editor rearrangement.
The first implementation train should be:

1. **Done — IR visual/audio identity and live high-pass response.** This combines
   the incorrect cutoff range/mapping, asynchronous visual response, missing
   post gain, and failure of the zero-cutoff sampled trace to overlay the
   editable curve. These symptoms share one preparation and invalidation
   boundary and should receive one focused TDD.
2. **Done — Trimesh link buttons toggle durably.** The missing canonical
   parameters and mature-panel settings bridge are restored, with pointer,
   keyboard, rebind, linked-vertex, and undo coverage.
3. **Implementation complete; OS proof pending — Phase-spectrum 2D background
   escapes its viewport.** Shared OpenGL scissoring now enforces both Trimesh
   host rectangles; macOS Screen Recording permission is required for the
   final production-pixel check.
4. **P1 quick correctness slice — IR ruler powers-of-two ticks and Guide phase
   band centering.** Both are bounded geometry defects with clear screenshot and
   automation assertions.

After those, implement the magnitude key-scale grid response and numeric-entry
behavior. Treat the Trimesh/Envelope control-region rearrangement, shared
enablement icon, and cross-editor control unification as one measured layout
TDD rather than independent local tweaks.

## P0: IR visualization does not match its audio or Cycle 1 behavior

Context:

- The useful High Pass range is compressed into too little of the current
  slider travel, suggesting either an over-broad control range or a mismatch
  with Cycle 1's spectral/log mapping.
- Spectrum and filtered-impulse visuals must update synchronously during a High
  Pass gesture. Other costly IR preparation should remain coalesced or deferred
  unless its associated control requires live visual feedback.
- The filtered visual trace omits Post gain.
- At 0 Hz, the sampled trace does not nearly overlay the authored IR curve, so
  the visual pipeline is not demonstrating the expected identity relationship.
- A production screenshot captured on 2026-09-04 shows an obvious additional
  inflection in the sampled/high-pass trace near the attack even though High
  Pass is 0 Hz. This is a shape/topology mismatch, not merely a small amplitude
  or antialiasing deviation.

Acceptance:

- Characterize Cycle 1's normalized High Pass mapping and spectral log mapping
  with shared numerical tests before altering the range.
- A multi-update High Pass drag changes spectrum and impulse visuals on every
  update without synchronously rebuilding unrelated state.
- The visual trace includes Post gain and shares convolution's prepared model,
  sampler, controls, filter, and domain. It retains Cycle 1's direct graphic
  sampling rather than displaying the ringing introduced by the distinct
  oversampled audio-kernel path.
- With High Pass at 0 Hz and unity Post gain, the sampled trace nearly overlays
  the editable curve within a documented rasterization tolerance, without
  introducing extrema or inflection points absent from the editable curve.

Current status: implemented 2026-09-05 in
`cycle-v2-ir-display-identity.md`. Cycle 1 uses an oversampled/downsampled audio
kernel and a directly sampled graphic impulse from the same curve rasterizer.
Cycle V2 now preserves that split, filters both through the shared high-pass
implementation, uses the graphic result for the OpenGL trace/spectrum, and
keeps the audio result for convolution. Focused tests guard exact zero-cutoff
identity, audio/display ownership, tight curve agreement, and topology. The
production 0 Hz/0 dB capture no longer shows the audio downsampler's ringing as
a second visual inflection.

## P0: Trimesh editor link buttons do not toggle

Context:

- Axis link buttons in the expanded Trimesh editor cannot be toggled on.
- This may be event routing, stale presentation state, or missing semantic
  command publication; appearance alone is not sufficient evidence.

Acceptance:

- Pointer and keyboard activation visibly toggle each supported link.
- The complete gesture publishes through the domain command boundary, survives
  rebind, and supports undo.
- Focused automation covers off-to-on and on-to-off sequences.

Current status: implemented 2026-09-01 in
`cycle-v2-trimesh-link-buttons.md`. The controls now publish canonical per-node
parameters through the command service, survive editor rebind, support undo,
and drive the mature `Interactor` link behavior without copying it.

## P0: Phase-spectrum 2D background is not clipped to its editor

Context:

- The phase-spectrum background from the 2D editor spills into the adjacent 3D
  editor content.
- The fix belongs at the authoritative OpenGL viewport/scissor or panel-host
  clip boundary, not in an overpaint.

Acceptance:

- The background, grid, and overlays remain inside the 2D panel at production
  size, Retina scale, and after resize.
- 3D content remains unchanged and no JUCE paint mask is introduced.

Current status: verified 2026-09-05 in
`cycle-v2-trimesh-panel-clipping.md`. Curve and Trimesh share the mature scissor
conversion and GL-state restoration, with 1x/2x numerical coverage and no JUCE
paint mask. A 2x production capture confirms the lower 2D background, grid, and
waveform stop at the host boundary without entering the 3D panel or controls.

## P1: Magnitude-spectrum log grid ignores the key-scale morph axis

Context:

- In the magnitude-spectrum Trimesh 2D editor, the log-spaced grid does not
  follow the modulation input axis assigned to `key scale`.
- The relevant axis position should come from the current preview MIDI note and
  the same normalized morph-position mapping used by the key-scale dimension.
- The exact keyboard range and endpoint convention need to be recovered from
  the authoritative preview-pitch implementation rather than assumed to be
  `[0, 88]`.

Acceptance:

- Changing preview MIDI note moves the log grid along whichever morph axis is
  assigned to key scale.
- Reassigning key scale changes the affected axis without duplicating pitch
  mapping logic.
- Tests cover at least two notes, two axis assignments, and an endpoint.

Current status: verified 2026-09-05 in
`cycle-v2-spectral-key-scale-grid.md`. The production fixture confirms an
inclusive MIDI span of `20` through `127`, and a 2x OpenGL capture shows the
spectral mapping changing continuously across the Red key-scale columns.

## P1: IR sample ruler chooses irregular intervals and omits its endpoint

Context:

- A 256-sample window currently labels `0, 29, 58, 86, ...` instead of the
  expected power-of-two rhythm `0, 32, 64, ...`.
- The final tick is absent. The ruler is distributing a fixed number of labels
  rather than selecting a musically and numerically meaningful interval.

Acceptance:

- Choose a power-of-two sample interval appropriate to the visible range and
  label density.
- A 256-sample unzoomed view shows `0, 32, 64, ... 256`, including both
  endpoints, without label clipping.
- Zoomed ticks remain aligned with the OpenGL grid and padded impulse origin.

Current status: implemented 2026-09-01 in
`cycle-v2-ir-power-of-two-ruler.md`. IR now declares one-sided domain padding,
so its OpenGL major grid divides the sample region into eight exact intervals;
the ruler includes domain `1` and displays `0, 32, ... 256` without clipping.

## P1: Guide phase-offset band is not centered

Context:

- The phase-offset band is visibly displaced from the center of the Guide curve
  panel.

Acceptance:

- Its zero/neutral center aligns with the panel's transformed 0.5 amplitude
  line at default and zoomed views.
- The band remains centered after resize and does not change the phase-offset
  value mapping.

Current status: closed as not reproduced 2026-09-05. At phase `0.5`, the
production panel reports width `740`, zoom x `0.025` with width `0.95`, and
symmetric phase-band domain endpoints `0.275` and `0.725`; both transform around
panel x `370`. A 2x production capture confirms symmetric horizontal and
vertical margins inside the OpenGL panel. No corrective offset was added.

## P1: Numeric property entry changes geometry and includes units

Context:

- Unfocused value-entry fields show an unnecessary bounding rectangle.
- Entering edit mode changes justification from centered/right-aligned display
  text to top-left text, causing a visible jump.
- Units are included in editable content instead of remaining stable,
  non-editable context.

Acceptance:

- Unfocused fields have no visible input rectangle; focus has a restrained,
  consistent focus treatment.
- Display and edit text retain the same baseline and horizontal justification.
- Editing selects only the numeric value. Units remain outside the editor and
  parsing/formatting round-trips without precision loss.

Current status: verified 2026-09-05 in
`cycle-v2-property-value-editing.md`. Production Retina captures confirm the
resting readout has no field rectangle; focus outlines only the numeric value,
preserves its right-aligned baseline, and leaves the unit outside the editor.

## P1: Envelope vertex Guide selector has misleading help text

Context:

- The Envelope vertex-properties Guide dropdown says, `guide attachments are
  available on mesh nodes`.
- This describes an internal graph category rather than the action available in
  the current Envelope editor and reads as though the control is unavailable.

Acceptance:

- Replace the copy with concise, action-oriented guidance for assigning a Guide
  curve to the selected Envelope property, or hide/disable the control with a
  truthful reason if Envelope assignment is unsupported.
- Do not expose implementation terms such as `mesh nodes` in local help.

Current status: implemented 2026-09-02. Envelope vertex Guide assignment is
not supported by the graph command boundary (only Trimesh cube fields are valid
targets), so the dead Guide affordances and internal-language popup were
removed. The vertex rails reclaim that width; Trimesh Guide controls are
unchanged.

## P1: Trimesh and Envelope property controls need a shared layout correction

Context:

- Guide-curve assignment uses an unclear icon and sliders with inadequate
  vertical acquisition/travel.
- Morph sliders consume excessive horizontal space.
- A likely hierarchy is vertex properties on the right of the control region,
  with cube display stacked above morph sliders and their buttons, but this must
  be validated as a complete space budget at production size.
- Envelope Axis and Link button groups are missing their labels.

Acceptance:

- Write one layout TDD covering both expanded editors, with measured section
  bounds, minimum slider hit/drag geometry, and resize behavior.
- Give Axis and Link groups top-centered spanning labels.
- Replace the Guide icon only through the shared semantic SVG icon system.
- Preserve vertex selection and all complete edit gestures through rearrangement.

Current status: implemented 2026-09-04 under
`docs/TDD/cycle-v2-shared-morph-vertex-layout.md`. Trimesh now stacks its cube
over full-span morph rails beside a full-height right vertex column. Guide
dropdowns are separate full-row-height controls, and Envelope's two morph
sliders reuse the same precise rail and line-marker painter. Geometry, hosted
interaction, Link undo, focused automation, and production screenshot review
pass.

## P1: Morph slider handle has contradictory geometry

Context:

- The current hollow circular handle with a dark line conflicts with the linear
  slider language and obscures the exact value.

Acceptance:

- Remove the circle and use a thin vertical indicator in the slider's semantic
  colour.
- Keep a larger invisible hit target, an unambiguous exact reference point, and
  existing fine-adjust/keyboard behavior.

Current status: implemented 2026-09-02 under
`docs/TDD/cycle-v2-shared-morph-vertex-layout.md`. The hollow circle and dark
overdraw are replaced by a 1.5 x 17 px semantic-colour value line while the
31 px-tall interaction target remains unchanged.

## P1: Expanded effect enablement lacks a shared placement and symbol

Context:

- Textual Enabled controls appear in inconsistent locations and look detached
  from editor chrome.
- The preferred direction is a semantic SVG power/electricity toggle in the
  upper-right editor chrome beside Close, with background highlight indicating
  enabled state, based on Cycle 1's interaction language.

Acceptance:

- Use one shared placement, size, SVG, hit target, tooltip, and state treatment
  across expanded effect editors.
- Enabled state is not communicated by colour alone, and keyboard/focus states
  remain visible.
- Removing the local Enabled row returns its space to content or controls.

Current status: implemented 2026-09-02 under
`docs/TDD/cycle-v2-expanded-effect-controls.md`, using the SVG icon-system
workflow.

## P2: Canvas legend is undersized

Context:

- The canvas utility legend's content and text should be approximately 30
  percent larger. Confirm this is the intended legend before implementation.

Acceptance:

- Increase symbol and text scale together by 30 percent from the current
  production geometry while preserving baseline alignment and utility-dock
  containment at the compact window size.
- Do not reduce performance-keyboard or minimap minimums to make room.

Current status: implemented 2026-09-05 in
`cycle-v2-canvas-legend-scale.md`. Text, domain samples, strokes, spacing, and
surface height are scaled together by exactly 30 percent through centralized
metrics. Responsive painting is clipped to the dock's returned legend bounds,
while the compact minimap and performance keyboard retain their dimensions.

## P2: IR editor needs an attack-zoom action

Context:

- The expanded IR editor has no one-step action to frame the attack/edit region.
- Cycle 1 has an existing visual reference; Cycle V2 should use a semantic SVG
  rather than copying a raster or using text.

Acceptance:

- The action frames the padded onset and useful early response, is reversible
  through the existing zoom reset, and has a clear tooltip and keyboard focus.
- The SVG belongs to the shared icon system and remains legible at production
  size.

Current status: implemented 2026-09-05 in `cycle-v2-ir-attack-zoom.md`. Shared
SVG attack/full controls now float inside the plot's upper-right corner. The
attack action uses Cycle v1's padded origin and 20-percent span, full view
restores the declared domain, and the zoomed ruler omits off-screen ticks.

## P2: Effect property controls are not unified

Context:

- IR slider/control geometry is inconsistent with Delay and Waveshaper.
- Waveshaper controls are poorly distributed, and the Antialiasing dropdown is
  much wider than its content requires.

Acceptance:

- Establish one expanded-effect property grid and control-size family, allowing
  domain-specific width only when value precision or content requires it.
- Size Antialiasing from its longest real option plus standard insets.
- Compare IR, Delay, Waveshaper, and Reverb together at the same production size.

Current status: implemented 2026-09-02 under
`docs/TDD/cycle-v2-expanded-effect-controls.md`.

## P1: Property slider density and Waveshaper space are imbalanced

Context:

- IR compact labels sit visibly too far above their tracks.
- Ordinary property sliders retain a hollow capsule even though the newer
  morph-slider hairline is clearer and more precise.
- The Waveshaper property stack is pinned to the top of a tall control rail,
  leaving an unexplained lower void, while the square transfer view remains
  smaller than the surrounding editor can usefully support.

Acceptance:

- Use the shared minimal hairline indication without shrinking interaction
  targets, and keep compact labels within 10 px of their visible tracks.
- Grow the preferred square Waveshaper view by approximately 20% and centre the
  complete property group vertically in its available rail.
- Add a general residual-space audit rule to the UI-design skill.

Current status: implemented 2026-09-05 under
`docs/TDD/cycle-v2-slider-density-and-space-balance.md`. Production IR and
Waveshaper captures confirm the tighter row density, minimal exact indicators,
larger square transfer view, and vertically balanced property group.

## Open: Node-palette fallback icon asserts during the complete Cycle V2 suite

Context:

- The complete Cycle V2 suite on 2026-08-28 logged a JUCE assertion at
  `NodePaletteEntryIconRenderer.cpp:27` while continuing to completion.
- The corner-metrics change only alters radius arguments and does not change
  palette entry kinds, icon dispatch, or fixture state.
- The assertion is separate from the suite's one failing hover-help test; 533
  of 534 cases otherwise pass.

Current status: open; identify which test supplies an unsupported palette entry
kind and decide whether the fallback should be a supported icon or a non-
asserting generic symbol.

## Open: Intermittent CoreMIDI endpoint assertion during Cycle V2 automation

Context:

- A sequential property-control fixture run on 2026-08-27 completed every
  Reverb command successfully but logged CoreMIDI error `580` and three JUCE
  assertions at `juce_CoreMidi_mac.mm:595` during application startup.
- The five adjacent Cycle V2 fixture launches did not report the assertion,
  and the issue is independent of property value formatting or slider paint.
- Repro artifacts: `/private/tmp/cycle-v2-reverb-property-controls-precision-logs.txt`
  and its `.raw` companion.

Current status: open; inspect MIDI endpoint initialization and teardown across
rapid sequential standalone launches.

## Open: Node canvas hit-router test misses edge hover help

Context:

- The complete Cycle V2 suite on 2026-08-26 reported one unrelated failure in
  `TestNodeCanvasHitRouter.cpp:66`: `edgeHelp.startsWith("Time signal from")`.
- The focused test reproduces consistently and also logs a JUCE assertion at
  `juce_String.cpp:327`.
- The Guide property-control change does not touch hit routing; its focused
  tests and automation remain green.
- JUnit evidence: `/private/tmp/cycle-v2-tests-junit.xml`.

Current status: open; inspect why the scene edge midpoint resolves to empty or
non-edge hover text before changing the product assertion.

## Open: Cycle V2 agent wrapper falls back to a non-GUI launch and aborts

Context:

- A focused hover-help fixture on 2026-08-26 failed to open the built app via
  LaunchServices with `kLSNoExecutableErr`, despite the bundle executable being
  present and executable.
- The wrapper then launched the executable directly; AppKit aborted while
  registering the application on the main thread.
- Repro artifacts: `/private/tmp/cycle-v2-hover-help-logs.txt` and
  `/private/tmp/cycle-v2-hover-help-logs.txt.ips`.

Current status: open; investigate the LaunchServices bundle resolution and do
not use direct executable fallback for GUI automation.

## Open: Envelope native smoke does not restore curve state on undo

Context:

- `python3 scripts/test_cycle_v2_native_edit_smoke.py envelope` now completes
  both finite audio captures, then fails at `envelope_sequence` line 987.
- After the curve-sharpness gesture and undo, the exported Envelope model state
  does not exactly equal the pre-gesture state.
- The failure is downstream of and independent from the linked-stereo audio fix.

Current status: open; compare the restored model fields and determine whether
the gesture commits an incomplete undo snapshot or the fixture includes
non-semantic state in its equality assertion.

## Open: Guide dock launch capture logs existing settings assertions

Context:

- The Guide/Spy dock capture on 2026-08-21 rendered successfully but logged two
  `juce_String.cpp:327` assertions followed by `Settings.cpp:225` and `:226`.
- Repro artifacts: `/tmp/cycle-v2-guide-dock.png` and
  `/tmp/cycle-v2-guide-dock.log`.
- The native OpenGL Guide-preview capture on 2026-08-22 reproduced the same
  settings-write assertions at shifted lines `Settings.cpp:227` and `:228`;
  all fixture assertions and rendering completed successfully. Repro log:
  `/private/tmp/cycle-v2-guide-opengl-final-logs.txt`.

Current status: open; inspect settings-map initialization separately from the
Guide resource UI work.

## Fixed: Canvas overlay faded while dragging nodes

Context:

- Cache-hit node sprites and the cached cable layer could be presented at
  greatly reduced opacity during a node drag, while the moving cache-miss node
  and canvas grid remained bright.
- Node-drag frames now use the authoritative node and cable renderers directly.
  Mouse-up restores the optimized sprite and composite caches for stable
  frames.
- In the focused native drag, direct cable rendering took 1.63 ms and direct
  node rendering took 3.68 ms; JUCE paints averaged 14.63 ms and peaked at
  24.74 ms.
- The native authoring sequence captures the cable and node presentation while
  the drag is still held.

Current status: fixed on 2026-08-29.

## Addressed: Curve preview resources created before OpenGL context

Context:

- During the presentation-cache lifecycle fix on 2026-08-31, synchronizing all
  curve widgets in `NodeCanvas` construction caused `EXC_BAD_ACCESS` in
  `Curve::recalculateCurve()` while the default Waveshaper rasterizer was being
  prepared without the established OpenGL render lifecycle.
- Repro crash: `~/Library/Logs/DiagnosticReports/CycleV2-2026-08-31-104535.ips`.

Current status: addressed in the same work by creating and synchronizing curve
preview resources at the existing OpenGL preview-render boundary. The
2026-09-05 merge retained that boundary and removed the older 30 Hz timer
synchronization, which otherwise invalidated every cached preview frame.
