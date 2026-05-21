# Node Graph Workflow TDD

## Overview

This document describes the technical design for Cycle's node graph workflow.

It implements [ADR 008](../ADR/008-node-graph-workflow.md).

The feature is intended for Cycle 2.0: a sister project to Cycle 1.x that can
reuse core processing code, preset concepts, and mesh data while leaving the
legacy multi-panel UI behind. Cycle 2.0 is centered on a full-window,
OpenGL-rendered node editor where signal flow is represented as a typed
processing graph.

Because this is a new major-version project rather than an in-place UI mode,
the design does not need to preserve the legacy workspace as an active editing
surface. Compatibility matters at the data and DSP level, not at the legacy UI
component level.

## Problem Statement

Cycle's current workflow is powerful but fixed: waveform layers, optional
spectral processing, inverse FFT, and effects are arranged by application
structure rather than by the user's patch.

The desired workflow is a spacious node canvas where users can freely compose
time-domain, spectral-domain, mesh, envelope, and effects processors. A patch
should be able to express flows such as:

- waveform source to FFT to spectral filters to inverse FFT to waveshaper,
- time-domain signal to impulse response to FFT to spectral shaping,
- envelope output multiplied into a time-domain signal,
- mesh source reused as a waveform, spectral filter, or waveshaper transfer
  curve,
- repeated time/spectrum/time transitions.

The main challenges are:

- enforcing signal-domain grammar at edges,
- making currently occluded cross-assignments visible and traceable,
- keeping audio execution realtime safe,
- keeping node previews live as upstream content changes,
- reusing the correct Cycle 1.x processing semantics without carrying the old
  UI architecture forward,
- giving rich mesh and signal editors enough space,
- making stereo behavior clear without overwhelming the graph.

## Goals

- Build Cycle 2.0 around a spacious node editor instead of the legacy
  multi-panel workspace.
- Render the node editor entirely through one OpenGL canvas.
- Implement node-editor widgets as OpenGL path drawing or texture-baked
  sprites.
- Allow JUCE `Graphics` drawing to be baked into textures for programmatic
  sprites and text-heavy controls.
- Represent Cycle processing as a typed graph of nodes, ports, and edges.
- Represent implicit assignments, such as scratch-channel-to-layer bindings, as
  visible graph structure.
- Compile valid graphs into realtime-safe audio and preview execution plans.
- Build a graph-driven update dependency graph from processing graph connections.
- Give every node a generous live preview of its internal signal, mesh, or
  processor state.
- Expand nodes into detailed editors on double-click.
- Default applicable processors to linked stereo operation on left and right
  channels.
- Support explicit split-stereo workflows without making every simple patch
  visually noisy.

## Non-Goals

- Recreating the Cycle 1.x UI layout or panel hierarchy.
- Exposing arbitrary third-party node plugins.
- Implementing a new GPU backend beyond the existing OpenGL path in the first
  milestone.
- Making the node canvas a collection of JUCE child components.
- Preserving the legacy workspace as an alternate editing mode inside Cycle 2.0.
- Allowing dynamic graph recompilation on the audio thread.

## Current Architecture

### Fixed Processing And Update Flow

Cycle 1.x uses `CycleUpdater` to build an `Updater::Graph` of
`Updateable` objects. This graph controls invalidation and processing order for
waveform, spectrum, envelope, guide-curve, scratch, effect, and visual DSP
updates.

The current `Updater::Graph` is an application update dependency graph, not a
user-authored processing graph. Cycle 2.0 should not migrate this graph
wholesale. It should preserve the important dependency semantics by expressing
them as `ProcessingNodeGraph`, `NodeUpdateGraph`, and compiled execution-plan
dependencies.

Important existing concepts:

- `CycleUpdater`: builds and mutates the current fixed update graph.
- `Updater`: queues update sources and executes `Updater::Graph`.
- `Updateable`: target interface for editor, rasterizer, and visual DSP
  invalidation.
- `VisualDsp`: owns current visual DSP processing paths.
- `GraphicRasterizer`, `VoiceMeshRasterizer`, `EnvRasterizer`, `FXRasterizer`,
  and `E3Rasterizer`: existing rasterizer owners that should be adapted before
  they are replaced.
- `Waveform2D`, `Waveform3D`, `Spectrum2D`, `Spectrum3D`, `Envelope2D`, and
  `Envelope3D`: existing rich editors that define interaction behavior and
  visual affordances Cycle 2.0 can reuse conceptually, without embedding these
  components directly.

### Existing Rendering Constraints

Cycle 1.x already has substantial JUCE painting and OpenGL-backed panel
rendering. Cycle 2.0 should not compose many JUCE widgets over the canvas. It
should own one OpenGL component and draw the whole workspace itself.

JUCE `Graphics` remains useful for programmatically producing sprites, labels,
icons, and complex widget textures. Those images should be baked into GL
textures and invalidated only when their inputs change.

## Target Architecture

The target design separates three graph-like systems:

- `ProcessingNodeGraph`: the user-authored patch of DSP, mesh, transform,
  envelope, and effect nodes.
- `GraphExecutionPlan`: the compiled realtime-safe audio and preview plan.
- `NodeUpdateGraph`: a graph-driven invalidation dependency graph built from
  `ProcessingNodeGraph` connections.

Cycle 1.x `Updater::Graph` remains useful as a behavioral reference, but it is
not part of the Cycle 2.0 runtime architecture.

### High-Level Ownership

```text
Cycle2Application / PluginEditor
  -> NodeWorkspace
    -> NodeCanvasOpenGLComponent
      -> NodeCanvasRenderer
      -> NodeInputRouter
      -> NodeSpriteCache
      -> NodeWidgetRegistry
    -> ProcessingNodeGraph
    -> GraphCompiler
    -> GraphExecutionPlan
    -> NodeUpdateGraph
```

### Cycle 1.x Reuse Boundary

Cycle 2.0 may reuse:

- mesh, envelope, preset, and sample data models where they remain compatible,
- DSP algorithms and rasterization policies after they are detached from legacy
  UI ownership,
- characterization tests and fixtures that describe current audio or visual
  behavior,
- preset importers and compatibility builders.

Cycle 2.0 should not reuse:

- the legacy panel hierarchy as a live UI dependency,
- `CycleUpdater` as the runtime dependency graph,
- hidden layer, scratch, guide-curve, or pitch assignments without representing
  them explicitly in graph state.

## Processing Graph Model

### Domains

Ports have a signal domain and a channel layout.

Initial domains:

- `TimeSignal`
- `SpectralMagnitudeSignal`
- `SpectralPhaseSignal`
- `MeshField`
- `EnvelopeSignal`
- `PitchSignal`
- `VoiceControlSignal`
- `ControlSignal`

`EnvelopeSignal` is separate from `ControlSignal` because envelopes are
time-varying signal-rate or block-rate curves that commonly multiply audio.

`PitchSignal` is separate from generic controls because pitch can be global,
per-voice, or voice-context dependent. `VoiceControlSignal` represents
voice-scoped control data such as unison voice index, per-voice detune, spread,
pan, and phase. The first implementation should model Cycle 1.x unison
semantics before introducing a broader voice-lane architecture.

General controls may be scalar, stepped, or event-like.

Source domain is established by graph context, not by permanently naming a
source node after one signal type. The first implementation should use a
`VoiceContext` or equivalent voice/domain context node to choose whether a
voice-aware source begins as waveform, spectral magnitude, spectral phase, or
another supported source mode. That context may connect to a source node's
domain/control port. A source such as a trilinear mesh or image then emits the
domain implied by that context and its downstream connection.

This keeps source nodes domain-agnostic where possible. A trilinear mesh is a
mesh operand/source with Yellow, Red, and Blue axes; it is not inherently a
"wave surface" or "spectral surface", and it should not expose hardcoded
magnitude or phase outputs. FFT and IFFT nodes are explicit domain transforms.
Fixed-domain sources remain fixed: a `Wave` source is time-domain only. If it is
connected to a `VoiceContext` that later switches to spectral mode, the edge
should enter an invalid/error state and become valid again when the context
returns to waveform mode.

Mesh nodes are source nodes. They do not expose an ordinary signal input and do
not process a signal through themselves. A mesh can be used as an operand in
math nodes or as a direct generator in the traversal domain implied by its
context or consuming edge.

Resolved signal type is a property of traversal context, not of a mesh node's
persistent schema. A mesh node may cache its currently resolved traversal domain
for preview or DSP preparation, but that cache is derived state and must be
invalidated when upstream context, consuming operation, or graph topology
changes. The graph source of truth remains node schema plus typed edges.

### Explicit Assignments

Some of Cycle's current behavior comes from assignments that are hard to see in
the UI. Scratch channels, guide curves, and other mesh-derived modifiers may
apply to one mesh layer, a subset of layers, or frequency-domain layers without
that routing being represented as a visible processing connection.

The node graph should make these relationships first-class. Scratch behavior is
attachment-first: there is no separate first-class scratch-envelope signal
domain. An ordinary envelope node can attach to a dedicated scratch port or
scratch sub-target on a mesh-backed layer. If scratch data needs to become an
ordinary signal, an explicit adapter node must perform that conversion.

A hidden assignment in the legacy model should become one of:

- a `ProcessingAttachment` when an envelope, guide curve, or modifier is bound
  to a specific node, layer, port, or sub-target,
- an ordinary typed edge only when the relationship is true signal flow,
- a small assignment node when the binding needs parameters, filtering, or
  fan-out control.

Assignments are graph-visible and inspectable, but may use a lighter visual
treatment than audio-rate signal edges. They still participate in validation,
serialization, dependency invalidation, and diagnostics.

Initial assignment cases:

- envelope node attached to one mesh layer's scratch port,
- envelope node attached to a dedicated scratch port on a time-axis structure,
- envelope node attached to selected spectral magnitude or phase layer scratch
  ports,
- guide curve to mesh-backed source or processor node,
- envelope or mesh modifier assigned to one specific node instance rather than
  the whole patch.

The editor should make the target scope clear. A scratch channel attached to one
layer must not look like it affects all layers in that node. The graph model
therefore needs stable sub-target identities for layer instances, not only node
ids.

Time-axis structures that currently receive scratch behavior implicitly should
expose dedicated scratch ports or attachable scratch sub-targets. This includes
waveform surface layers, spectral magnitude layers, and spectral phase layers.
Ordinary envelope inputs should remain distinct from scratch ports so that a
global volume envelope and an envelope attached for scratch behavior cannot be
confused in the graph.

### Channel Layouts

Initial channel layouts:

- `Mono`
- `LinkedStereo`
- `Left`
- `Right`
- `StereoPair`

Default behavior:

- audio processors expose linked stereo ports by default,
- if an operation is applicable to both channels, the same operation is applied
  to left and right,
- mono sources may feed linked stereo processors by duplication when the
  destination explicitly allows it,
- spectral processors inherit the channel layout of their audio input unless
  they declare otherwise.

Split-stereo behavior:

- the primary explicit workflow is `ChannelSplitter` and `ChannelMerger` nodes,
- a node-local "split stereo" toggle may be added later as a UI shortcut, but
  it should materialize the same underlying split/merge representation,
- split ports are shown only when a node or explicit splitter is in split mode,
- edge color should continue to communicate domain first; channel state should
  be shown with lane markers, port labels, or edge striping instead of replacing
  the domain color.

### Port Compatibility

Connections are valid when all of the following match:

- source and destination domains are compatible,
- channel layout can be passed or adapted without ambiguity,
- the destination cardinality allows the connection,
- graph direction does not create an illegal processing cycle.

Examples:

- `TimeSignal LinkedStereo` to `TimeSignal LinkedStereo`: valid.
- `TimeSignal LinkedStereo` to FFT input: valid.
- FFT magnitude output to inverse FFT magnitude input: valid.
- FFT phase output to inverse FFT phase input: valid.
- `SpectralMagnitudeSignal` directly to `TimeSignal`: invalid.
- `SpectralMagnitudeSignal` and `TimeSignal` into an ordinary add node:
  invalid because the operand domains cannot be unified.
- two `SpectralMagnitudeSignal` operands into add or multiply: valid.
- `TimeSignal` plus a context-resolved mesh source in time mode: valid.
- `SpectralMagnitudeSignal` plus a context-resolved mesh source in spectral
  magnitude mode: valid.
- a domain/context port into a trilinear mesh source: valid when the source is
  voice/domain aware.
- `EnvelopeSignal` to multiply node factor input: valid.
- `EnvelopeSignal` to a mesh layer scratch port: valid as a
  `ProcessingAttachment`.
- A scratch-port attachment to an ordinary multiply factor input: invalid unless
  an explicit adapter converts that scratch interpretation back into ordinary
  envelope or control signal flow.
- `PitchSignal` to a voice-aware pitch input: valid.
- `PitchSignal` to a non-voice-aware audio processor: invalid.
- `Left` plus `Right` to `StereoPair`: valid through `ChannelMerger`.

### Core Node Families

The first graph implementation should define node schemas for:

- voice/domain context nodes that define voice count, unison-compatible
  controls, pitch/control routing, and initial source domain,
- source nodes backed by 2D mesh, trilinear mesh, image, or simpler waveform
  generators,
- domain-agnostic mesh source nodes whose edge context determines whether they
  currently generate time, spectral magnitude, spectral phase, or another
  compatible signal domain,
- FFT and inverse FFT transform nodes,
- multiply, sum, gain, and clamp utility nodes,
- envelope source nodes,
- voice and pitch control nodes,
- channel splitter and merger nodes,
- spy/monitor nodes for explicit non-mutating signal visualization,
- existing effects: unison, impulse modeller, waveshaper, convolution reverb,
  EQ, and delay with pan,
- output node.

Node implementation code should be grouped by node domain where a node has
multiple runtime surfaces. For FFT/IFFT, `cycle-v2/src/Nodes/FFT/` owns the
cycle transform backing:

- `FftBlockwiseDsp`: serial audio/render-block execution for one power-of-two
  Cycle period at a time,
- `FftGridwiseDsp`: UI/update-time execution across many independent cycle
  columns for spectrogram and grid previews,
- future `FftNode` schema/controller code can live beside those DSP classes
  once node-specific content ownership moves out of the generic factory.

Both DSP surfaces share the same cycle transform semantics. They do not use
Blackman-Harris or other analysis windows; Cycle periods are already periodic
power-of-two buffers, matching Cycle 1.x.

The audio-side blockwise inverse path applies the Cycle 1.x half-cycle carry
crossfade for continuity between adjacent generated cycles. The first cycle is
left raw; subsequent cycles fade their first half in with the sine half-ramp
and fade the previous carried half out with the complementary ramp. Gridwise UI
processing does not apply this carry because each displayed cycle column is an
independent analysis/rendering result.

Spy/monitor nodes are the preferred home for rich signal visualization such as
spectrograms, unwrapped phasigrams, cyclograms, waveform scopes, meters, and
vectorscopes. FFT and IFFT nodes remain compact transform nodes rather than
owning expanded preview popups. A spy node compiles as a non-mutating
pass-through/tap: its output domain and channel layout match its input exactly,
and its available views are constrained by the signal type flowing through it.

The first implementation should model spies as ordinary graph nodes. Adding a
spy to a selected edge should be command sugar that replaces `A -> B` with
`A -> Spy -> B`, preserving edge grammar, undo, selection, serialization,
minimap participation, and compilation semantics. A true edge-owned attachment
can be considered later as a presentation layer, but should not complicate the
initial edge model.

Domain and operation naming should stay separate. Generic operations are named
by what they do (`Add`, `Multiply`, `Clamp`, `Mesh`, `Image`, `Wave`) rather
than by the domain currently flowing through them. The edge domain and compiler
resolution decide whether a given operation instance is acting on time-domain
audio, spectral magnitude, spectral phase, control, or another compatible data
stream.

The global volume envelope is represented as ordinary graph structure:

```text
TimeSignal source -> Multiply.audio -> Output
Envelope node     -> Multiply.factor
```

This replaces hidden global volume-envelope wiring with visible graph semantics.

Scratch and guide-curve behavior should follow the same rule. If a scratch
channel modifies only one layer of a spectral node, the graph should show that
targeted attachment explicitly instead of implying node-wide influence.

Pitch should not be forced into only one global singleton port. The legacy graph
may use a global `Voice` or `VoiceContext` node to represent current behavior,
but pitch should apply only inside voice-aware generators and processors.
Generators must prime or fulfill enough content for zero-latency downstream
output after pitch application.

Cycle 1.x unison should be the baseline for the first Cycle 2.0 unison node.
Current unison stores per-voice pan, phase, and detune/fine values and the
visual frequency path combines unison with pitch-envelope sampling by
phase-shifting and summing per-voice columns. The first node implementation
should reproduce that behavior before generalizing unison into arbitrary
fan-out/fan-in voice lanes.

### Cycle Frame Model

Cycle graph execution is cycle-oriented. A cycle buffer is a power-of-two
period, matching Cycle 1.x oscillator and FFT processing.

FFT nodes always consume one cycle of `TimeSignal` content and produce one
cycle worth of spectral magnitude and phase content. They should not introduce
latency drift.

Inverse FFT nodes consume one cycle worth of spectral magnitude and phase
content and produce one cycle of `TimeSignal` content. The default inverse FFT
mode is cyclic. Optional overlap modes may be added:

- cyclic overlap, which wraps within the current cycle,
- acyclic overlap, which may incur latency and should use an explicit one-cycle
  carry buffer to sum material into the next cycle.

Any node that introduces acyclic carry or latency must declare that latency in
its node schema and compiled execution plan. Downstream zero-latency paths
should remain the default.

## Node Editor UI

### Visual Direction

Cycle 2.0 should feel meaningfully distinct from Cycle 1.x. The node editor is
not a rearranged panel UI; it is a studio-canvas workspace built around signal
flow, embedded visualizations, and readable routing.

Visual direction:

- spacious dark canvas with a subtle technical grid,
- minimap for large patch navigation,
- soft, physical-feeling cables that route around node bodies and avoid
  covering previews or labels,
- domain-colored ports and edges, with channel state shown as lane markers or
  double-line treatments,
- lighter dashed attachment routing for scratch, guide-curve, and other
  non-audio assignments,
- generous node previews as primary content, not decorative thumbnails,
- visualizations that preserve Cycle's identity, especially spectrograms and
  cyclogram-style cycle views where useful,
- compact OpenGL-drawn widgets and texture-baked labels instead of legacy JUCE
  panel chrome.

Cable routing should be treated as a core interaction feature. Cables should
look soft and organic, but their paths should be intelligent: avoid node
bodies, preserve socket readability, reduce crossings where possible, and keep
invalid or tentative connections visually distinct from committed graph edges.

### Canvas

The node editor is one OpenGL-backed canvas that owns:

- pan and zoom,
- minimap navigation,
- node layout,
- edge routing,
- selection,
- drag and connection gestures,
- keyboard focus,
- command palette or menus,
- inline node controls,
- expanded node editors,
- tooltips and validation feedback.

The canvas should feel spacious. Nodes should be large enough to show useful
state, not only names and ports.

Implementation rules:

- do not use JUCE child widgets inside the node canvas for normal controls,
- draw widget geometry directly with OpenGL path rendering or cached meshes,
- bake JUCE `Graphics` output into textures for text, icons, complex labels,
  and reusable sprites,
- keep sprite cache keys explicit: text, font, scale, colors, widget state,
  and DPI scale,
- keep GL resource creation and texture upload off the audio thread.

### Node Visuals

Previewable nodes own a node UI module. The same module paints both the compact
in-node view and the expanded editor view, with a paint mode deciding how much
detail is shown. The compact preview is therefore not a separate data model from
the expanded editor; it is the same node widget drawing a smaller, simplified
view over the same node/UI state.

The renderer owns canvas-level concerns such as clipping, transforms, zoom
scale, node bounds, cable drawing, selection, and popup placement. Node widgets
own node-specific body/editor drawing and interaction. Preview or editor data
may live outside the widget, but the widget should receive direct references to
the node model, UI-side analysis state, and any cached visual-DSP output it
needs during update and paint. Avoid normalizing every node's visual state into
a single `NodePreviewResult` or other generic preview DTO.

Example ownership:

```text
Nodes/FFT/
  FftNode
  FftBlockwiseDsp
  FftGridwiseDsp
```

`FftBlockwiseDsp` serves serial audio/render-block processing.
`FftGridwiseDsp` serves UI/update-time analysis across independent cycle
columns. Spy/monitor widgets can consume gridwise DSP output directly for
spectrogram or phasigram views; they should not have to translate through a
universal preview payload first.

Each previewable node's compact view must update when upstream content that
affects the node changes.

Preview defaults:

- trilinear mesh nodes show a live 3D surface preview,
- 2D mesh nodes show the active slice or curve graph,
- spy/monitor nodes show spectrogram, phasigram, cyclogram, waveform, meter, or
  vector views according to the tapped signal type,
- source nodes show the material they generate or expose; a trilinear mesh
  preview should use Yellow, Red, and Blue axes rather than pitch/morph/phase
  labels,
- image source nodes show an image/raster preview when image data is present,
- waveshaper nodes show transfer curve plus input/output preview,
- convolution and impulse nodes show impulse response and wet/dry output
  summary,
- EQ nodes show the EQ curve,
- delay nodes show taps, feedback, and pan summary,
- envelope nodes show Cycle-style point-curve envelopes with loop or sustain
  markers where relevant,
- FFT, inverse FFT, arithmetic, channel split/join, and output nodes are compact
  non-preview nodes identified through iconography and port topology rather
  than oversized placeholder preview regions.

Generated mockups are inspirational visual references, not literal node-schema
specifications. Controls shown in mockups should be treated as examples unless
they match a defined node parameter. The visual style choices that should carry
forward are the studio-canvas grid, rich but contained node previews, compact
header controls, clear domain-colored ports, and cable-ready port affordances.

Node widgets should be backed by the same rasterizer or processing adapter used
by graph execution where feasible. If a cheaper UI/update path is needed, it
must be explicitly marked as preview-only and covered by tests that keep it
semantically aligned.

The expanded editor panels should visually match Cycle 1.x panel baselines
closely enough that the new node workspace feels like a new shell around the
same high-quality editor language, not a simplified redraw. Reference images
live under `docs/media/images/cycle-panel-baselines/`:

- `waveform2d-bongo.png` for Waveform2D-style translucent surface bands,
  dense grid, mesh/vertex overlay, and layered curve history,
- `spectrum2d-bongo-magnitude.png` and `spectrum2d-bongo-phase.png` for
  Spectrum2D magnitude/phase density, bar/curve overlays, logarithmic-feeling
  guides, and guide-curve lines,
- `waveform3d-bongo.png` and `spectrum3d-bongo-*.png` for the 3D panel camera,
  depth bands, grid planes, and mesh surface overlays,
- `waveshaper-fx-bongo.png` for FX curve panels,
- Cycle 1.x morph panel references for morph rails, cube-position preview, axis
  selectors, cube-range controls, and link toggles.

Placeholder polylines, low-resolution approximations, and hand-drawn preview
curves are acceptable only as short-lived scaffolding. The real implementation
should use the Cycle rasterizer/curve data path (`Curve`, mesh rasterizers,
gridwise DSP, or faithful adapters around them) so displayed curves, surfaces,
vertices, rails, guide overlays, and spectra are computed from the same model
that drives DSP.

### Expanded Editors

Double-clicking a node expands the relevant editor in the node workspace.

Expanded editor behavior:

- the editor opens in-place as a large canvas region or modal workspace panel,
- the graph remains visible enough to preserve context where practical,
- editing changes are applied to the node model and propagate through
  `NodeUpdateGraph`,
- closing the expanded editor returns to the normal node canvas without
  rebuilding the whole graph unless the node schema changed.

Trilinear mesh expanded editor:

- switchable 3D view and 2D slice view,
- trilinear position widget using Yellow, Red, and Blue axes,
- current mesh/layer controls required for that node,
- live preview of downstream impact through invalidated dependent nodes.

Trilinear mesh implementation plan:

- create a dedicated trilinear mesh node model that owns or references mesh
  data, Yellow/Red/Blue morph position, primary view axis, selected
  cube/vertex state, scratch and guide attachment targets, and mesh-specific
  dirty revisions,
- keep source-domain resolution derived from graph traversal context; a mesh
  node may cache its resolved domain for preview or DSP preparation, but the
  cache is not persistent node schema,
- keep blockwise realtime DSP and gridwise UI DSP as separate backing modules
  under the trilinear mesh node folder,
- use blockwise DSP for serial cycle rendering and audio execution, preserving
  Cycle-style power-of-two cycle processing and realtime allocation rules,
- use gridwise DSP for UI columns, 3D heatmaps, 2D slices, mesh overlays, and
  reduced-detail previews,
- make gridwise UI rendering reproduce the Cycle 1.x panel aesthetic: heatmap
  and translucent surface bands, dense but subdued grid lines, vertex/cube
  overlays, guide-curve overlays, and high-resolution Curve/Rasterizer-derived
  traces rather than coarse placeholder polylines,
- treat Cycle 1.x `Waveform2D`, `Spectrum2D`, `WaveformInter2D`,
  `SpectrumInter2D`, `Waveform3D`, `Spectrum3D`, `WaveformInter3D`,
  `SpectrumInter3D`, `GraphicRasterizer`, and `VisualDsp` as parity references
  rather than classes to embed wholesale,
- build a dedicated trilinear mesh widget/editor module; `NodeCanvas` should
  own canvas-level transforms, routing, clipping, and popup placement, while
  the mesh widget owns compact preview drawing, expanded editor drawing, mesh
  hit-testing, and mesh-specific interaction,
- organize the expanded editor as a 3D grid/heatmap panel beside a morph and
  vertex-parameter panel, with the 2D waveform or spectrum slice editor taking
  the full width below,
- render the morph panel and vertex parameters with Cycle 1.x semantics but
  with more latitude in layout; expected parameters include amplitude, phase,
  sharpness, and component curve controls for selected mesh elements,
- treat the current Cycle 2 implementation of `selectedVertexIndex` plus
  `vertex.*` override parameters as a temporary interaction scaffold only; the
  final design needs serialized mesh-state edits so multiple vertices/cubes can
  be edited, undone, copied, and loaded without relying on a single selected
  vertex override,
- make all mesh edits undoable and route mesh, morph, preview camera, and
  attachment changes through `NodeUpdateGraph` invalidation.

2D mesh expanded editor:

- curve/slice editor,
- layer controls required for that node,
- preview of sampled waveform, spectral filter, envelope, or transfer curve
  depending on the receiving role.

Effect expanded editors should reuse current effect UI semantics but render
their controls as GL widgets or baked sprites inside the node workspace.

## Graph-Driven Updates

### NodeUpdateGraph

`NodeUpdateGraph` is built from the processing graph and additional preview
dependencies.

Responsibilities:

- mark downstream processing nodes dirty when an upstream node changes,
- mark node previews dirty when their input signal, mesh, or parameters change,
- schedule low-detail and full-detail preview refreshes during interaction,
- notify `GraphCompiler` when topology or schema changes require recompilation,
- avoid recompilation for parameter-only changes when the compiled node can
  update prepared state safely outside the audio thread.

The graph should distinguish invalidation kinds:

- topology changed,
- node parameters changed,
- node mesh content changed,
- upstream signal changed,
- preview camera or viewport changed,
- channel layout changed,
- domain or port schema changed.

Cycle 1.x update sources should be mapped into these invalidation kinds only as
compatibility input. Cycle 2.0 should not preserve a separate fixed-flow update
mode.

### Preview Update Policy

Previews are visible-editor artifacts and should not block audio.

Rules:

- visible nodes get highest preview priority,
- expanded node editor preview gets highest visual detail,
- offscreen nodes may update summary caches lazily,
- interaction can request reduced-detail previews,
- full-detail previews restore after the interaction settles,
- preview caches include the graph revision and relevant upstream revisions.

## Runtime Execution

### Compilation

`GraphCompiler` turns a valid `ProcessingNodeGraph` into `GraphExecutionPlan`.

Compilation responsibilities:

- topologically sort executable processing nodes,
- validate domain and channel compatibility,
- insert explicit adapters only where graph rules allow them,
- allocate intermediate buffers,
- bind node processors to prepared state,
- emit diagnostics for invalid or incomplete graphs,
- produce a preview execution plan for selected or visible nodes when needed.

Compilation must happen off the audio thread. The audio thread receives an
immutable plan snapshot through an atomic or lock-free handoff pattern.

### Execution

`GraphExecutionPlan` owns or references:

- ordered processing steps,
- prepared node state,
- typed intermediate buffers,
- channel layout metadata,
- FFT work buffers,
- spectrum wrappers from ADR 006,
- output routing.

Realtime rules:

- no allocation on the audio thread,
- no graph traversal decisions on the audio thread beyond executing the plan,
- no access to JUCE components from audio execution,
- parameter changes use prepared state or lock-free snapshots,
- topology changes compile a new plan before publication.

## Serialization And Compatibility

Graph presets need stable identity separate from visual layout.

Persist:

- graph format version,
- node instance id,
- node kind id,
- node parameter state,
- port ids,
- edge ids and endpoints,
- attachment ids, endpoints, and target sub-target ids,
- channel layout mode,
- FFT/IFFT cycle-frame and overlap mode,
- editor layout position and expanded/collapsed state,
- preview/editor view state that is safe to store with the preset.

Do not persist:

- GL texture ids,
- baked sprite cache entries,
- transient preview buffers,
- compiled execution plan internals,
- raw pointers to legacy panels or rasterizers.

Cycle 1.x presets should import through a compatibility layer until a native
Cycle 2.0 graph preset format is ready. `LegacyCycleGraphBuilder` creates a
graph equivalent to the current patch for inspection, testing, and migration.

## Proposed Type Responsibilities

Names may change during implementation, but the following responsibilities
should remain stable.

- `ProcessingNodeGraph`: persistent user patch model.
- `ProcessingNode`: node instance id, kind id, parameter block, port state, and
  visual layout state.
- `ProcessingPort`: port id, domain, channel layout, direction, cardinality,
  and display metadata.
- `ProcessingEdge`: source port, destination port, domain, channel metadata,
  and validation status.
- `ProcessingAttachment`: visible non-audio assignment from a source node or
  port to a node, layer, or parameter sub-target.
- `GraphSubTarget`: stable identity for attachable internals such as mesh
  layers, spectral layers, scratch slots, guide-curve inputs, and effect
  parameters.
- `NodeKindRegistry`: schemas, factories, display names, default parameters,
  attachable sub-targets, and preview/editor capabilities.
- `GraphValidator`: domain, scratch-port attachment, pitch/voice-scope,
  channel, cardinality, cycle, and required-input checks.
- `GraphCompiler`: plan builder and diagnostics producer.
- `GraphExecutionPlan`: immutable realtime plan.
- `NodeUpdateGraph`: graph-driven invalidation and preview scheduling.
- `NodeWorkspace`: top-level workspace component and mode owner.
- `NodeCanvasOpenGLComponent`: single GL surface for the node editor.
- `NodeCanvasRenderer`: draws canvas, nodes, edges, selection, clipping, popup
  placement, and delegates node bodies and expanded editors to node widgets.
- `NodeSpriteCache`: texture-baked JUCE `Graphics` sprites and labels.
- `NodeWidgetRegistry`: resolves each previewable/editable node to a UI module
  that can paint compact and expanded modes.
- `NodeWidget`: node-specific UI module for body preview, expanded editor
  drawing, and node-local interaction.

## Progressive Milestones

### Milestone 1: Cycle 2.0 Project Shell

Goal: create the new application/plugin target without depending on the legacy
Cycle 1.x panel hierarchy.

Acceptance:

- Cycle 2.0 builds as a sister target or project alongside Cycle 1.x,
- the app/plugin opens a full-window OpenGL workspace,
- shared processing, mesh, preset, and utility code can be linked intentionally,
- no legacy panel component is required for the initial editor window,
- smoke automation can launch Cycle 2.0 and capture the blank node workspace.

### Milestone 2: Graph Model And Validation

Goal: represent a typed graph independent from UI and audio execution.

Acceptance:

- create nodes, ports, and edges in memory,
- create attachment edges to stable node sub-targets,
- validate compatible and incompatible domain connections,
- validate assignment scope for scratch, guide-curve, and layer-targeted
  bindings,
- validate that scratch behavior is represented as `EnvelopeSignal`
  attachments to scratch ports or scratch sub-targets, not as a separate
  scratch-envelope domain,
- validate that `PitchSignal` and `VoiceControlSignal` connect only to
  voice-aware nodes or explicit conversion nodes,
- validate FFT/IFFT cycle-frame and overlap-mode schema,
- validate source-domain resolution from `VoiceContext` or equivalent context
  ports into mesh, image, and waveform sources,
- validate that domain-agnostic source/filter nodes do not expose hardcoded
  magnitude or phase outputs unless their schema is specifically a transform
  such as FFT,
- validate linked stereo, split left/right, and mono-to-linked-stereo cases,
- reject illegal direct time/spectral connections,
- serialize and deserialize graph identity, layout, edges, attachments, and
  sub-target ids,
- unit tests cover graph validation diagnostics.

### Milestone 3: Minimal Compiled Runtime

Goal: execute a small validated graph without depending on UI objects.

Acceptance:

- compile a valid graph into an immutable `GraphExecutionPlan`,
- execute simple time-domain source to output graph,
- execute waveform to FFT to inverse FFT to output round trip,
- execute envelope to multiply to output,
- execute linked stereo processors on both channels by default,
- execute split/merge stereo graph explicitly,
- prove no audio-thread allocation in representative execution tests.

### Milestone 4: Cycle 1.x Compatibility Graph

Goal: express current Cycle 1.x fixed-flow behavior as an inspectable graph.

Acceptance:

- `LegacyCycleGraphBuilder` emits waveform, FFT, spectral magnitude/phase,
  inverse FFT, effect-chain, envelope, and output nodes matching current
  behavior,
- hidden scratch, guide-curve, and layer-specific assignments are emitted as
  visible attachments or assignment nodes,
- pitch and unison voice behavior is represented as explicit voice or pitch
  control structure using current Cycle 1.x semantics as the baseline,
- generated graph validates cleanly,
- generated graph stores enough layout metadata to be readable in the future
  editor,
- tests compare generated graph order against current fixed flow assumptions.

### Milestone 5: OpenGL Node Canvas Shell

Goal: show and interact with the Cycle 2.0 node canvas.

Acceptance:

- one OpenGL component fills the editing workspace,
- pan, zoom, node selection, node dragging, and edge dragging work,
- subtle grid and minimap render correctly at multiple zoom levels,
- nodes and edges are drawn without JUCE child controls,
- cable routing avoids node bodies and keeps ports, labels, and previews
  readable in representative patches,
- attachment cables have a distinct lighter routed style from signal cables,
- sprite-baked labels/icons render correctly at DPI scale,
- compact utility nodes render without fake preview regions,
- invalid edge attempts show visible feedback,
- automation can capture a screenshot of the node workspace.

### Milestone 6: Live Node Previews

Goal: nodes show useful, updating content.

Acceptance:

- waveform, spectrum, envelope, mesh, image, and previewable effect nodes
  display previews,
- trilinear mesh nodes display a surface preview,
- 2D mesh nodes display a curve or slice preview,
- compact non-preview nodes remain visually identifiable through iconography,
- spectrogram, phasigram, and cyclogram-style preview experiments exist on
  spy/monitor nodes,
- upstream parameter or mesh edits invalidate downstream previews,
- reduced-detail previews are used during interaction,
- full-detail previews restore after interaction settles.

### Milestone 7: Expanded Node Editors

Goal: double-clicking a node opens a rich editor in graph mode.

Acceptance:

- trilinear mesh node expands to 3D and 2D editor modes,
- trilinear morph position widget edits that node's preview/execution state,
- trilinear mesh editor uses a dedicated node widget/editor rather than
  `NodeCanvas`-owned preview special cases,
- 3D heatmap, 2D slice, morph panel, and vertex-parameter panel are visible in
  the expanded trilinear editor,
- vertex/cube selection and mesh edits are undoable,
- 2D mesh nodes expand to a curve/slice editor,
- effect nodes expose their current controls as GL widgets or sprites,
- edits propagate through `NodeUpdateGraph`,
- closing the editor preserves node state and canvas position.

### Milestone 8: Adapter Nodes For Existing Cycle Processing

Goal: move real Cycle processors behind node schemas.

Acceptance:

- adapter nodes exist for existing mesh-backed waveform and spectral paths,
- adapter nodes exist for global envelope, waveshaper, impulse modeller,
  convolution reverb, EQ, delay, and output,
- generated legacy graph can use adapter nodes,
- audio and preview tests compare representative outputs against the fixed
  flow.

### Milestone 9: Cycle 2.0 Presets And User Editing

Goal: make graph patches persist and become usable as a workflow.

Acceptance:

- save/load graph presets with stable ids and layout,
- import Cycle 1.x presets without data loss where supported,
- generate a node graph from a Cycle 1.x preset for inspection,
- create, delete, connect, disconnect, duplicate, and rename nodes,
- graph diagnostics are visible and actionable,
- automation fixtures cover common editing flows.

## Test Plan

### Unit Tests

- graph validation for domain mismatch, missing inputs, cycles, and cardinality,
- assignment validation for scratch-channel and guide-curve bindings scoped to
  specific layers or node instances,
- scratch-port attachment validation for waveform surface layers, spectral
  magnitude layers, and spectral phase layers,
- pitch and voice-control validation for Cycle 1.x-equivalent voice context,
  unison per-voice controls, and voice-aware generators/processors,
- FFT/IFFT schema tests for one-cycle transforms, cyclic overlap, acyclic
  overlap, and acyclic carry-buffer latency declaration,
- channel layout validation for linked stereo, mono duplication, split left/right,
  and merge behavior,
- serialization round trips for graph identity, ports, edges, attachments,
  sub-target ids, layout, and channel state,
- compiler tests for topological order and buffer allocation plans,
- FFT/IFFT node round trips using known signals,
- multiply node tests for envelope-style modulation,
- adapter-node tests for representative rasterizer and effect behavior.

### Integration Tests

- generated legacy graph matches current fixed-flow processing order,
- generated legacy graph exposes previously hidden scratch and guide-curve
  assignments,
- generated legacy graph exposes pitch and unison voice-control assumptions
  rather than hiding them in fixed synth state,
- minimal compiled runtime matches representative Cycle 1.x audio behavior,
- upstream mesh edit invalidates downstream previews and execution state,
- parameter-only edits avoid unnecessary graph recompilation,
- topology edits force recompilation and publish a new plan snapshot.

### UI And Automation Tests

- launch Cycle in node workspace,
- capture node canvas screenshot,
- capture grid/minimap screenshot coverage,
- drag nodes and verify persisted layout,
- create valid and invalid edges,
- verify routed cables avoid node bodies in a representative patch,
- verify attachment cables are visually distinct from signal cables,
- verify invalid-edge feedback,
- double-click trilinear and 2D mesh nodes,
- switch trilinear editor between 3D and 2D modes,
- edit morph position and observe preview invalidation,
- create envelope-to-multiply-to-output graph,
- attach an envelope to a mesh layer scratch port,
- create splitter/merger stereo graph.

### Realtime And Performance Tests

- assert graph execution performs no audio-thread allocation,
- measure plan publication under topology changes,
- stress preview invalidation with many visible nodes,
- verify sprite cache reuse and bounded texture growth,
- verify reduced-detail previews during drag/interaction.

## Open Design Notes

- The internal spectral amplitude name should stay `SpectralMagnitudeSignal`
  until a true power-scale representation is introduced. The UI can later choose
  whether to label this as magnitude or power.
- Channel split should start as explicit splitter/merger nodes. Node-local
  split-stereo toggles can be a later convenience if they map to the same graph
  semantics.
- The node workspace should not try to embed existing JUCE panels as child
  components. Existing panel behavior should be adapted conceptually into
  GL-rendered expanded editors progressively.
- `ProcessingNodeGraph` and `Updater::Graph` must remain clearly named in code
  and documentation if Cycle 1.x code is reused. One is the user's patch; the
  other is a legacy invalidation-order graph.
- Do not add a first-class scratch-envelope domain. Scratch behavior is an
  envelope attachment to a dedicated scratch port or sub-target.
- Keep the first unison node close to Cycle 1.x behavior. Broader voice-lane
  routing can be revisited after parity is proven.
- Treat intelligent, soft cable routing as part of the product feel and graph
  readability, not as a cosmetic detail.
- Keep spectrogram and cyclogram previews as signature Cycle 2.0
  visualizations, even if the first implementation uses simpler placeholders.
- Avoid baking signal-domain names into ordinary processing node names. Edge
  colour and graph topology should describe whether a signal is time-domain,
  spectral magnitude, spectral phase, control, or attachment data. Nodes like
  `FFT`, `IFFT`, and domain/source selector nodes may change or establish signal
  domain, but generic operations should be named by operation (`Add`, `Multiply`,
  `Clamp`, `Mesh`) rather than by the domain currently flowing through them.
- Revisit arithmetic node port topology before hardening schemas. `Add` and
  `Multiply` need visual variants such as side-by-side, T, and uptack layouts so
  repeated layer pipelines can be strung together without awkward cable routing,
  and so magnitude/phase sibling pipelines can mirror each other cleanly.
- Non-preview nodes such as `FFT`, `IFFT`, arithmetic, channel split/join, and
  output should be compact and identifiable through iconography rather than
  oversized placeholder preview regions.
- Fold the initial domain/source selector into `VoiceContext` or equivalent
  context state rather than adding separate `Start` nodes. This lets graphs
  start directly in a spectral domain, such as a trilinear mesh used as an
  initial magnitude spectrum without an upstream FFT, without making mesh
  generators imply a permanent signal domain from their name alone.
- Envelope nodes should follow Cycle 1.x point-curve semantics, not ADSR-first
  envelope semantics. ADSR-style controls may appear only as a later preset or
  convenience editing mode over the same point-curve model.
- Guide curves should be visible as attachments to mesh nodes, with the target
  scope described more granularly than node-wide attachment. A later schema
  needs a way to address mesh sub-targets such as vertex cube and guided
  dimension (`amp`, `phase`, `sharp`, component curve) without exploding every
  possible target into permanent visible ports.
