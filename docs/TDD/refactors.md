# Refactor Notes

## Cycle 2 Node Architecture TDDs

The repository-wide node architecture review is captured in four ordered TDDs:

1. `cycle-v2-node-definition-and-graph-model.md` defines the authoritative node
   registry, typed parameter schema, graph aggregate, transactions, change
   sets, and versioned serialization.
2. `cycle-v2-node-module-runtime.md` defines concrete processor and preview
   module boundaries plus compiled buffer-slot ownership and realtime views.
3. `cycle-v2-node-canvas-architecture.md` defines graph-document, command,
   viewport, scene, hit-testing, rendering, editor-hosting, presentation, and
   automation boundaries.
4. `cycle-v2-curve-node-models-and-editors.md` separates flat curve nodes from
   Envelope domain state and replaces the shared Effect2D god-objects with
   composed panel infrastructure and cohesive node editors.

Implement the definition and graph-model foundation first. Runtime, canvas, and
curve-model work may then proceed independently where their touched boundaries
do not overlap.

## Cycle 2 DSP Configuration Publication

The graph-wide lifecycle, ownership, migration plan, and verification gates are
specified in `cycle-v2-dsp-configuration-publication.md`.

## Cycle 2 Node Canvas Responsibilities

The authoritative design and staged extraction plan now live in
`cycle-v2-node-canvas-architecture.md`.

`cycle-v2/src/UI/NodeCanvas.*` is currently carrying rendering, hit-testing, graph editing dispatch, snapshot save/load, undo/redo, and graph-status presentation. This was acceptable for fast prototyping, but it should be split before deeper interaction work lands.

Suggested extraction points:

- `NodeCanvasViewTransform` for pan/zoom and world/screen projection.
- `NodeCanvasHitTester` for nodes, ports, edges, palette, and minimap.
- `NodeCanvasRenderer` or smaller render helpers for grid, cables, nodes, previews, minimap, palette, and status.
- `NodeCanvasController` for graph edit commands, undo/redo, and snapshot persistence.

Keep the graph mutation rules in `GraphEditor`; the canvas layer should remain a client of that API rather than duplicating graph semantics.

## Cycle 2 Graph Audio Buffer Ownership

The authoritative runtime storage, compiled-slot, and verification plan now
lives in `cycle-v2-node-module-runtime.md`.

`cycle-v2/src/Runtime/AudioProcessBlock` currently owns `std::vector<float>`
sample storage. This is acceptable scaffolding, but it should not become the
long-term DSP storage model.

Suggested direction:

- Use `Buffer<float>` and the repo's stereo buffer conventions for node DSP
  APIs wherever possible.
- Keep `AudioProcessBlock` as a lightweight graph-runtime view/metadata wrapper
  for domain, channel layout, and port routing.
- Introduce a compiled-plan work arena backed by `ScopedAlloc<float>` or a
  similar preallocated block allocator.
- Let the graph execution plan precompute buffer sizes, lifetimes, and port
  mappings.
- During processing, hand node modules `Buffer<float>` views into that arena
  instead of allocating per-node/per-port vectors.

This should be addressed before hardening the Cycle 2 runtime for realtime
audio-thread execution.

## Cycle 2 Runtime Node Processor Boundaries

The authoritative processor, preview, and module-factory design now lives in
`cycle-v2-node-module-runtime.md`.

`cycle-v2/src/Runtime/NodeAudioProcessor.cpp` currently contains too many
node-specific implementations inside one `FixedRoleProcessor` class. That was
useful scaffolding while the graph payload model was moving, but it violates
the intended node ownership boundaries and makes it too easy to reimplement
Cycle 1 DSP semantics incorrectly in the executor.

Suggested direction:

- Move domain-specific DSP into node folders, following the FFT and Trimesh
  split (`BlockwiseDsp`, `GridwiseDsp`, and small role adapters).
- Keep `NodeAudioProcessor` as a dispatch/factory interface and payload
  adapter, not as the place where Waveshaper, IR, Delay, Reverb, or Trimesh
  algorithms live.
- Port or wrap Cycle 1 implementations such as
  `Waveshaper::processBuffer(AudioSampleBuffer&)` and
  `IrModeller::processBuffer(AudioSampleBuffer&)` behind Cycle 2 node-specific
  adapters instead of approximating their DSP locally.
- Share exact rasterizer/table semantics with UI previews when a node depends
  on editable `Panel2D` curves.

## Cycle 2 Effect2D Mesh Ownership

The authoritative model, panel-host, editor, and migration design now lives in
`cycle-v2-curve-node-models-and-editors.md`.

`Effect2DPanelBridge` currently uses `EnvelopeMesh` as the backing store for
all effect-style panels. That is semantically wrong for waveshaper, guide, and
IR panels, which only need a flat `Mesh` of vertices. The current arrangement
exists because the shared bridge also serves Envelope, which requires cubes,
loop/sustain markers, and `EnvRasterizer`.

Suggested direction:

- Split Envelope-specific bridge/model code from flat FX panel code.
- Back Waveshaper, Guide, and IR panels with `Mesh`, not `EnvelopeMesh`.
- Keep Envelope loop/sustain and morph state in an envelope-only model.
- Let shared UI host code depend on a narrow panel interface rather than a
  concrete mesh type.

## Cycle 2 Trimesh Rasterizer Compatibility Boundary

The voice rasterizer façade, chaining state, and policies now live in
`AmaranthLib` as `Rasterization::VoiceRasterizer`, `VoiceCycleState`, and the
shared voice policy set. Cycle 1's `VoiceMeshRasterizer` is a thin adapter that
publishes its application constant and preserves legacy construction. Cycle
2's current Trimesh backend correctly reuses
`Rasterization::TrilinearMeshRasterizer` and does not yet require voice
chaining.

Graphic render-state, scaling, batch, and publication behavior likewise lives
in `Rasterization::GraphicRasterizer`. The Cycle adapter retains application
settings, scratch position, updater detail state, and interactor access.
Time-column traversal and layer summation now consume explicit shared layer
values; Cycle retains only `MeshLibrary` translation, view-stage selection, and
scratch-property lookup.
E3 envelope grid traversal and sampling now live in
`Rasterization::EnvelopeGridRasterizer`; Cycle retains UI sizing, storage,
locking, mesh selection, and update dispatch.
The remaining Cycle `GraphicMorphPositionPolicy` is intentionally an adapter:
its inputs and decisions are Cycle layer groups, settings, panel morph state,
and scratch-channel state rather than reusable rasterization behavior.

Suggested direction:

- Use the shared voice façade when Cycle 2 introduces oscillator/voice-cycle
  execution; keep immutable configuration publication separate from mutable
  per-voice `VoiceCycleState`.
- Keep Cycle 2 node DSP modules under `cycle-v2/src/Nodes/Trimesh/`.
- Preserve separate blockwise audio and gridwise UI/update surfaces, matching
  the FFT folder pattern.
- Let the node model own mesh data and pass explicit mesh/context references to
  DSP and widget modules instead of reaching through `MeshLibrary`.

## Cycle 2 OpenGL Cable Tessellation

The prototype GL cable renderer exposed platform-dependent artifacts with wide
`GL_LINE_STRIP` strokes and hand-built triangle strips: square cutouts on
curves, disappearing vertical sections, and poor selected-line readability.
Cycle 2 currently keeps cables on the JUCE path renderer while the background
remains OpenGL-backed.

Suggested direction:

- Re-enable GL cables only after adding a real stroked-path tessellator that
  emits joins, caps, and dash runs as explicit geometry.
- Treat selection as a separate narrow highlight stroke rather than by making
  the halo heavily opaque.
- Keep node shells and node contents in the same render layer unless the whole
  node widget moves to GL, because split shell/content rendering breaks
  overlap z-order.

## Trilinear Mesh Intercept Ownership

`TrilinearMeshRasterizer` currently keeps its update-geometry intercept output
in `meshIntercepts` and publishes that into the rasterizer snapshot, while the
base rasterizer also has `rasterizerData.intercepts`. Refactor this to a single
authoritative intercept store so UI overlays, interactors, and waveform baking
cannot accidentally read different intercept sources.
