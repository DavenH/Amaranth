# Cycle 2 Effect UI Refinement

## Status

Implemented.

## Context

The initial Reverb, Delay, and Equalizer editors are implemented under
`cycle-v2-effect-control-popups.md`. Follow-up work established a more precise
interaction and visualization standard on Delay. This TDD applies that standard
consistently to all effect editors without reopening completed DSP extraction
or graph-hosting architecture.

An effect display is an explanation of configured production DSP, not a
decorative animation. Controls must expose the parameter's real range,
quantization, mapping, unit, default, and causal role.

## Authoritative Implementations

- Reverb kernel, filtering, wet, damping, and stereo semantics:
  `lib/src/Audio/Effects/ConvReverb.*`, the shared Reverb kernel generation
  path, and `cycle/src/Audio/Effects/Reverb.cpp`.
- Delay timing, feedback, wet, and stereo pan-cycle semantics:
  `lib/src/Audio/CycleDsp/CycleDelay.*`.
- Equalizer parameter mapping and response:
  `lib/src/Audio/CycleDsp/EqualizerCore.*` and
  `cycle/src/Audio/Effects/Equalizer.cpp`.
- Shared effect parameter mappings:
  `lib/src/Audio/CycleDsp/EffectParameterMapping.*`.
- Effect editor hosting and publication:
  `cycle-v2/src/UI/NodeEditorHost.*` and
  `cycle-v2/src/UI/ConcreteNodeEditors.cpp`.
- Compact effect presentation:
  `cycle-v2/src/Nodes/Effects/EffectPreviewRenderer.*`.
- Canvas palette baseline: `kCanvasBackground`, `kCanvasGridMajor`, and
  `kCanvasGridMinor` in `cycle-v2/src/UI/NodeCanvasPresentation.cpp`, mirrored
  by the OpenGL background constants in `NodeCanvasGlRenderer.cpp`.

Existing DSP algorithms and normalized graph parameter IDs remain unchanged
unless a criterion below explicitly calls for an authoritative shared mapping
change. UI code may translate parameter values to positions, labels, ticks,
and readouts; it must not reproduce DSP transfer functions.

## Shared Interaction Contract

- Audit every parameter for range, unit, default, mapping, quantization, and
  audible role before changing its control.
- Continuous parameters remain continuous. Optional musical or perceptual
  landmarks snap only within a small pixel-distance zone.
- Discrete parameters show every meaningful stop and snap strictly to those
  stops for pointer, keyboard, wheel, reset, and programmatic slider changes.
- Nonlinear controls position ticks and evaluate snap proximity after the
  authoritative inverse mapping. Raw normalized-value proximity is forbidden.
- Readouts contain only compact values such as `6×`, `+3.2 dB`, `850 Hz`, or
  `0.37 s`. Bespoke semantics belong in concise labels and tooltips.
- User-facing names may improve while serialized parameter IDs remain stable.
- Defaults must preserve an intentional audible state after any mapping change.

## Shared Visualization Contract

- Visual data derives from the same prepared DSP configuration or shared
  analysis core as audio. Do not add hand-authored approximations when the
  mature implementation is available.
- Each parameter with an audible effect must produce an honest, testable visual
  consequence. Do not add motion, markers, or attenuation unsupported by DSP.
- Stable visual references must preserve comparisons between parameter states.
  Per-row, per-bin, or per-frame normalization must not conceal relative
  attenuation, wet level, feedback, decay, or other requested relationships.
- Grids and axes paint beneath data. Related axes and data share exact
  floating-point geometry. Plot margins remain visually symmetric.
- Bypassed displays retain the configured response in greyscale rather than
  erasing it or substituting an unrelated flat response.
- Compact and expanded displays communicate the same semantics at different
  information densities.

## Palette Contract

- Effect plots belong to the canvas palette family: near-black blue-charcoal
  grounds based on the canvas `#101318`, with restrained blue-grey grid and
  axis lines.
- Do not use a saturated green or “matrix” field as a generic DSP background.
- Cyan and amber may identify meaningful response energy or active data, but
  large background areas remain neutral and visually subordinate to the graph.
- Alpha gradients must composite cleanly over the canvas-family ground. Avoid
  translucent masks that create muddy blocks, unexpected opacity plateaus, or
  a different apparent background colour.
- Disabled presentation removes saturation from the complete plot—including
  background tint, grid, axes, labels, and data—while retaining useful contrast.
- Palette constants should be shared presentation tokens rather than repeated
  effect-local literals once the refinement touches more than one renderer.

## Reverb Refinement

- Size displays seconds and visibly extends the response in time.
- Audit the current power-of-two kernel-length quantization. Either expose its
  real stops or provide a perceptually continuous control backed by an
  authoritative DSP mapping; do not present hidden coarse jumps as continuous.
- Damping visibly changes high-frequency decay without changing the meaning of
  High Pass.
- High Pass visibly attenuates lower spectral energy through the production
  kernel calculation. Do not add a display-only high-pass mask or cutoff marker.
- Wet changes represented response energy under a stable display reference.
- Width receives a truthful stereo visualization derived from the production
  stereo kernels. Candidate presentations are paired L/R energy, a stereo
  correlation view, or paired spectrograms; select one only after auditing the
  actual width implementation.
- Time and frequency axes use lightweight labels or landmarks sufficient to
  interpret the spectrogram. The frequency mapping must not allocate an
  outsized fraction of display height to the first partial.

## Equalizer Refinement

- The horizontal graph and frequency controls use the authoritative logarithmic
  frequency mapping.
- Add restrained, correctly positioned frequency landmarks such as 60, 120,
  250, and 500 Hz and 1, 2, 4, 8, and 16 kHz. They are reference ticks, not
  mandatory snap points.
- Frequency remains continuous. If proximity snapping is enabled, calculate it
  in slider pixels after nonlinear positioning.
- Gain controls have a strong exact 0 dB detent and visible centre mark.
- Add restrained 0 dB and useful positive/negative dB reference lines to the
  response graph, with 0 dB visually dominant.
- Identify each band consistently between its controls and its response feature
  without turning the plot into a multicolour legend.
- Continue using the shared filter response core at display resolution. Do not
  enlarge a compact trace or approximate the curve in UI code.
- Audit fixed-Q or bandwidth semantics. If Q is musically meaningful but not
  adjustable, communicate the fixed behavior; do not imply that the graph
  exposes a missing control.
- Disabled EQ retains its configured response in greyscale.

## Delay Reference Behavior

Delay is the established reference for these refinements:

- Time retains its authoritative nonlinear scaling, visible musical stops, and
  pixel-proximity snapping while remaining continuous elsewhere.
- Pan Amount and Pan Cycle use clear related terminology. Pan Cycle is a strict,
  evenly distributed integer control with concise multiplier readout.
- The first tap is Wet-scaled; Feedback begins with subsequent taps.
- Tap spacing, pan phase, decay, clipping, and radius derive from production
  mappings.
- The stereo centreline and beat grid paint beneath taps with symmetric margins
  and exact shared geometry.
- Disabled compact and expanded plots are greyscale.

## Architecture And Complexity

- High-level editor code coordinates layout, events, publication, and calls to
  domain renderers. DSP analysis, filter response, and parameter transfer logic
  stay in shared domain modules.
- Prefer a small shared effect-plot palette/state primitive over additional
  `NodeKind` branches or repeated colour literals in each editor.
- Cached Reverb analysis remains off audio and paint paths. Paint work may
  composite cached images but must not regenerate kernels or FFT grids.
- Ordinary parameter drags remain constant-time UI publication plus existing
  prepared preview invalidation. No audio-thread allocation, locking, logging,
  parsing, FFT analysis, or image processing is permitted.
- Expected production work is a focused refinement across shared mapping,
  effect presentation, and editor layout files. A large new compatibility
  adapter or copied DSP implementation requires redesign before proceeding.

## Negative Boundaries

- Do not add effect-specific behavior to `NodeCanvas`.
- Do not duplicate Reverb, Delay, or Equalizer DSP formulas in editor code.
- Do not normalize spectrogram rows or bins independently merely to increase
  contrast.
- Do not use post-processing to fabricate a DSP parameter's visual effect.
- Do not introduce hard snapping for continuous EQ frequencies or Reverb
  parameters without an explicit discrete DSP contract.
- Do not change serialized parameter IDs as part of naming or presentation work.
- Do not mark this TDD implemented while Reverb Width, Reverb Size
  quantization, or EQ completion criteria remain unresolved.

## Verification

- Mapping tests cover every discrete stop, nonlinear inverse mapping, defaults,
  endpoints, and snap-zone boundaries.
- DSP/analysis tests compare parameter states numerically before rasterization.
  Reverb tests independently cover Size, Damping, High Pass, Wet, and Width.
- Final-raster tests run through the same frequency-row mapping, magnitude
  mapping, palette, alpha compositing, and image path seen by the UI.
- Pixel-level tests prove enabled plots contain the intended accent colour and
  disabled Delay, Reverb, and EQ plots are fully greyscale.
- EQ tests compare the displayed response against the shared filter core and
  verify exact 0 dB detents and logarithmic landmark positions.
- Focused automation fixtures cover open, edit, drag, snap, reset, enable,
  disable, save, reopen, and live preview publication for each effect.
- Before/after screenshots are visual evidence only when the capture is valid
  and the claimed difference is actually observable. Numerical and pixel-level
  assertions remain authoritative for subtle DSP relationships.
- Standalone Debug and focused test targets build successfully; filtered launch
  logs contain no assertion, crash, or suspicious runtime failure.

## Completion Criteria

- Every Reverb, Delay, and EQ parameter satisfies the shared interaction and
  visualization contracts.
- Reverb Width has an implemented, DSP-derived stereo representation.
- Reverb Size quantization is either honestly exposed or replaced by an
  approved authoritative continuous mapping.
- EQ has logarithmic landmarks, a 0 dB detent/reference, truthful band identity,
  and disabled greyscale presentation.
- All three effects use the canvas-family palette without saturated generic
  green backgrounds or faulty alpha masking.
- Shared presentation tokens replace repeated effect-local palette literals
  where appropriate.
- Focused numerical, pixel, interaction, persistence, and crash-regression
  evidence passes.

## Implementation Evidence

- `c6158175` introduced shared canvas-family effect plot tokens and consistent
  enabled/bypassed rendering for compact and expanded displays.
- `0a6609bd` exposed every production Reverb Size kernel stop and derived the
  paired L/R Width display from the production stereo mixing semantics.
- `1b86af1b` added the EQ 0 dB pixel detent, logarithmic frequency landmarks,
  response references, numbered band identity, and fixed-bandwidth guidance.
- The final integration pass moved the remaining tick colours to shared tokens,
  added lightweight Reverb time/frequency labels, and removed temporary kernel
  diagnostics from runtime and test output.

The implementation adds no effect branch to `NodeCanvas`, no compatibility
adapter, and no display-only DSP transfer. Reverb analysis remains in the
preview processor, EQ response calculation remains in the domain renderer, and
editor code coordinates controls and paint calls.

Verification on macOS arm64:

- `AmaranthLib_tests '[mapping]'`: 34 assertions passed.
- `CycleV2_tests '[effects][preview]'`: 41 assertions across seven cases passed.
- Standalone `CycleV2` and focused `CycleV2_tests` targets built successfully.
- Reverb, Delay, Equalizer, and disabled-effect automation fixtures completed
  all 53 commands, including editor publication and persistence checks, with no
  failed command, assertion, or crash in their filtered final logs.
- The full CTest run completed 509 tests: 506 passed. The three failures are
  unrelated existing architecture/performance expectations
  (`GuideCurveOffsetSeeds`, rich-node view width, and preview alias counting);
  all effect mapping, DSP, preview, palette, bypass, and editor tests passed.
- The aggregate tests build remains blocked only while linking the unrelated
  Oscillo application target, whose bundle currently has no `_main` symbol;
  the test executables used above build independently.

Automation screenshots were produced by the fixtures but are not cited as
visual proof because macOS focus/capture reliability is environment-dependent.
Numerical response assertions, final-raster pixel tests, runtime state
assertions, and clean filtered logs are the authoritative evidence.
