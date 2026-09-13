# Cycle V2 UI Polish Batch

Status: Implemented — time-frequency redesign intentionally deferred

## Scope

This batch addresses local Cycle V2 presentation defects without changing graph
semantics or DSP behavior:

- align Voice Context slider tracks, landmark rows, and value columns;
- provide one reusable contiguous segmented selector whose outer ends are
  rounded and whose interior selection and hover endcaps are flat;
- use that selector for Voice Context domain/oversampling and Waveshaper
  oversampling, while applying the same geometry to Envelope mode hover;
- render Reverb runtime spectrograms with the runtime result's spectral domain
  profile so their palette cannot alternate with the node's time-domain profile;
- expose Reverb Size as the seven effective power-of-two kernel stops;
- replace the Envelope loop action glyph with a compact, rounded return-arrow
  silhouette;
- scale the Output meter's visible gain-fader geometry with canvas zoom while
  keeping its existing semantic gesture and undo path.

## Design Contract

- Segmented controls have one 28-30 px production control box, one continuous
  outer boundary, no gaps, and equal-width hit targets. Only the first segment
  rounds its left corners and only the last rounds its right corners. Selection
  and hover use the same endcap mask.
- Voice Context uses one 88 px label column, one shared slider track column,
  and one 72 px value column. All three sliders use the existing shared
  `PrecisionSlider` mapping and landmark painter.
- Reverb spectrogram data remains the authoritative prepared runtime preview;
  the UI changes only the profile used to map that spectral-magnitude result.
- Reverb Size snaps through the shared DSP mapping so its seven displayed
  durations are exactly the seven kernel lengths the processor can allocate.
- Output gain remains a relative-drag semantic edit. Zoom changes presentation
  geometry only and must keep visual and interaction tracks aligned.

## Authoritative Implementations And Boundaries

- `PropertyControls` owns shared property-control colour, corner, and spacing
  vocabulary. The new selector is a presentation/input primitive, not a graph
  command adapter.
- `CurveExpandedEditorComponent` continues to own discrete publication for
  Waveshaper. `NodeEditorCommands` continues to own Voice Context edits.
- `NodePreviewRenderer` and `TrimeshRenderProfile` remain authoritative for
  cached runtime heatmap rasterization.
- `EffectParameterMapping` remains authoritative for Reverb kernel length and
  Output gain mapping.
- `NodeCanvasAuthoring` remains authoritative for Output gain transactions.

## Complexity

All changes are O(1) per paint or edit. No graph/model copies, serialization,
resource preparation, or additional analysis are introduced.

## Follow-up Disposition

- Guide Curve shelf snapshots pass the focused preset-replacement fixture; a
  production OpenGL capture confirms the replacement preset's guide is
  populated, so no additional replacement renderer was added.
- Spectral Phase compact and expanded views already share
  `TrimeshRenderProfile` and its bipolar orange/purple scale. Focused mapping
  tests and a production OpenGL capture confirm parity, so no second mapping
  path was introduced.
- Time-frequency node proportions and iconography remain deferred by explicit
  product direction.

## Completion Criteria

- Geometry and semantic tests cover segmented endcaps, aligned Voice Context
  columns, discrete Reverb Size movement, stable Reverb palette selection,
  and proportional Output gain visuals.
- Cycle V2 builds with `--parallel 10`, relevant tests pass, SVG parses, the
  production UI is captured at native size, and `git diff --check` is clean.
- The completed local batch is committed; the intentionally skipped
  time-frequency redesign remains explicit above.

## Verification

- Shared segmented geometry, Voice Context layout/commands, Waveshaper
  publication, discrete Reverb Size, Reverb spectral palette, and zoomed
  Output meter geometry have focused Catch regressions.
- `cycle-v2-agent-ui-polish.json` verifies the aligned Voice Context tracks and
  a real nested segmented-option edit through Cycle V2 automation.
- Native-size captures were reviewed for Voice Context, Reverb, and the
  Envelope purpose/marker toolbar. The revised loop glyph was refined after the
  toolbar-size pass to keep its rounded stroke inside the 48 px source canvas.
- The standalone target and focused test target build with `--parallel 10`.
  The relevant focused tests pass. A broad randomized suite run retained
  unrelated graph/Trimesh failures recorded in `ui-bugs.md`.

## Follow-up Correction Slice

The production review identified four corrections to the first batch:

- Voice Context keeps its 440 px width, grows vertically to 300 px, uses a
  10 px row rhythm, centres landmark labels on a 15 px inset track, and packs
  readout text from the left edge of the value column.
- Segmented-control dividers span the full pill height. Selection is conveyed
  by the filled segment alone; legacy Processing Scope and Unison mode
  underlines are removed. Spectral Trimesh exposes its existing `spectralMode`
  parameter through the shared Auto/Add/Multiply selector in the editor header.
- Reverb retains `ReverbSpectrogramPreviewProcessor` as the authoritative data
  source. Its surface uses the mature logarithmic frequency and brightness
  transform, while the runtime spectral render profile remains authoritative
  for the inferno palette. During a transient
  preview gap, the renderer retains the last authoritative heatmap instead of
  switching to the qualitative cyan reflection bars.
- Output meter mapping and hit geometry remain unchanged; only the visible
  thumb width and height increase by 50 percent.

The Trimesh selector is a discrete `GraphCommandDispatcher` edit through the
existing editor command service. It adds no graph copies or live gesture path.
All correction work remains O(1) except the unchanged local Reverb heatmap
raster product.

The focused Catch regressions and Voice Context, Reverb, Waveshaper, and
Trimesh automation fixtures pass. Production-size review confirms the centred
Voice Context landmarks and restored Reverb spectrogram. The Trimesh fixture
publishes `multiplicative` through the shared selector; the Waveshaper fixture
confirms the Context and Antialiasing rows share label and selector axes.

The Reverb fixture now opens the connected runtime Reverb in `with-spies`
rather than creating an uncompiled orphan node, and captures the canvas rather
than the unreliable macOS window surface. Its runtime stats report a
384-by-1025 `Reverb Spectrogram` magnitude grid, and the native capture confirms
that the compact and expanded views retain the same inferno heatmap after Size,
Width, and Wet edits.

## Reopened Live-preview Contract

- `ReverbSpectrogramPreviewProcessor` remains the sole source of Reverb visual
  content. Slider edits must never substitute a qualitative reflection diagram
  or another fallback for its kernel-derived magnitude grid.
- A live slider movement records one transient semantic edit, asynchronously
  refreshes the affected runtime traversal, publishes a new preview content
  revision, and explicitly repaints the already-bound editor component. The
  editor binding itself remains stable during the gesture so control state is
  not reset by asynchronous completion.
- Regression coverage uses a connected Reverb, performs two movement updates,
  and checks the `Reverb Spectrogram` role, magnitude domain, grid dimensions,
  and content fingerprint before commit and after undo.
- Spectral Phase compact and expanded Trimesh rendering must both receive the
  same `GraphRenderSemanticResolver` profile. Domain-only fallback profiles are
  not authoritative where graph context establishes a more specific scale or
  role.
- The Guide Curve preset-reset fixture is rerun as lifecycle verification; no
  replacement implementation is introduced unless that focused reproduction
  fails.

### Follow-up Verification

- The connected native Reverb fixture now advances the real UI message loop,
  observes two distinct in-gesture kernel-spectrogram fingerprints, retains the
  `Reverb Spectrogram` magnitude grid throughout, and returns to the original
  fingerprint on undo. Both expanded screenshots retain the mature spectral
  heatmap; no qualitative fallback was added.
- Spectral Phase already shares `TrimeshRenderProfile` between compact and
  expanded rendering. Focused compact/expanded pitch-region tests pass, and the
  native OpenGL capture confirms the phase surface and bipolar trace use the
  same orange-centre-purple profile. No duplicate scale adapter was needed.
- The preset-reset fixture passes with a native capture of `guitar-4-g`; its
  Guide Curve tile is populated after switching from `sat-bass`, confirming the
  existing document-presentation reset remains effective.
- FFT/IFFT/time-frequency proportions and iconography remain outside this
  batch by explicit direction.

### Reopened Probe-policy Regression

The first live-preview correction proved eventual publication only after the
fixture switched the Spy rail to `Live`. That missed the product contract for
the default `On Release` mode: local editor products remain live while only
downstream traversal and probes are deferred. Local node edits now request the
existing `CompactPreview` product for the edited node, while commit requests
retain `PreviewTraversal` and `ProbePreview`. The cached kernel spectrogram
remains authoritative, and the local movement path stays O(1) in unrelated
graph and probe count.

The native `reverb-preview` interaction holds the real mouse gesture open and
asserts distinct runtime content fingerprints at two successive Wet positions
under `On Release`. The focused app fixture's production-size captures retain
the expanded and compact kernel-derived spectrogram presentation at both
positions.

### Reopened Reverb Size And Update-cost Correction

The continuous Size control was a misleading regression: its display changed
between the seven power-of-two kernel lengths even though those positions did
not expose another tail duration. Size now uses the shared
`EffectParameterMapping` quantizer for pointer and keyboard interaction.

Cycle 1 is authoritative for Reverb update boundaries. Size, Damping, and High
Pass replace the immutable kernel resource; Wet, Width, and Enabled reuse it.
Cycle 2 carries the previous published configuration into the configuration
factory, shares an unchanged kernel by identity, and invalidates prepared
convolvers only when that identity changes. This keeps a mix-only movement
O(1) in kernel length and proves reuse without timing-sensitive assertions.

The native baseline for held Wet publications measured mean worker time of
12.71 ms: 2.57 ms configuration, 0.10 ms preview audio, and 7.47 ms preview
extraction. Kernel reuse reduced configuration to 0.045 ms and worker time to
9.99 ms; the truthful spectrogram extraction remained 7.49 ms. Canvas paint
and asynchronous publication delay are measured separately and must not be
misattributed to Reverb impulse construction.
