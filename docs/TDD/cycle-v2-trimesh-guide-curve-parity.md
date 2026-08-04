# Cycle V2 Trimesh Guide-curve Parity

## Status

Implemented. Focused guide/Trimesh tests and both preset automation fixtures
pass. The repository-wide Cycle V2 suite was also run; its two remaining
failures share the unrelated Stengah `magnitudeLayer1Process.pan` preset drift
recorded in `audio-bugs.md`.

## Problem

Cycle V2 preserves Cycle 1 guide-curve nodes, cube-component guide metadata,
and graph attachment edges, but its Trimesh renderers never receive a
`GuideCurveProvider`. Consequently:

- 2D and 3D Trimesh panels do not draw guide rails or assignment tags;
- component curves are baked as ordinary sharpness curves;
- guide assignments do not move rasterized intercepts;
- blockwise and gridwise DSP ignore the attached guide curves.

Baroque Flute and African Horn are the principal parity fixtures because they
contain authored guide curves, cube-component assignments, mesh editor data,
and traversal-grid output.

## Authoritative Implementation

Cycle 1 remains authoritative at these boundaries:

- `GuideCurvePanel` rasterizes each authored guide mesh into an 8192-sample
  bipolar table over the padded interval `[0.05, 0.95]`, and applies stable
  noise, vertical-offset, and phase-offset parameters.
- `GuideCurvePolicy` applies red/blue, phase, amplitude, and curve guides to
  trilinear intercepts.
- `WaveformBakePolicy` applies the Time/component guide while baking the
  waveform and records guide regions.
- `Panel::createLinePath`, `Panel2D::drawGuideCurveTags`, and
  `Panel3D::drawGuideCurveTags` render the deformed rails and assignment tags
  from the same provider-backed rasterizer state.

Cycle V2 must reuse those implementations unchanged. It must not add another
curve evaluator, guide deformation algorithm, component-curve baker, or panel
rail renderer.

## Design

Introduce a narrow Cycle V2 guide snapshot/provider boundary.

The boundary owns:

- immutable sampled guide tables and their V1-compatible parameters;
- deterministic guide noise storage and stable per-guide seeds;
- a literal translation from graph attachment targets to local provider slots;
- a render-time mesh copy whose guide-channel metadata reflects only current
  graph attachments.

The boundary delegates:

- point-curve rasterization to the existing point-list/FX rasterizer;
- intercept deformation to `GuideCurvePolicy`;
- component-curve baking to `WaveformBakePolicy`;
- rail and tag drawing to the existing Panel 2D/3D implementation.

The graph edge is authoritative for attachment identity. Stable
`guide.cube.<index>.<field>` targets map directly to one `VertCube` component.
The provisional `guide.vertex.<index>.<field>` authoring route must be migrated
to cube-component targets and then removed; it must not become a second domain
model.

Snapshot ownership is outside the audio callback. Blockwise and gridwise DSP
hold only stable provider/configuration references during processing. Guide
edits invalidate every attached Trimesh configuration and preview product.

## Slices

1. Add the immutable provider and characterize its table, parameter, seed, and
   density behavior against Cycle 1.
2. Resolve graph guide attachments into prepared Trimesh configurations and
   use them in blockwise, gridwise, oscillator, and preview rendering.
3. Bind the same prepared context to compact and expanded Trimesh UI rendering;
   enable the mature 2D/3D guide rail, component curve, and tag paths.
4. Replace provisional vertex-target authoring with cube-component assignment,
   including full gesture, commit, refresh, and undo coverage.
5. Add Baroque Flute and African Horn focused automation assertions for guide
   attachment resolution, changed grid output, and 2D/3D render state.

## Semantic Tests

- A known flat guide produces the same padded bipolar table as Cycle 1.
- Noise, DC offset, and phase offset are deterministic and use the same units
  and indexing rules as Cycle 1.
- A cube amplitude guide changes intercept amplitude and blockwise output.
- A cube phase guide changes intercept phase and gridwise output.
- A Time/component guide changes the baked component curve rather than merely
  changing vertex sharpness.
- Unattached guide nodes do not affect a Trimesh.
- Editing one attached guide invalidates and changes every attached Trimesh,
  including captured traversal output.
- Compact preview, expanded fallback rendering, Panel2D, Panel3D, blockwise DSP,
  and gridwise DSP consume one equivalent provider-backed configuration.
- Two updates in one guide-assignment gesture, commit, visible/downstream
  refresh, and undo preserve the durable base-revision contract.
- Baroque Flute resolves all eleven imported cube-component assignments.
- African Horn renders its assigned guide in both mesh panels and produces
  guide-affected traversal-grid values.

## Completion Criteria

- Guide rails and assignment tags render in Cycle V2 Trimesh 2D and 3D panels.
- Component curves render with Cycle 1 semantics.
- Assigned guide curves move the displayed intercepts/vertices and affect
  blockwise and gridwise DSP output.
- Baroque Flute and African Horn focused fixtures pass with observable render
  and traversal assertions.
- No guide evaluation, deformation, component baking, or rail drawing logic is
  duplicated in Cycle V2.
- The provisional vertex-target compatibility path and its tests are deleted.
- DSP and visualization hot-loop checks, `git diff --check`, applicable
  clang-tidy, focused tests, the Cycle V2 test suite, and UI captures are run;
  unrelated failures or unavailable tools are recorded explicitly.

## Implementation Result

- `GuideCurveSnapshotProvider` reproduces Cycle 1's 8192-sample padded bipolar
  tables, guide parameters, stable per-guide noise, and domain-stable visual
  offset seeds.
- `TrimeshGuidePreparation` is the only graph-to-rasterizer adapter. It copies
  the destination mesh, clears legacy channels, maps cube-component edges to
  local provider slots, and delegates all deformation and waveform baking to
  the shared rasterizer policies.
- Blockwise, gridwise, spectral, oscillator, preview, compact, Panel2D, and
  Panel3D paths consume the prepared mesh/provider pair. Guide-source edits are
  part of the destination configuration and compact-sprite cache keys.
- Vertex-menu authoring now resolves the selected vertex's owning cubes and
  writes only `guide.cube.<index>.<field>` edges. The provisional vertex target
  parser and production path are deleted.
- The Baroque Flute fixture asserts amplitude rails and Time/component guide
  segments; the African Horn fixture asserts its Curve guide and populated
  guided panel output. The Baroque capture is
  `/private/tmp/cycle-v2-baroque-guides.png`.

Verification completed with `*Guide*` (98 assertions), `[trimesh]` (703
assertions), both automation fixtures, the standalone Cycle V2 build, and
`git diff --check`. The full 459-case Cycle V2 run passed 457 cases; the two
Stengah failures are the shared preset-pan issue above. `clang-tidy` was not
available in the environment.
