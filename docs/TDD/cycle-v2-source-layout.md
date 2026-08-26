# Cycle V2 Source Layout

## Status

In progress (2026-08-26).

## Problem

Cycle V2 has grown without internal directory boundaries in four areas:

- `src/UI` mixes canvas mechanics, rendering, automation, editors, palettes,
  previews, and workspace tools across 78 files;
- `src/Runtime` mixes processing contracts, graph coordination, realtime audio,
  preview execution, and oscillator-region preparation across 60 files;
- `src/Nodes/Effect2D` groups Envelope, Guide Curve, Impulse Response, and
  Waveshaper behavior by a legacy rendering mechanism instead of domain; and
- `src/Nodes/Trimesh` is one cohesive domain, but its 56 files leave model,
  DSP, editing, panel hosting, and rendering ownership implicit.

The graph layer is large but cohesive and remains under `src/Graph`.

## Authoritative Implementations And Preserved Behavior

This is a behavior-neutral source-layout migration. Every existing production
type remains authoritative for its current behavior. Files move intact except
for include paths, narrow naming corrections, and already-documented source
decomposition:

- graph mutation continues through `GraphCommandDispatcher`;
- runtime and preview execution retain their existing processors and lifecycle;
- Trimesh model, DSP, interaction, rasterization, and rendering behavior is
  reused unchanged;
- curve panels continue hosting the mature panel/interactor/rasterizer code;
  and
- generic canvas services retain the ownership established by
  `cycle-v2-node-canvas-architecture.md`.

No compatibility adapter is introduced. Cycle V2 is undeployed, so obsolete
`Effect2D` source vocabulary is renamed directly where it describes shared
curve infrastructure.

## Target Layout

```text
src/
  Graph/
  Runtime/
    Processing/
    Graph/
    Realtime/
    Preview/
    Oscillator/
  UI/
    Canvas/
      Automation/
      Rendering/
    Editors/
    Palette/
    Preview/
    Workspace/
  Nodes/
    Curve/
      Model/
      Panel/
      Editor/
    Envelope/Editor/
    Guide/Editor/
    ImpulseResponse/Editor/
    Waveshaper/Editor/
    Trimesh/
      Model/
      Dsp/
      Editor/
      Panel/
      Rendering/
```

Tests mirror the major `Graph`, `Runtime`, `UI`, and `Nodes` ownership
boundaries. Cross-directory project includes are source-root-qualified. Files
within one directory may use a local include.

## Ownership Rules

- `Runtime/Processing` owns payloads, buffers, processor contracts, factories,
  and shared processing mechanics.
- `Runtime/Graph` owns runtime graph configuration, causal updates,
  invalidation, module registration, and presentation coordination.
- `Runtime/Realtime` owns live/offline audio graph execution, MIDI state and
  queues, and concrete audio-processor registration units.
- `Runtime/Preview` owns graphic execution and preview processor registration.
- `Runtime/Oscillator` owns prepared oscillator regions and chained/spectral
  region rendering.
- `UI/Canvas` owns canvas orchestration, geometry, interaction, and queries;
  its `Automation` and `Rendering` children isolate those two concerns.
- `UI/Editors`, `Palette`, `Preview`, and `Workspace` own their named UI
  products and do not become generic helper folders.
- `Nodes/Curve` owns only behavior shared by curve-backed node families.
  Domain editors live with Envelope, Guide, Impulse Response, and Waveshaper.
- Trimesh subdirectories separate model state, DSP/preparation, editing,
  context-bound panels, and rendering without copying behavior between them.

## Migration Slices

1. Replace the `Effect2D` folder with shared Curve infrastructure and
   domain-owned editors. Rename misleading shared `Effect2D` types and split
   umbrella editor declarations by domain.
2. Partition Trimesh into Model, Dsp, Editor, Panel, and Rendering.
3. Partition Runtime into Processing, Graph, Realtime, Preview, and Oscillator.
4. Partition UI into Canvas, Editors, Palette, Preview, and Workspace. Apply
   the existing `ConcreteNodeEditors.cpp` registry decomposition while moving
   it so domain editor adapters live with their domain.
5. Mirror tests by ownership, normalize remaining project includes, and
   simplify the CMake source manifests around the resulting modules.

Each slice receives a production-diff review, style check, focused/full tests
as appropriate, a standalone build, and an imperative commit before the next
slice.

## Refactor Boundaries

The layout migration does not disguise unrelated large-file refactors.
`EnvelopeCurvePanel.cpp` retains its existing behavior and its decomposition
remains tracked in `docs/TDD/refactors.md`. Other translation units crossing
the style review threshold are recorded there rather than mechanically split
without an ownership design.

## Completion Criteria

- The four flat source folders described above no longer exist in their old
  form.
- No production include references `Nodes/Effect2D`.
- Shared Curve code contains no unnecessary node-kind switchboard introduced
  by the migration.
- Trimesh model, DSP, editor, panel, and rendering files have explicit homes.
- Runtime and UI directories communicate their lifecycle and ownership
  boundaries without generic `Helpers` or `Utils` modules.
- Tests mirror major production ownership boundaries.
- CMake source lists reflect the new modules without duplicate entries.
- `git diff --check`, the full Cycle V2 tests, and the Cycle V2 standalone
  build pass.
- Every migration slice is committed, and this document records final proof
  before its status changes to implemented.
