# UI Bug Notes

## Addressed: Hosted Trimesh vertex drags used stale desktop coordinates

Context:

- `PanelHostComponent` discarded the coordinates carried by each JUCE mouse
  event and reconstructed them from the current desktop cursor position.
- Delayed delivery and in-process pointer automation could therefore hit or
  move a different vertex. A pointer-down at the panel's phase-0.50 intercept
  selected a phase-0.84 vertex in the focused reproduction.

Current status: addressed by translating the delivered event into the panel
host's coordinate space with JUCE's event-relative conversion. The focused
in-process fixture asserts the resulting vertex parameters, while native
smokes cover a full edit/commit/undo sequence and a spectral Stengah Trimesh.

## Open: Closing a hosted editor through automation dereferences its deleted target

Context:

- The combined Stengah interaction fixture opened the magnitude Trimesh editor
  and invoked `pointer` on `expanded:magnitudeLayer1.close`.
- Both a down/up sequence and the atomic click path crashed on the message
  thread in `CycleV2Automation::pointer` after the close action deleted the
  hosted component used as the current pointer target.
- Repro artifacts: `/private/tmp/cycle-v2-expanded-pan.log.ips` and
  `~/Library/Logs/DiagnosticReports/CycleV2-2026-08-03-213846.ips`.

Current status: open; pointer automation must not retain or dereference a
component after an event synchronously destroys its hosted editor.

## Open: Expanded curve-editor automation target can resolve to empty canvas

Context:

- A focused native Stengah Waveshaper check on 2026-08-03 opened the editor
  through automation, then captured the reported
  `expanded:waveshaper.panel2D` screen bounds.
- The crop at `/tmp/stengah-native/stengah-waveshaper-before-edit.png` contains
  empty canvas and compact graph nodes rather than the expanded curve panel.
  Native clicks at that target therefore cannot serve as evidence for editor
  gestures or hover cursors.
- The semantic curve publication and raw input/output probe assertions pass;
  this is an automation/editor-host visibility or coordinate-boundary defect,
  not the Stengah DSP failure.

Current status: open; make `openNodeEditor` wait for a visible hosted component
and derive native screen bounds from that component before restoring the
focused native curve gesture fixture.

## Addressed: Stengah Waveshaper selection restored before panel initialization

Context:

- Opening the Waveshaper editor from the locally edited Stengah preset crashes
  while restoring its saved flat-curve selection.
- The stack reaches `Interactor::getModPosition(bool)` through
  `FlatCurvePanelBase::restoreFlatSelection`. The selected vertex is valid, but
  selection frames were built before `Interactor2D::init` installed the
  panel's morph-position service.
- Repro artifact: `/private/tmp/cycle-v2-stengah-seven-probes-logs.txt.ips`.
- The consecutive-preset reproduction also produced
  `/private/tmp/cycle-v2-preset-version-logs.txt` and
  `CycleV2-2026-08-02-224917.ips`.

Current status: addressed by staging selection-frame reconstruction until the
panel host initializes; covered by focused lifecycle and consecutive-preset
automation.

## Addressed: Voice Context sliders only responded to pointer-down

- Octave and Pitch previously applied only the initial pointer-down value;
  pointer drag was not routed back to those controls.
- Voice Length discarded every cached preview sprite for each movement, making
  continuous dragging visibly laggy.
- The focused Voice Context fixture now drags Octave, Pitch, Voice Length, and
  Oversampling through complete down-drag-up sequences and asserts their
  resulting graph or preview state.

## Addressed: Trimesh rasterization ignored attached guide curves

Context:

- Baroque Flute and Stengah preserve their Cycle 1 cube-component guide
  assignments as `guide.cube.<index>.<field>` attachment edges.
- The Trimesh editor and preview currently rasterize the base mesh without the
  attached guide curve. The guide waveform and its noise, DC-offset, and phase
  controls therefore produce no visible contribution on assigned vertices.
- Repro: open either preset, inspect a Trimesh node with a guide attachment,
  then change the attached Guide Curve's noise control. The Trimesh rendering
  remains unchanged.

Current status: addressed by the provider-backed graph attachment preparation
described in `cycle-v2-trimesh-guide-curve-parity.md`; the Baroque Flute and
African Horn fixtures assert guide rails, component curves, and guided output.

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

## Addressed: Cycle v2 performance keyboard disappears after OpenGL startup

Context:

- The floating performance keyboard was initially a sibling drawn over a
  full-workspace `NodeCanvas`.
- It appeared during startup, then the canvas's attached native OpenGL surface
  covered it when that context became active. App-side component screenshots
  still showed the sibling and therefore produced a false-positive visual
  result.
- Reproduced on 2026-08-02 with the OS capture
  `/private/tmp/cycle-v2-keyboard-before-os.png`.

Current status: addressed by hosting the compact node-like panel as a child of
the OpenGL-attached canvas, reusing the established expanded-editor component
composition path. The performance fixture asserts the ownership boundary and
header drag behavior; the OS capture
`/private/tmp/cycle-v2-keyboard-node-os.png` verifies native composition.

## Addressed: Cycle v2 performance keyboard loses grip while dragging

Context:

- A header drag moved the canvas keyboard briefly, then stopped even though the
  primary mouse button remained held.
- `PerformanceKeyboardPanel::mouseDrag` re-evaluated the original mouse-down Y
  against the component's current coordinate system. Moving the component
  changed that coordinate, so an initially valid header gesture could fail its
  own header check partway through the drag.

Current status: addressed by latching header-drag ownership on mouse-down and
retaining it until mouse-up. The focused fixture performs two consecutive drag
updates, and a native `cliclick` gesture completed five diagonal movement
updates without losing grip.

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

## Addressed: Trimesh drag leaves graph edits transient

Context:

- A Trimesh drag could publish one or more mesh changes and then finish with
  `DidMeshChange` cleared after collision handling. The interactor consequently
  omitted the gesture-complete event, leaving the dispatcher's transient graph
  open.
- Later node moves and edge deletions were applied to that transient copy, so
  neither the durable graph nor the canvas changed.
- The 2D and 3D interactors now retain whether the current gesture published
  an edit and always emit its matching completion on mouse-up.
- `scripts/test_cycle_v2_native_edit_smoke.py trimesh-versioning` covers the
  collision/edit sequence followed by node move, edge deletion, and undo.

Current status: addressed on 2026-08-05.

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
## Open: GuideCurveOffsetSeeds paired-seed ownership test disagrees with current data

Context:

- The repository-wide test run on 2026-08-03, while verifying downstream pan
  preview invalidation, failed `TestRasterizerTypes.cpp:31` twice.
- `GuideCurveOffsetSeeds owns paired phase and vertical seed arrays` expected
  `verticalAt(2) == 4`, but the current implementation returned `16`.
- The failure is outside the graph presentation and pan paths changed in that
  work and reproduces in isolation.

Current status: open; reconcile the test's paired-array expectation with the
current `GuideCurveOffsetSeeds` storage contract.

## Open: synthetic expanded-editor close click uses a destroyed component

- Cycle V2 automation resolves an expanded editor component once, sends
  mouse-down, then sends mouse-up through the same raw pointer. An editor close
  button may destroy that component during mouse-down, causing `EXC_BAD_ACCESS`
  at `CycleV2Automation.cpp:1446` before mouse-up.
- Reproduced while verifying performance-keyboard occlusion with
  `expanded:waveMesh.close` on 2026-08-02. Crash artifact:
  `/private/tmp/cycle-v2-performance-keyboard-canvas-composition-logs.txt.ips`.
- This remains open; the keyboard composition fixture avoids the unrelated
  close gesture and asserts the reported pan/overlap sequence directly.

## Addressed: Cycle V2 guide rails requested a missing PathRepo singleton

- A Baroque Flute guide-curve screenshot run on 2026-08-04 completed the
  expanded Trimesh guide assertions, then logged `JUCE Assertion failure in
  SingletonRepo.h:54` when the guide rail was drawn.
- Enabling the mature `Panel::createLinePath` implementation exposed its
  `PathRepo` dependency, which the narrow Cycle V2 panel environment had not
  registered.
- Repro artifacts: `/private/tmp/cycle-v2-baroque-guides-report.json` and
  `/private/tmp/cycle-v2-baroque-guides-logs.txt.raw`.

Current status: addressed by registering and initializing `PathRepo` in the
Cycle V2 Trimesh panel environment; the guide fixture is the regression check.

## Open: Stengah magnitude-pan preset expectation has drifted

Context:

- The full `CycleV2_tests` run on 2026-08-04 failed
  `TestGraphSerializer.cpp:664` in `Stengah starts from its populated spectral
  layers`.
- The test expects `magnitudeLayer1Process.pan == 0.5`, while the committed
  `stengah.cyclegraph` contains `0.75833`.
- The failure reproduces in isolation and predates the probe-label, traversal,
  and expanded-detail changes.

Current status: open; decide whether the bundled preset or the canonical preset
assertion owns the intended authored pan, then update the losing authority.
