# Cycle V2 UI Polish Batch

Status: Partial — easy batch complete; staged follow-ups open

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
- remove explicit UI snapping from Reverb Size while retaining the mature
  power-of-two kernel mapping in DSP;
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
- Reverb Size remains a continuous normalized parameter. Kernel allocation can
  still quantize to the mature power-of-two sizes below the UI boundary.
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

## Deferred Stages

- Reproduce and repair blank Guide Curve shelf snapshots across preset changes;
  this crosses the document/OpenGL snapshot lifecycle and needs a focused
  fixture before mutation.
- Reconcile Spectral Phase compact and expanded colour/scale semantics using a
  shared source-to-display contract.
- Redesign time-frequency node proportions and iconography as one production-
  size node family, including FFT, IFFT, and spectral views.

## Completion Criteria

- Geometry and semantic tests cover segmented endcaps, aligned Voice Context
  columns, continuous Reverb Size movement, stable Reverb palette selection,
  and proportional Output gain visuals.
- Cycle V2 builds with `--parallel 10`, relevant tests pass, SVG parses, the
  production UI is captured at native size, and `git diff --check` is clean.
- The completed local batch is committed; deferred stages remain explicitly
  open above.

## Verification

- Shared segmented geometry, Voice Context layout/commands, Waveshaper
  publication, continuous Reverb Size, Reverb spectral palette, and zoomed
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
  source. Its already-normalized surface uses the existing spectral grid mapper
  without the generic magnitude tension curve, while the spectral render
  profile remains authoritative for the inferno palette. During a transient
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
