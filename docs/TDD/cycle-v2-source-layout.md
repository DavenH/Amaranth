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

The same problem also appears inside function-grouped translation units.
`EffectPreviewRenderer`, `EffectSignalProcessors`, `EffectNodeAudioProcessors`,
`CurvePreviewProcessors`, `ConcreteNodeEditors`, and `NodePreviewRenderer`
collect behavior for unrelated node kinds. Functions such as `paintReverb` and
`paintDelay` make the implementation searchable only if a reader already knows
which generic switchboard owns it, and adding a node expands central branches
instead of its domain module.

The graph layer is large but cohesive and remains under `src/Graph`.

## Authoritative Implementations And Preserved Behavior

This is a behavior-neutral source-layout and ownership migration. Every
existing production algorithm remains authoritative for its current behavior.
Files move intact where already cohesive; mixed-domain translation units are
split into domain objects without changing their algorithms:

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
    Oscillator/
  UI/
    Canvas/
      Automation/
      Rendering/
    Editors/
    Palette/
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
    Reverb/
    Delay/
    Equalizer/
    Unison/
    FFT/
    SpectralLayer/
    WaveSource/
    ImageSource/
    VoiceContext/
    Add/
    Multiply/
    StereoSplit/
    StereoJoin/
    Output/
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
  queues. It does not own concrete node processors.
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
- Each concrete node domain owns its Cycle V2 audio adapter, DSP configuration,
  runtime-preview adapter, compact-preview painter, and editor/factory as
  applicable. A reader looking for Reverb behavior starts in `Nodes/Reverb`.
- Domain preview painters are objects implementing a narrow preview-painting
  capability. `ReverbPreviewPainter::paint()` and
  `DelayPreviewPainter::paint()` replace generic `paintReverb`/`paintDelay`
  functions.
- Generic registries may map a `NodeKind` to a domain factory or capability.
  Registration code contains no rendering, interaction, DSP, or parameter
  policy and must not become another switchboard.
- Shared processing and presentation contracts live outside domains only when
  they contain no concrete `NodeKind` branches or domain-specific parameter
  identifiers.

## Negative Boundaries

- `Runtime` and generic `UI` files do not implement concrete node algorithms.
- A generic preview renderer does not branch across Reverb, Delay, Equalizer,
  Unison, FFT, Spectral Layer, source, routing, and output presentation.
- A generic editor does not branch on effect kind to choose controls, snapping,
  plotting, automation, or layout.
- A broad `Nodes/Effects` folder does not remain as the owner of distinct
  Reverb, Delay, Equalizer, Impulse Response, Waveshaper, and Unison behavior.
- Domain extraction must not copy the existing paint, DSP, or interaction
  algorithms. The existing implementation moves behind the domain object.

## Migration Slices

1. **Completed:** Replace the `Effect2D` folder with shared Curve infrastructure
   and domain-owned editors. Rename misleading shared `Effect2D` types and
   split umbrella editor declarations by domain.
2. **Completed:** Partition Trimesh into Model, Dsp, Editor, Panel, and
   Rendering.
3. Extract the Reverb, Delay, Equalizer, and Unison vertical domains. Split
   effect preview painting, signal processors, audio adapters, preview
   processors, and editor implementations into domain-owned objects. Delete
   the broad `Nodes/Effects` implementation folder.
4. Extract remaining concrete runtime audio and preview implementations from
   function-grouped Runtime files into Envelope, Impulse Response, Waveshaper,
   FFT/IFFT, Spectral Layer, Trimesh, source, math, routing, Voice Context, and
   Output domains. Runtime retains only contracts and orchestration.
5. Replace the node-kind branches in `NodePreviewRenderer` with registered
   domain preview painters, and reduce `ConcreteNodeEditors.cpp` to registry
   assembly from domain factories.
6. Partition the remaining generic UI and Runtime orchestration files by their
   cohesive lifecycle boundaries.
7. Mirror tests by ownership, normalize remaining project includes, and
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
- Reverb, Delay, Equalizer, Unison, and every other concrete node behavior can
  be found under its domain folder.
- `EffectPreviewRenderer`, `EffectSignalProcessors`,
  `EffectNodeAudioProcessors`, and the other mixed-domain processor files are
  deleted after their implementations move behind domain objects.
- Generic Runtime and UI orchestration contain no multi-domain behavior
  switchboards; central kind lists perform registration only.
- Preview painting and editor behavior are invoked through domain-owned
  objects rather than `paintFoo`/`drawFoo` branches in generic files.
- Runtime and UI directories communicate their lifecycle and ownership
  boundaries without generic `Helpers` or `Utils` modules.
- Tests mirror major production ownership boundaries.
- CMake source lists reflect the new modules without duplicate entries.
- `git diff --check`, the full Cycle V2 tests, and the Cycle V2 standalone
  build pass.
- Every migration slice is committed, and this document records final proof
  before its status changes to implemented.
