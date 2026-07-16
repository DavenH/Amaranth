# Rasterizer Result Ownership And Publication

## Status

Implemented, 2026-07-16.

This is a focused corrective follow-up to
[`rasterization-simplification.md`](rasterization-simplification.md). That TDD
defines `RenderResult` as the single owned output of a rasterization pass. The
current implementation does not consistently satisfy that rule.

## Problem

`TrilinearMeshRasterizer` currently stores the same geometry in three places:

1. `meshSliceResult.intercepts` and `meshSliceResult.colorPoints`, produced by
   `TrilinearMeshSlicer`;
2. `meshIntercepts` and `meshColorPoints`, copied immediately after slicing;
3. `RasterizerData::intercepts` and `RasterizerData::colorPoints`, copied again
   during panel snapshot publication.

The publication copy can be legitimate when it is the synchronization boundary
between a mutable producer and panel consumers. The intermediate
`meshIntercepts`/`meshColorPoints` copy is not such a boundary. It is competing
working authority. `waveformOutput`, a nullable pointer selecting another
`RenderResult`, then decides which curves and waveform belong beside those
vectors.

This is not isolated to intercepts. The audit found related ownership and
publication problems:

- `RasterizationRequest` contains both `batchMode` and `publishSnapshot` even
  though publication is orchestration, not rasterization input.
  `publishSnapshot` has no production reader. `batchMode` is honored by
  `GraphicRasterizer` but ignored by `TrilinearMeshRasterizer`.
- Cycle V2 sets `batchMode` for `TrimeshBlockwiseDsp` and
  `TrimeshGridwiseDsp`, but the trilinear rasterizer still publishes and copies
  a panel snapshot after every render. Grid rendering repeats that work for
  every column.
- `TrimeshBlockwiseDsp` samples the mutable working waveform but obtains its
  initial sample index from the separately copied snapshot.
- `SnapshotView` returns mutable vector references from `const` accessors and
  exposes locking as an optional caller responsibility. This causes repeated
  `const_cast` use and permits reads outside the publication lock.
- `TrimeshPanelRasterizer` exposes both `getIntercepts()` from working state and
  `snapshotView()` metadata. Its bridge adds another pass-through accessor used
  only by a test.
- `EnvRasterizer` keeps `paddingSize`, `unsampleable`, and `needsResorting`
  beside fields with the same apparent meaning on `RenderResult`. In
  particular, `RenderResult::sampleable` and `RenderResult::needsResorting`
  are copied by `adoptPreparedData` but are not maintained by the normal
  envelope render path.
- `VoiceRasterizer` has legitimate chained and ordinary outputs, but models
  the active result through parallel state plus `chainedOutputActive`, rather
  than exposing an explicit selected result/view.

These shapes make it possible for geometry, waveform, metadata, and panel
markers to describe different render passes. They also hide avoidable vector
copies and allocation-capable publication work inside analysis and audio paths.

## Goals

- Make one `RenderResult` the authoritative working output of each render
  pass.
- Treat panel publication as an explicit, optional synchronization boundary,
  not a flag embedded in `RasterizationRequest`.
- Make a sampler and its cursor metadata come from the same result.
- Make snapshot data immutable to consumers and make correct locking
  unavoidable or unnecessary.
- Remove duplicate status fields or give genuinely distinct state distinct
  names and types.
- Preserve Cycle v1 rendering, panel interaction, envelope playback, voice
  chaining, and Cycle v2 Trimesh behavior.
- Reduce containers, copies, forwarding methods, and state-selection booleans;
  do not add another adapter layer.

## Non-Goals

- Change interpolation, guide curves, padding, curve resolution, or waveform
  mathematics.
- Merge envelope playback state into the generic rasterizer.
- Remove the immutable/copy publication boundary when panel and producer
  threads genuinely require it.
- Reimplement mature Cycle v1 rasterization behavior in Cycle v2.
- Introduce another domain-specific rasterizer family.

## Ownership Rules

### Working result

Each rasterizer owns exactly one authoritative result for an ordinary render.
Mesh slicing writes geometry into that result and waveform construction extends
the same result with padding, curves, buffers, and sampleability. Code must not
copy geometry into a second working vector merely to pass it to the next stage.

Voice chaining may own a second result because it represents a distinct
cross-call product. The API must name ordinary versus chained results
explicitly; a consumer must never combine fields from both.

### Published snapshot

A panel snapshot is an immutable publication of one complete working result.
It is allowed to own copied storage when that copy is the thread boundary. The
publication operation must copy or exchange all geometry, curves, waveform,
indices, padding, wrapping, and sampleability atomically.

Render-only consumers do not publish. Audio, analysis grids, configuration
builders, and batch column renderers consume the working result or a sampler
view directly.

### Views

`SamplerView` borrows waveform and cursor metadata from one result.
`SnapshotView` exposes only const data. Lock ownership must be represented by
an RAII read object or eliminated through immutable snapshot exchange; callers
must not receive mutable containers and remember to acquire a separate lock.

## Proposed Design

The existing `RenderResult` remains the owned product. The trilinear path is
collapsed around it:

```cpp
class TrilinearMeshRasterizer {
public:
    const RenderResult& renderGeometry(const RasterizationRequest& request);
    const RenderResult& renderWaveform(const RasterizationRequest& request);

    const RenderResult& result() const;
    SamplerView sampler() const;
    void publishSnapshot();

private:
    RenderResult output;
};
```

The concrete API may retain current mesh/request ownership temporarily, but
the result relationship is invariant:

- `TrilinearMeshSlicer` fills `output.intercepts` and `output.colorPoints`;
- curve/waveform construction extends `output` without clearing or copying its
  geometry;
- `sampler()` and cursor initialization read `output.waveform`;
- `publishSnapshot()` publishes `output` only when a panel owner requests it.

`updateGeometry()` and `updateWaveform()` may remain compatibility operations
that render and publish for Cycle v1 `Updateable` callers. New DSP and analysis
code must use the render-only boundary.

## Work Slices

### 1. Unify Trilinear Working Output

- Replace `meshSliceResult`, `waveformResult`, `meshIntercepts`,
  `meshColorPoints`, and `waveformOutput` with one authoritative
  `RenderResult` where behavior permits.
- Change curve/waveform construction to extend an existing result instead of
  clearing a separate one and copying intercepts into it.
- Delete `TrilinearMeshRasterizer::getWaveformOutput()`; it has no callers.
- Replace the Trimesh bridge's working-intercept accessor with the appropriate
  result or immutable snapshot view.
- Remove the test-only `getRasterizerIntercepts()` forwarding chain.

Acceptance:

- exactly one working intercept vector and one working color-point vector
  exist on the ordinary trilinear path;
- geometry-only renders clear waveform fields in that same result;
- waveform renders preserve the geometry produced by the same slice;
- existing numerical and panel characterization tests remain equivalent.

### 2. Separate Rendering From Publication

- Remove the unused `RasterizationRequest::publishSnapshot` field.
- Remove publication policy from `batchMode`; then remove `batchMode` if no
  rendering mathematics requires it.
- Give panel owners an explicit render-and-publish operation.
- Route Cycle v2 blockwise, gridwise, preview, and configuration rendering
  through render-only operations.
- Ensure grid rendering does not copy a panel snapshot for every column.

Acceptance:

- a render-only Trimesh call leaves the published snapshot unchanged;
- a panel update publishes exactly once after a completed render;
- a grid of `n` columns performs zero panel publications, not `n`;
- the audio processing call performs no snapshot locking, vector copying, or
  allocation caused by publication.

### 3. Make Result Views Coherent And Immutable

- Add cursor/index access to `SamplerView`, or provide a block sampling method
  that removes the external index entirely.
- Stop combining `rasterizer.sampler()` with
  `rasterizer.snapshotView().zeroIndex()`.
- Make snapshot vector access const and make `snapshotView()` const-correct.
- Replace caller-managed `snapshot.lock()` sequences with an RAII immutable
  read snapshot or immutable snapshot exchange.
- Remove `const_cast` from Cycle v2 curve panel snapshot reads.

Acceptance:

- sampler data and cursor metadata always originate from the same result;
- no snapshot consumer can mutate published vectors;
- no panel consumer can read snapshot storage without owning its read lifetime;
- ThreadSanitizer-friendly tests can alternate publication and reads without
  observing mixed fields.

### 4. Audit Remaining Rasterizer State

- Collapse `EnvRasterizer` sampleability, padding, and resort state into
  `RenderResult` where those fields describe rasterization output.
- Give envelope playback-only state distinct names and ownership.
- Make ordinary versus chained voice results explicit and remove parallel
  selection logic where possible.
- Review `FXRasterizer` and `PointListWaveformRasterizer` for redundant source
  extraction and publication copies after the common result API lands.
- Delete dead flags, getters, forwarding methods, and tests that only preserve
  the old duplicated surface.

Acceptance:

- every result-like scalar has one writer and one documented owner;
- no boolean chooses between unnamed parallel result bundles;
- no production request field controls publication side effects;
- the rasterizer tree has fewer state containers and less code after the work.

## Semantic Tests

Tests must assert ownership consequences, not member names:

- after geometry render, result and published snapshot agree only after an
  explicit publication;
- after waveform render, intercepts, curves, waveform indices, and sampler
  describe the same pass;
- deliberately skipping publication changes render-only output but not the
  prior snapshot;
- a panel publication advances all observable fields together;
- Trimesh blockwise and gridwise render paths perform no panel publication;
- allocation instrumentation around prepared audio rendering detects no
  publication allocation;
- Cycle v1/Cycle v2 numerical and visual parity remains within existing
  tolerances;
- Envelope and voice tests distinguish rasterization result state from
  playback/chaining state.

At least one fault injection must redirect a consumer to stale snapshot cursor
metadata and demonstrate that the coherence test fails.

## Verification

- Build affected `AmaranthLib`, Cycle, Cycle v2, and test targets with
  `--parallel 10` or higher.
- Run focused rasterizer, Trimesh, Envelope, voice, and allocation tests.
- Run the complete test suite.
- Run Cycle v1 panel baselines and PSNR comparison for waveform, spectrum,
  guide, Waveshaper, and IR panels.
- Run Cycle v2 native Trimesh editing and downstream audio smoke coverage.
- Use OS capture for OpenGL-backed panel verification.

## Implementation Evidence

- `TrilinearMeshRasterizer` now owns one `RenderResult`. Slicing fills its
  intercepts and color points, and waveform construction extends that same
  object. The former parallel results, copied geometry vectors, nullable result
  selector, and working-intercept forwarding API are gone.
- Rendering and publication are separate operations. Cycle v1 analysis grids
  and Cycle v2 block/grid DSP use `renderWaveformOnly`; compatibility panel
  updates render and publish. `batchMode` and the unused `publishSnapshot`
  request flags were deleted.
- `SamplerView::initialIndex()` comes from its borrowed waveform.
  `SnapshotView` is a move-only, RAII-locked immutable read lifetime; mutable
  vector access, caller-managed locking, temporary container views, and panel
  `const_cast`s were removed. Snapshot publication now includes sampleability
  with geometry, waveform, indices, padding, and wrapping.
- Envelope output status now lives on its `RenderResult`. Voice rasterization
  uses explicit ordinary/chained output selection, and chained output status
  lives with the chained result. The FX/point-list boundary was reviewed: its
  transient vector is the required `Vertex*`-to-`Intercept` conversion input,
  not a second retained result authority.
- The ownership test proves that a render-only call changes working output
  while leaving the prior complete published snapshot unchanged, then proves
  that explicit publication makes intercepts, sampleability, and cursor index
  coherent. Fault injection returning cursor index `0` instead of the result's
  index failed this test with `48 == 0`, then was reverted.

Verification performed:

- `AmaranthLib_tests`, `Cycle_tests`, `CycleV2_tests`, `Cycle`, and `CycleV2`
  built with `--parallel 10`.
- Focused rasterization, mesh, voice, envelope-grid, Trimesh, and realtime
  allocation suites passed.
- 410 of 411 discovered tests passed. The sole failure is the pre-existing,
  already-recorded exact-float Reverb comparison in `audio-bugs.md`; its two
  outputs differ at floating-point rounding scale.
- The macOS-native Cycle v2 edit smoke passed through real AppKit/JUCE event
  delivery.
- Cycle v1 OS-captured bongo panels rendered successfully. Static Guide and
  Waveshaper panels matched pixel-for-pixel. Dynamic waveform/spectrum panels
  are not deterministic between two consecutive captures (16.55--38.62 dB
  between those runs), so their historical 48 dB PSNR threshold cannot serve
  as a valid regression oracle for this change.

## Completion Criteria

- The ordinary trilinear path owns one working `RenderResult`.
- Published snapshot storage is the only justified duplicate, and its thread
  boundary is explicit.
- Render-only paths cannot accidentally publish.
- Snapshot consumers are immutable and lifetime-safe.
- Samplers never obtain cursor metadata from another result.
- Duplicate Envelope/voice result state is removed or explicitly separated by
  domain role.
- The change produces a net reduction in containers, forwarding APIs, flags,
  and rasterizer code.
