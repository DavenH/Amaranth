# Cycle V2 Trimesh Guide-curve Parity

## Status

Implemented. Compact Trimesh nodes retain the same authoritative prepared
widget model as the expanded editor, spectral grids use one domain-owned
frequency and value mapping in Trimesh views and signal spies, imported guide
assignments match Cycle 1's canonical data, and both mesh panels initialize the
shared assignment-tag resources.

`cycle-v2-guide-resource-dock.md` supersedes this TDD's Guide node and graph-edge
ownership. The provider, preparation, cube-component, DSP, and rendering
behavior documented here remains authoritative while resources and typed
assignments replace nodes and edges.

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

For imported presets, Cycle 1's migrated canonical JSON is the authoritative
source. The port preserves its meshes, layer properties, guide meshes, and
cube-component assignments unchanged. The boundary translation creates Cycle
2 graph nodes and routing edges only; it does not reinterpret the domain data.
The stable end state is a reusable preset conversion path rather than curated
copies whose guide edges can drift from the source.

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
6. Remove the compact-preview dual authority and characterize Baroque Flute's
   spectral-phase traversal with and without its authored guide attachments.
7. Make spectral row sampling and value mapping a render-profile property used
   identically by Trimesh grids and signal spies; delete the role-specific
   spectral preview mapper.

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
- Given one spectral traversal grid, Trimesh and signal-spy heatmaps produce
  identical mapped pixels; DC exclusion and logarithmic row sampling are
  independent of preview role.
- Two updates in one guide-assignment gesture, commit, visible/downstream
  refresh, and undo preserve the durable base-revision contract.
- Baroque Flute resolves all eleven imported cube-component assignments.
- African Horn renders its assigned guide in both mesh panels and produces
  guide-affected traversal-grid values.
- Alto Sax preserves all four guide meshes and its seventeen source-authored
  cube-component assignments across time and spectral layers.

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
- African Horn and Alto Sax graph attachments exactly match Cycle 1's migrated
  canonical preset data.
- DSP and visualization hot-loop checks, `git diff --check`, applicable
  clang-tidy, focused tests, the Cycle V2 test suite, and UI captures are run;
  unrelated failures or unavailable tools are recorded explicitly.

## Implementation Result

- `GuideCurveSnapshotProvider` owns Cycle 2's immutable 8192-sample padded
  bipolar tables and delegates table lookup, downsampling, phase rotation,
  stable noise, DC offset, and seed generation to the same
  `GuideCurveTableDsp` core used by Cycle 1.
- `TrimeshGuidePreparation` is the only graph-to-rasterizer adapter. It copies
  the destination mesh, clears legacy channels, maps cube-component edges to
  local provider slots, and delegates all deformation and waveform baking to
  the shared rasterizer policies.
- Blockwise, gridwise, spectral, oscillator, preview, compact, Panel2D, and
  Panel3D paths consume the prepared mesh/provider pair. Guide-source edits are
  part of the destination configuration and compact-sprite cache keys.
- Compact Trimesh rendering no longer returns early through the captured-grid
  heatmap route. That route applied spectral frequency remapping at audio-block
  resolution, while the expanded editor rendered its prepared model grid.
  Compact and expanded views now resolve through the same widget-owned grid;
  captured traversal remains authoritative for DSP diagnostics and probes.
- `TrimeshRenderProfile` owns DC exclusion, phase unwrapping, and spectral value
  scaling. Pitch-dependent frequency coordinates come from Cycle 1's
  `LogRegions` core. Trimesh model grids, mesh coordinates, and spy heatmaps
  consume that shared mapping independently of preview role; the former
  UI-specific spectral mapper remains deleted.
- Vertex-menu authoring now resolves the selected vertex's owning cubes and
  writes only `guide.cube.<index>.<field>` edges. The provisional vertex target
  parser and production path are deleted.
- The Baroque Flute fixture asserts amplitude rails and Time/component guide
  segments, including `phaseLayer1`, and captures the compact canvas; the
  African Horn fixture asserts its Curve guide and populated guided panel
  output. The Baroque compact capture is
  `/private/tmp/cycle-v2-baroque-compact-guides.png`.
- African Horn now restores the source preset's guide-1 phase assignments on
  cubes 2 and 3; the earlier cube-0 Curve edge was not present in Cycle 1.
  Cycle 2's 2D and 3D panels no longer suppress the mature Panel tag-atlas
  refresh, so their numbered assignment markers use the shared renderer.
- `port_cycle_v1_preset.py` translates canonical Cycle 1 data into graph
  ownership and routing. Alto Sax preserves its time, magnitude, phase, four
  guide, envelope, and waveshaper models, its magnitude range/pan/mode, and all
  seventeen cube-component guide assignments. Downstream domain inference now
  passes through a spectral-layer node so this mixed time/spectral graph does
  not need duplicated voice contexts or flattened layer behavior.

Verification completed with `*Guide*` (110 assertions), `[spectral][ui]` (50
assertions), `[preview]` (140 assertions), `[trimesh]` (674 assertions), the
Stengah probe fixture, the standalone Cycle V2 build, and `git diff --check`.
The earlier full 459-case Cycle V2 run passed 457 cases; the two Stengah
failures are the shared preset-pan issue recorded in `audio-bugs.md`.
`clang-tidy` was not available in the environment.

The source-fidelity follow-up passed the focused domain and shipped-preset
suites, the African Horn guide fixture, and the four-preset automation fixture.
The full Cycle V2 suite passed 8,639 assertions in 479 test cases. The
standalone Debug build completed, `git diff --check` passed, and no scalar
`std::<math>` calls were introduced in the changed production paths.
