# UI Bug Notes

## Open: Stengah Waveshaper editor restore crashes on a stale vertex selection

Context:

- Opening the Waveshaper editor from the locally edited Stengah preset crashes
  while restoring its saved flat-curve selection.
- The stack reaches `Interactor::getModPosition(bool)` through
  `FlatCurvePanelBase::restoreFlatSelection`; the saved selected vertex no
  longer resolves to a valid interactive vertex.
- Repro artifact: `/private/tmp/cycle-v2-stengah-seven-probes-logs.txt.ips`.
- This is incidental to the spy-routing defect; the focused spy fixture can edit
  the Waveshaper parameter without opening its editor.

Current status: open; validate saved selection IDs before restoring selection
frames and add an editor-open regression for stale model selections.

## Addressed: Voice Context sliders only responded to pointer-down

- Octave and Pitch previously applied only the initial pointer-down value;
  pointer drag was not routed back to those controls.
- Voice Length discarded every cached preview sprite for each movement, making
  continuous dragging visibly laggy.
- The focused Voice Context fixture now drags Octave, Pitch, Voice Length, and
  Oversampling through complete down-drag-up sequences and asserts their
  resulting graph or preview state.

## Open: Trimesh rasterization ignores attached guide curves

Context:

- Baroque Flute and Stengah preserve their Cycle 1 cube-component guide
  assignments as `guide.cube.<index>.<field>` attachment edges.
- The Trimesh editor and preview currently rasterize the base mesh without the
  attached guide curve. The guide waveform and its noise, DC-offset, and phase
  controls therefore produce no visible contribution on assigned vertices.
- Repro: open either preset, inspect a Trimesh node with a guide attachment,
  then change the attached Guide Curve's noise control. The Trimesh rendering
  remains unchanged.

Current status: open; route the existing guide provider/snapshot contract into
Trimesh rasterization and add a focused visual fixture that proves the assigned
cube component changes without copying Cycle 1 rasterization logic.

## Open: GuideCurveOffsetSeeds vertical seed contract fails

Context:

- Repository-wide CTest during Modulation Triple work on 2026-07-23 passed
  550 of 551 tests.
- `GuideCurveOffsetSeeds owns paired phase and vertical seed arrays` failed at
  `lib/tests/TestRasterizerTypes.cpp:31`: `verticalAt(2)` returned 16 instead of
  4.
- A focused rerun failed identically. The modulation work does not modify
  rasterizer seed storage or guide-curve rasterization.

Current status: open; reconcile the paired seed test contract with the current
`GuideCurveOffsetSeeds` representation.

## Open: Cycle v2 automation launch emits Settings assertions

Context:

- The focused compact Modulation and Modulation Triple fixtures on 2026-07-23
  completed every command successfully but logged JUCE assertions at
  `Settings.cpp:222` and `Settings.cpp:223`.
- The assertions occur during standalone launch and are incidental to the
  modulation graph, editor, bundle, and persistence behavior.
- The Voice Context duration fixture still reproduces the assertions at the
  shifted source locations `Settings.cpp:223` and `Settings.cpp:224`; all
  fixture commands and state assertions complete successfully.
- Repro logs:
  `/private/tmp/cycle-v2-modulation-source-compact-logs.txt` and
  `/private/tmp/cycle-v2-modulation-triple-logs.txt`, plus
  `/private/tmp/cycle-v2-voice-context-length-logs.txt`.

Current status: open; inspect the settings property access performed during
standalone initialization.

## Open: Cycle v2 full suite emits JUCE Component assertions

Context:

- A full `CycleV2_tests` run during Modulation source parity work on 2026-07-22
  repeatedly emitted `JUCE Assertion failure in juce_Component.cpp:1607` while
  otherwise completing 347 test cases.
- The focused Modulation suite and standalone automation fixture do not emit
  the assertion, so it is incidental to the control-source implementation.

Current status: open; isolate the broad UI test that adds an already-parented
component and capture its exact call site.

## Addressed: Cycle v2 automation screenshots appended to existing PNG files

Context:

- Reusing an existing `path` with the Cycle v2 `screenshot` automation command appends another PNG payload instead of replacing the prior image.
- Image viewers consequently decode the oldest frame, which can make visual-regression review appear stale even though the report says the screenshot succeeded.
- Reproduced on 2026-07-22 with `scripts/fixtures/cycle-v2-agent-screenshot.json` and `/private/tmp/cycle-v2-agent-canvas.png`; moving the old output away before capture produced the current frame.

Current status: addressed by deleting the exact destination before opening the
new screenshot stream. Offline and live WAV automation artifacts now use the
same replacement behavior.

## Open: Trilinear mesh 2D rasterization changes when primary view axis changes

Context:

- In the Cycle v2 Trilinear Mesh popup, changing the primary view axis should only change which morph dimension is swept by the 3D grid.
- At a fixed morph position, the 2D waveshape panel should remain unchanged when switching between yellow/time, red, and blue primary axes.
- Yellow/time and red behave plausibly after the Cycle v2 bridge started propagating node-local primary axis state into the 3D panel/interactor.
- Blue is still wrong: the 2D waveshape changes, and the rails/color points appear offset relative to the curve. The visual effect looks similar to control points being associated with a neighboring intercept, or the yellow dimension not being applied consistently except for color points.
- The same class of behavior was checked in Cycle v1, which suggests this is likely in shared trilinear rasterizer/slicer logic rather than only the Cycle v2 popup bridge.

Current status: open.

## Open: Cycle v2 full-suite architecture assertions

Context:

- A full `CycleV2_tests` run during the Reverb spectrogram work on 2026-07-18
  failed `TestGraphPreviewExecutor.cpp:441`: `aliasedInputCount` was 0 rather
  than 8 in `Graph preview address lookup scales with compiled inputs`.
- The same run failed `TestNodeCanvasArchitecture.cpp:334`: the rich mesh view
  width was 972 rather than the expected 1080. Daven: That's probably just because of the spy bar though.
- The focused Reverb preview suite passes. Neither failure exercises the
  spectral mapping or Reverb rendering path modified in that work.

Current status: open; failures reproduced in the existing full test binary.

Likely area:

- `lib/src/Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h`
- `lib/src/Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.*`
- primary-view-axis handling across `RasterizationRequest::primaryViewDimension`, `RasterizationRequest::dims`, and `MorphPosition`

Expected invariant:

- For the same mesh and morph position, re-rasterizing the 2D waveshape with a different primary view axis should produce the same 2D waveform and aligned intercept/color-point overlays.

## Open: default Cycle v2 launch asserts while creating Effect2D widgets

Context:

- The focused EQ editor automation run on 2026-07-17 completed node creation and editor inspection, then logged `JUCE Assertion failure in Effect2DWidget.cpp:8`.
- The assertion is incidental to the EQ/Reverb/Delay popup work; the EQ editor state and response preview were produced successfully.
- Repro artifacts: `/private/tmp/cycle-v2-eq-editor-report.json` and `/private/tmp/cycle-v2-eq-editor-logs.txt`.

Current status: open.
