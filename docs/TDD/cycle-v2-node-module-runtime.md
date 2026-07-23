# Cycle 2 Node Module And Runtime Boundaries

## Status

Partially implemented (audited 2026-07-23).

Concrete module factories, published configurations, compiled routing, stable
output slots, retained per-voice processors, and allocation-free representative
execution are implemented for the current Cycle 2 node set. The arena-backed
payload-view migration and realtime enforcement described below are not
complete. Realtime slots still use owning `std::vector<float>` payloads whose
capacity is reserved during preparation.

Depends on `cycle-v2-node-definition-and-graph-model.md` for authoritative node
definitions and typed configuration inputs. Complements:

- `cycle-v2-dsp-configuration-publication.md`
- `shared-cycle-dsp-core.md`
- `signal-traversal-grid.md`

Those documents remain authoritative for immutable configuration publication,
Cycle 1 DSP reuse, and traversal-grid semantics respectively.

## Problem

`NodeAudioProcessor.cpp` routes most audio roles through one
`FixedRoleProcessor`. Every instance owns DSP objects for unrelated roles and a
default Trimesh mesh, then selects behavior with a role switch. The preview
system repeats the pattern in `FixedPreviewProcessor`, combining authoritative
Trimesh rendering with qualitative placeholder algorithms for other node types.

`GraphAudioExecutor` builds per-node contexts with owning vectors and copies
parameters, input payloads, attachments, outputs, traversal grids, and
diagnostic results during processing. `AudioProcessWorkArena` reserves vector
capacity but does not provide a compiled buffer-slot or lifetime plan.

The result has weak object boundaries and is not a safe final realtime model:

- processors own state unrelated to their actual node
- adding a role modifies central runtime switches
- preview and audio implementations can drift
- processor dependencies and lifecycle requirements are hidden
- the executor performs allocation-capable container work and deep copies
- diagnostic capture defines the production processing shape

## Goals

- Give each node module a cohesive audio processor or narrow role adapter.
- Construct only the dependencies and mutable state required by that module.
- Keep generic runtime code responsible for scheduling, routing, storage, and
  lifecycle—not domain algorithms.
- Use shared Cycle DSP implementations through thin Cycle 2 adapters.
- Define whether each preview is authoritative, qualitative, or derived from
  upstream runtime data.
- Reuse the same prepared domain model for audio and preview where parity is
  required.
- Compile buffer slots, port mappings, attachment mappings, and lifetimes ahead
  of processing.
- Process with non-owning buffer and context views and no heap allocation.
- Keep full per-node capture as an optional diagnostic observer outside the
  realtime contract.

## Non-Goals

- Force streaming effects, transforms, generators, and state machines through
  one universal DSP algorithm interface.
- Move mature DSP algorithms into graph or executor code.
- Make every preview sample-identical to audio when the product intentionally
  calls for a qualitative visualization.
- Change immutable DSP configuration publication or per-voice state isolation.
- Optimize before measuring graph-plan and processing behavior.

## Node Module Boundary

Each executable definition supplies the factories appropriate to its
capabilities. Factories create concrete objects rather than a role-tagged
container:

```cpp
class NodeAudioProcessor {
public:
    virtual ~NodeAudioProcessor() = default;
    virtual void prepareExecution(const AudioExecutionSpec& spec) = 0;
    virtual void adoptConfiguration(
            const PublishedNodeConfiguration& configuration) = 0;
    virtual void process(NodeProcessView process) = 0;
};
```

Examples include `TrimeshSignalProcessor`, `FftSignalProcessor`,
`EnvelopeSignalProcessor`, `StereoSplitProcessor`, and
`PassthroughProcessor`. Shared payload mechanics remain in focused composition
helpers such as unary and binary signal processors.

A processor must not own an instance of every possible node DSP class. Its
constructor and data members should reveal its actual dependencies and mutable
state.

### Domain Ownership

- `AmaranthLib` owns reusable DSP algorithms, configurations, and execution
  state as specified by `shared-cycle-dsp-core.md`.
- A node folder owns the Cycle 2 adapter, configuration builder registration,
  and preview adapter for that node.
- Runtime owns retained processor identity, scheduling, port routing, and
  prepared work storage.
- The graph compiler owns the immutable execution plan and published
  configurations.

Node adapters translate types and orchestrate lifecycle. They must not contain
approximate replacements for mature Cycle 1 behavior.

## Preview Contracts

Every preview-capable definition declares one of:

- `AuthoritativeModel`: render the same prepared model or table used by DSP
- `RuntimeTap`: visualize an upstream or node output traversal payload
- `Qualitative`: intentionally illustrative and not DSP-equivalent
- `None`

Qualitative previews require a named product decision and tests that prevent
them from being consumed as DSP data. Placeholder preview algorithms must not
masquerade as authoritative node behavior.

When a node depends on an editable curve, mesh, envelope, transfer table, FFT
policy, or convolution kernel, audio and authoritative preview adapters consume
the same immutable configuration or shared domain snapshot. They maintain
separate mutable execution state where rendering could advance or reset audio.

## Compiled Processing Plan

The compiler extends `GraphExecutionPlan` with:

- concrete input and output port indices
- attachment ranges or indices per step
- processor/configuration handles
- buffer slot for each output
- last-consumer information and slot lifetime
- maximum scalar, vector, and traversal-grid requirements
- channel layout and domain metadata
- state and latency metadata required for scheduling

Buffer planning may initially assign one stable slot per produced port. Slot
reuse is a later optimization after lifetimes are tested. Correctness and fixed
capacity are more important than minimum memory.

## Processing Storage And Views

Replace owning `std::vector<float>` payloads in the realtime path with
repository-native `Buffer<float>` or an equivalent non-owning view over a
preallocated arena. Traversal grids receive the same treatment.

```cpp
struct NodeProcessView {
    size_t frameCount {};
    AudioProcessTiming timing;
    AudioVoiceContextView voice;
    Span<const SignalPayloadView> inputs;
    Span<const AudioAttachmentView> attachments;
    Span<SignalPayloadView> outputs;
    const INodeDspConfiguration* configuration {};
    NodeScratchArena& scratch;
};
```

The exact view types should reuse existing `Buffer`, `ScopedAlloc`, and stereo
buffer conventions. Empty, scalar, vector, and matrix payload shapes remain
explicit; no caller should infer them from vector length alone.

Preparation allocates or sizes:

- graph buffer arena
- traversal-grid arena
- bounded per-step scratch
- retained per-node and per-voice execution state
- routing and diagnostic metadata

Processing performs no resize, reserve, parsing, rasterization, processor
construction, logging, or graph lookup by string identity.

## Results And Diagnostics

The production executor writes the graph's declared output slots and returns a
small output view/status. Per-node payload capture is optional:

```cpp
class GraphProcessObserver {
public:
    virtual void nodeProcessed(const NodeProcessObservation& observation) = 0;
};
```

Tests, agent audio capture, and preview diagnostics may install an observer on
a non-realtime execution path or copy selected slots after the block. The
executor must not copy every payload merely because a caller might inspect it.

## Processor Identity And State

Retain mutable processors by stable node identity, voice identity where
required, processor type, and execution specification. A definition or module
change replaces the concrete processor. Configuration revision changes adopt a
new immutable configuration without replacing unrelated mutable state unless
the module's explicit transition policy requires it.

Removal and plan replacement reclaim processors only after no in-flight plan
can reference them. Do not encode retention solely through repeated linear
searches of node IDs on the audio thread.

## Migration Plan

### Slice 1: Concrete Processor Factories

- Characterize every existing audio role and its retained state.
- Extract source, routing, arithmetic, passthrough, and Trimesh behavior into
  concrete processors under appropriate node folders.
- Register factories in node definitions.
- Delete `FixedRoleProcessor` when all roles have migrated.

### Slice 2: Preview Classification And Extraction

- Classify every current preview contract.
- Move authoritative preview adapters beside their node modules.
- Replace curve and effect placeholders with shared prepared-model rendering or
  explicitly retain them as qualitative previews.
- Delete `FixedPreviewProcessor` when all roles have migrated.

### Slice 3: Static Routing Plan

- Precompute step port indices, attachment ranges, output slots, and maximum
  capacities.
- Remove per-block string-based output lookup and graph node-kind lookup.
- Retain the current owning payloads temporarily behind slot APIs.

### Slice 4: Arena-Backed Payload Views

- Introduce arena-owned sample and traversal storage.
- Adapt processors to non-owning input/output views.
- Remove per-node parameter, input, attachment, output, and grid copies.
- Separate diagnostic capture from production output.

### Slice 5: Realtime Enforcement

- Instrument allocations and locks during adoption and processing.
- Test maximum-size and mixed-shape graphs.
- Remove legacy owning-vector and live-parameter processing paths.

## Verification

### Module Tests

- each executable definition creates the correct concrete processor type
- processors own no state for unrelated roles
- configuration adoption preserves or resets state only according to the
  module policy
- audio and authoritative preview consume the same immutable domain data
- qualitative previews are explicitly classified

### Plan Tests

- every input, output, and attachment resolves to a valid compiled slot
- fan-out shares source storage without mutation or copying
- slot lifetimes cover every consumer
- empty, scalar, vector, matrix, stereo, and multi-output routing remain valid
- plan replacement preserves eligible processors and removes stale state

### Realtime Tests

- no allocations or locks during steady-state processing
- no parameter parsing, model parsing, rasterization, or factory calls during
  processing
- processing accepts every block size through the prepared maximum
- multiple voices have independent mutable state and shared immutable
  configuration where declared
- diagnostic observation is absent from or bounded outside realtime execution

### Parity Tests

- shared Cycle 1/Cycle 2 DSP characterization remains authoritative
- previews that promise parity update from the same configuration revision
- traversal rendering cannot advance or reset realtime state

## Completion Criteria

- Central role-tagged audio and preview god-objects are removed.
- Adding a node module requires registering factories, not editing runtime
  dispatch switches.
- The compiled plan owns routing and buffer-slot decisions.
- Realtime processing uses preallocated views and performs no deep payload
  copies or live parameter parsing.
- Diagnostic capture is optional and separate from the production executor.

## Completion Evidence

Implemented:

- `FixedRoleProcessor` and `FixedPreviewProcessor` were replaced by cohesive
  concrete processors and explicit factory registration tables.
- Preview definitions declare `AuthoritativeModel`, `RuntimeTap`,
  `Qualitative`, or `None`; Trimesh audio and authoritative preview share the
  same immutable prepared mesh configuration.
- `GraphCompiler` resolves input, output, and attachment indices, assigns every
  produced port a stable buffer slot, records first-producer/last-consumer
  lifetimes, and publishes typed DSP configurations.
- `GraphAudioExecutor::prepareExecution` prepares storage and retained direct
  processor dispatch for each voice. Realtime processing performs no factory
  calls, graph lookup, configuration/model parsing, or diagnostic payload
  capture.
- Processor reclamation accounts for all prepared voices, while configuration
  revisions preserve eligible processor state and role changes replace the
  concrete processor.
- Tests cover compiled slot/lifetime routing, fan-out storage identity,
  optional observation, plan replacement, independent and alternating voices,
  configuration revision behavior, authoritative preview sharing, and zero
  `operator new` allocations at both maximum and shorter prepared block sizes.
- The complete `CycleV2_tests` target passes after the migration.

Remaining:

- `AudioProcessWorkArena` records capacities but does not own the aligned graph,
  traversal-grid, and scratch arenas specified by this design.
- `SignalPayload`, `SignalBlock`, and traversal grids retain owning vectors in
  realtime slots. `NodeProcessView`, `SignalPayloadView`,
  `AudioVoiceContextView`, and `NodeScratchArena` have not been introduced.
- Realtime processing still uses allocation-capable vector operations whose
  safety depends on previously reserved capacity rather than APIs that cannot
  allocate.
- Lock instrumentation and removal of the legacy owning-payload processing
  surface remain incomplete.

Live host audio is a separate integration boundary. Cycle 2 currently executes
graphs for previews and offline automation capture; the standalone application
does not connect `GraphAudioExecutor` to an audio-device callback, and no Cycle
2 plugin callback target exists. That work is not a completion criterion for
this node-module boundary, but it is required before Cycle 2 has live playback.
