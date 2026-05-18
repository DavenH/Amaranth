# ADR 008: Node Graph Workflow For Cycle Processing

## Status

Proposed

## Context

Cycle currently presents a fixed processing flow:

- create and combine waveform layers,
- optionally analyze the signal into a spectrum,
- apply harmonic magnitude and phase layers,
- transform the spectrum back to the time domain,
- process the result through the fixed effects chain.

That model matches the current UI and audio architecture, but it limits the
kind of sound-design workflows Cycle is well suited for. A designer should be
able to move between time-domain and spectral-domain processing repeatedly:
waveform generation, FFT analysis, spectral magnitude and phase filtering,
inverse FFT, waveshaping, impulse-response processing, another FFT, further
spectral filtering, and so on.

The current fixed flow can be represented as a graph, but users cannot edit
that graph. Processing order, domain conversion, and effects placement are
mostly implicit in application structure instead of explicit in the patch.

Cycle also has multiple kinds of processing elements:

- source nodes with no signal input, such as additive spectral generators or
  mesh-rasterized waveform oscillators,
- transform nodes, such as FFT and inverse FFT,
- domain-specific processors, such as subtractive spectral mesh filters,
- time-domain processors, such as waveshapers, convolution, delay, and EQ,
- mesh or control outputs that can influence more than one signal domain.

Those elements need a visible and validated grammar. A time-domain signal should
not be directly summed with a spectral magnitude or phase signal, while mesh
outputs may be interpreted by compatible receiving nodes in different domains.

ADR 006 already moves FFT representation toward typed spectrum buffers. The
node graph should build on that direction: signal-domain meaning should be
explicit at graph edges and runtime boundaries, not inferred from raw buffer
types or UI context.

## Decision

We will move Cycle toward a Blender-like node graph workflow where processing is
represented as a typed directed graph of nodes, ports, and edges.

The expected product direction is Cycle 2.0: a sister project to Cycle 1.x that
can reuse processing, mesh, preset, and compatibility code while leaving the
legacy multi-panel UI behind. Compatibility with Cycle 1.x behavior matters at
the DSP and data level, but the node editor does not need to preserve the old UI
as an alternate workspace.

The current fixed Cycle flow will become the first compatibility target: it
should be representable as a generated "legacy graph" before arbitrary user
patches replace it. This lets the node model prove that it can express existing
behavior without requiring a flag-day rewrite of the audio engine or UI.

Graph edges will enforce a small signal grammar. The initial domains are:

- `TimeSignal`: a time-domain audio or periodic waveform signal,
- `SpectralMagnitudeSignal`: spectral magnitude data produced by FFT analysis
  or spectral-domain source nodes,
- `SpectralPhaseSignal`: spectral phase data produced by FFT analysis or
  spectral-domain source nodes,
- `MeshField`: mesh-rasterized or geometric data that can be interpreted by
  compatible time, spectral magnitude, spectral phase, or control ports,
- `EnvelopeSignal`: time-varying envelope data that can modulate ordinary signal
  inputs or attach to dedicated scratch ports,
- `PitchSignal`: pitch data accepted only by voice-aware generators or
  processors,
- `VoiceControlSignal`: voice-scoped unison-style data such as per-voice pan,
  phase, detune, and voice index,
- control and modulation domains as later graph nodes require them.

FFT and inverse FFT will be explicit graph nodes:

- an FFT node consumes a `TimeSignal` and produces separate
  `SpectralMagnitudeSignal` and `SpectralPhaseSignal` outputs,
- an inverse FFT node consumes compatible spectral magnitude and phase inputs
  and produces a `TimeSignal`.

Nodes will declare typed input and output ports. Edges are valid only when the
source output domain can connect to the destination input domain. Direct
connections between incompatible domains are rejected by graph validation and by
the editor during patching.

Mesh outputs are domain-flexible, but not untyped. A mesh source should expose a
domain-neutral output such as `MeshField`; the receiving port decides whether
that field is interpreted as waveform shape, spectral magnitude, spectral
phase, waveshaper transfer, envelope shape, or another supported role.

Scratch behavior is not a separate scratch-envelope signal domain. Ordinary
envelope nodes may attach to dedicated scratch ports or sub-targets on
mesh-backed time and spectral layers. Those attachments are visible,
serializable graph structure and require explicit conversion before they can be
used as ordinary signal flow again.

FFT nodes operate on one cycle of time-domain content and produce one cycle of
spectral magnitude and phase content. Inverse FFT nodes produce one cycle of
time-domain content. Optional inverse overlap modes may be introduced later, but
any acyclic overlap must declare its carry buffer and latency in the node schema
and compiled execution plan.

The runtime audio path will not evaluate UI objects directly. The editable graph
will compile into a realtime-safe execution plan. The compiled plan owns the
ordered processing operations, typed intermediate buffers, and any prepared
state needed for audio or analysis execution.

The target architectural concepts are:

- `NodeGraph`: owns nodes, edges, graph identity, serialization, and validation,
- `Node`: describes a processing element and its typed ports,
- `PortDomain`: names the signal or control grammar accepted by an edge,
- `GraphCompiler`: validates a graph and emits an executable plan,
- `GraphExecutionPlan`: allocation-free runtime representation used by audio
  and analysis code,
- `LegacyCycleGraphBuilder`: builds a graph equivalent to the current fixed
  Cycle 1.x processing flow for compatibility, inspection, and migration.

## Visual Grammar

The graph editor must make signal domains visible without relying on color
alone.

The recommended visual language is:

- time-domain edges use one color family and a consistent port shape,
- spectral magnitude edges use a distinct color family and port shape,
- spectral phase edges use another distinct color family and port shape,
- mesh or cross-domain outputs use a neutral or source-specific treatment,
- invalid edges show immediate feedback before a connection is committed,
- ports expose concise labels or tooltips for their domain and interpretation.

Color is useful for fast scanning, but shape, labels, and invalid-connection
feedback are required for accessibility and for avoiding ambiguous graph states.

## Consequences

### Positive

- Cycle's visible editing model can match the actual processing model.
- Users can build arbitrary-length processing chains across time and spectral
  domains.
- FFT and inverse FFT boundaries become explicit sound-design operations.
- Domain mistakes can be prevented at graph-edit time instead of becoming
  subtle DSP bugs.
- The existing fixed pipeline can be preserved as a generated graph while the
  new architecture is introduced incrementally.
- Currently hidden scratch, guide-curve, layer, pitch, and unison assignments
  become visible and inspectable graph structure.
- Graph compilation creates a clean boundary between interactive editing and
  realtime audio execution.
- Typed spectrum buffer work from ADR 006 has a natural higher-level consumer.

### Negative

- The UI, serialization, validation, and audio execution surfaces become more
  complex.
- Node and port schemas become a compatibility surface for presets.
- Graph validation must handle disconnected ports, cycles, missing phase or
  magnitude partners, and incompatible domains clearly.
- Realtime execution needs careful buffer planning to avoid audio-thread
  allocation.
- Existing effect and rasterization code may need adapter nodes before it can
  participate in arbitrary graphs.
- Debugging user-authored graphs requires better inspection tools than the
  current fixed pipeline.

## Alternatives Considered

### Keep the fixed pipeline and add more insertion points

Rejected as the long-term model.

More insertion points would help with some workflows, but it would still encode
processing order in application structure rather than in the user's patch. It
would also make repeated time-to-spectrum-to-time workflows awkward and special
case heavy.

### Build a free-form graph with untyped audio buffers

Rejected.

Untyped buffers would make the editor easier to prototype, but they would lose
the main safety benefit of the graph. Time-domain, spectral magnitude, and
spectral phase data are not interchangeable. The graph should make those
differences explicit.

### Treat magnitude and phase as one spectrum edge

Deferred as a possible UI simplification, but rejected as the core model.

Some editor views may group magnitude and phase visually, but Cycle needs to
support independent magnitude and phase processing. The graph model should keep
them as separate typed signals and allow the UI to present paired ports where
that is more usable.

### Replace Cycle 1.x internals in one pass

Rejected.

The current pipeline is audio and preset sensitive. Cycle 2.0 can leave the
legacy UI behind, but it should still represent existing behavior as a
compatibility graph and migrate processing semantics incrementally.

## Implementation Plan

### Phase 1: Define the graph model

- Introduce graph, node, port, edge, and domain types.
- Add validation for compatible edge domains, required inputs, graph cycles,
  and disconnected outputs.
- Add serialization identifiers for node kinds, node instances, ports, and
  edges.
- Keep this layer independent from JUCE components and audio-thread execution.

### Phase 2: Represent the current Cycle 1.x flow

- Add a builder that emits a graph equivalent to the current waveform,
  spectrum, inverse FFT, and effects-chain flow.
- Validate that the generated graph preserves current processing order.
- Use this graph as the compatibility baseline for preset migration and tests.

### Phase 3: Compile graphs for runtime use

- Add a compiler that turns a validated graph into a deterministic execution
  plan.
- Preallocate intermediate buffers and node state outside the audio thread.
- Keep execution allocation-free and independent from graph-editor UI objects.
- Reuse typed spectrum concepts from ADR 006 at FFT boundaries.

### Phase 4: Add node adapters

- Wrap existing waveform, spectral layer, mesh rasterization, FFT, inverse FFT,
  and effects processors as graph nodes.
- Start with nodes needed to reproduce the legacy graph.
- Add more flexible nodes only after compatibility behavior is covered.

### Phase 5: Add the graph editor

- Build a node editor that exposes typed ports, edge creation, invalid-edge
  feedback, graph inspection, and domain-aware visual styling.
- Make the legacy graph visible before arbitrary patch editing becomes the
  default workflow.
- Add focused UI fixtures for connection creation, invalid connections, and
  domain rendering.

## Validation

The ADR itself is documentation-only.

Future implementation should include:

- unit tests for graph validation and incompatible edge rejection,
- FFT and inverse FFT node tests using known signals and round trips,
- tests that the generated legacy graph matches the current fixed processing
  order,
- serialization round-trip tests for graph identity, node ids, port ids, and
  edges,
- realtime checks that compiled execution does not allocate on the audio
  thread,
- UI automation fixtures for creating valid and invalid connections,
- screenshot coverage for domain color, shape, and invalid-edge feedback.

## Follow-Up Work

- Write a TDD document for staged implementation once this architectural
  direction is accepted.
- Decide the exact naming of the public graph types when code work begins.
- Decide whether the user-facing editor labels the magnitude domain as
  "magnitude", "power", or another term. Internally, the first implementation
  should match the current FFT magnitude semantics unless a true power-scale
  representation is introduced.
- Define preset migration policy for legacy fixed-flow presets after the graph
  serialization format exists.
