# UI Bug Notes

## Open: Envelope purpose selector test segfaults after its assertions

Context:

- Repository-wide CTest on 2026-08-12 failed
  `Envelope purpose selector publishes bipolar pitch presentation` at
  `cycle-v2/tests/TestNodeEditorHost.cpp:694` with `SIGSEGV` after three of its
  four assertions passed.
- A focused rerun reproduces the same crash. This is incidental to the shared
  trilinear dimension-order fix, which does not touch Envelope editor hosting.
- Repro output is recorded in
  `build/tests/Testing/Temporary/LastTest.log`.

Current status: open; inspect teardown/lifetime ownership in the Envelope
purpose selector fixture.

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

## Resolved: Trilinear mesh 2D rasterization changed with primary view axis

Context:

- In the Cycle v2 Trilinear Mesh popup, changing the primary view axis should only change which morph dimension is swept by the 3D grid.
- At a fixed morph position, the 2D waveshape panel should remain unchanged when switching between yellow/time, red, and blue primary axes.
- Yellow/time and red behave plausibly after the Cycle v2 bridge started propagating node-local primary axis state into the 3D panel/interactor.
- Blue is still wrong: the 2D waveshape changes, and the rails/color points appear offset relative to the curve. The visual effect looks similar to control points being associated with a neighboring intercept, or the yellow dimension not being applied consistently except for color points.
- The same class of behavior was checked in Cycle v1, which suggests this is likely in shared trilinear rasterizer/slicer logic rather than only the Cycle v2 popup bridge.

Likely area:

- `lib/src/Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h`
- `lib/src/Curve/Rasterization/Rasterizer/TrilinearMeshRasterizer.*`
- primary-view-axis handling across `RasterizationRequest::primaryViewDimension`, `RasterizationRequest::dims`, and `MorphPosition`

Expected invariant:

- For the same mesh and morph position, re-rasterizing the 2D waveshape with a different primary view axis should produce the same 2D waveform and aligned intercept/color-point overlays.

Resolution:

- `MorphPosition::getOtherDims(Vertex::Blue)` returned `(Red, Time)`, while
  `VertCube::getFace(Vertex::Blue)` stores its face coordinates as
  `(Time, Red)`. Blue-axis slicing consequently interpolated the face across
  the wrong edges.
- The shared dimension order now matches the authoritative `VertCube` face
  layout, fixing both Cycle v1 and Cycle v2 trilinear callers.
- Regression coverage renders one fixed mesh/morph point through all three
  primary axes and requires identical intercepts, color-point overlays, and 2D
  waveform buffers. A focused native fixture also exercises the red/blue
  primary-axis controls:
  `scripts/fixtures/cycle-v2-agent-trimesh-primary-axis.json`.

Current status: resolved on 2026-08-12.

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

## Open: default Cycle v2 launch asserts while creating Effect2D widgets

Context:

- The focused EQ editor automation run on 2026-07-17 completed node creation and editor inspection, then logged `JUCE Assertion failure in Effect2DWidget.cpp:8`.
- The assertion is incidental to the EQ/Reverb/Delay popup work; the EQ editor state and response preview were produced successfully.
- Repro artifacts: `/private/tmp/cycle-v2-eq-editor-report.json` and `/private/tmp/cycle-v2-eq-editor-logs.txt`.

Current status: open.
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

## Open: Trimesh native smoke compares serialized floats exactly

Context:

- The complete `trimesh` native edit smoke on 2026-08-10 completed its drag,
  parameter, deletion, undo, and cube-add interactions without crashing, then
  failed its save/reload topology equality assertion at
  `scripts/test_cycle_v2_native_edit_smoke.py:1108`.
- Reloaded mesh values differed only by XML/JSON float quantization (for
  example, `0.15441218` became `0.15441`). The focused
  `trimesh-versioning` sequence, including an immediate second drag after graph
  publication, passes.

Current status: open; compare serialized Trimesh float fields with an explicit
tolerance while retaining exact topology and guide-assignment checks.

## Open: native downstream fixtures can sample incomplete asynchronous state

Context:

- During shared curve interaction verification on 2026-08-12, the Envelope
  native sequence completed its hover, multi-update reshape, visible-direction,
  publication, and exact-undo assertions, then `captureAudio` returned JSON
  `null` entries to the existing final downstream comparison.
- The existing `causal-trimesh` sequence also observed completed downstream
  preview products before its immediate snapshot contained the expected
  `DurablePublication` event. The returned trace continued through later
  completion events, indicating the fixture sampled an asynchronous boundary.
- Repro log:
  `/private/var/folders/zx/hdzf3v1s6vvdz7chbz40bbtc0000gn/T/cycle-v2-native-edit-smoke.log`.

Current status: open; make audio capture reject or retry non-finite samples and
wait explicitly for durable causal completion before evaluating the trace.
