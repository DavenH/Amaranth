# Cycle V2 Oscillator Region Compilation

## Status

In progress.

Implemented compiler foundation:

- node definitions publish declarative execution traits;
- compiled steps distinguish signal domain, execution coordinate, and runtime
  ownership scope;
- cycle-producing branches are partitioned into explicit Voice Context-owned
  oscillator regions;
- each region records its Unison lane count, materialization boundary,
  reconstruction policy, and chained or shared-spectral strategy;
- an operation reached by multiple Voice Contexts fails compilation with an
  `AmbiguousVoiceContext` diagnostic.
- the Cycle 1 fractional lane clock and chained `VoiceRasterizer` priming,
  sampling, and spillover transition are shared Cycle DSP primitives consumed
  by the Cycle 1 time-only renderer.
- a prepared Cycle V2 chained-region runtime owns bounded per-lane clocks and
  cycle buffers, preserves split-block continuity, and folds lanes with the
  shared Cycle 1 pan and level contracts.
- a direct single-Trimesh `ChainedPerLane` region is prepared off the realtime
  thread and executes through the mature shared `VoiceRasterizer` transition;
  its graph-runtime lowering performs no topology search or allocation and
  preserves contiguous output across arbitrary block partitions.
- prepared chained recipes now route multiple Trimesh cycle fields through
  vectorized Add and Multiply operations before the region's Unison lanes are
  folded. The compiler publishes the unique terminal cycle operation as the
  time-only materialization boundary, and the prepared recipe converts graph
  step references into compact operation-buffer indices off the realtime
  thread.
- oscillator-region lifecycle is segmented at exact event sample offsets.
  Note-on and retrigger reset lane/rasterizer history before activating the
  region, Reset clears and silences it, and NoteOff leaves oscillator history
  running for the voice envelope's release contract.
- the mature Cycle 1 fixed-frame time-mesh transition (ordinary
  `VoiceRasterizer` traversal, intercept padding, integral/interval sampling)
  is exposed beside the chained transition in `OscillatorLaneRasterizer`, and
  Cycle 1's spectral voice consumes the shared facade. This is the
  authoritative frame-generation boundary for the upcoming Cycle V2 shared
  spectral recipe; it is not the normalized blockwise preview renderer.
- a prepared Cycle V2 spectral recipe now executes fixed time-mesh frames,
  FFT/IFFT, spectral mesh operands, and vectorized spectral Add/Multiply using
  compact preallocated slots. Power-of-two transforms are prepared for the
  bounded note range, and their per-region exclusive ownership disables the
  legacy shared-instance mutex on the realtime path. A fixed-frame
  FFT/IFFT identity test proves the recipe consumes the shared mature
  rasterizer output unchanged.
- cyclic spectral frames now reconstruct independently through the shared
  clock, phase/composition core, mature Hermite resampler history, pan, and
  level contracts for every Unison lane. The frame calculation remains shared
  and is independent of lane count.
- explicit spectral materialization prevents sibling time-only discovery from
  crossing back into the spectral region. Mixed spectral and chained siblings
  therefore prepare as independent regions, fold their own Unison lanes, and
  meet at ordinary allocation-free sample-block Add or Multiply.
- the Cycle 1 parity policies explicitly declare zero algorithmic output
  latency and envelope-owned reconstruction history with no finite region
  tail. NoteOn and Reset remain sample-exact, while NoteOff keeps cyclic state
  alive for the envelope. The unimplemented `acyclicCarry`/OLA mode now fails
  compilation instead of silently running the cyclic policy.

The spectral recipe is a domain executor, not a compatibility copy. Its
authoritative operations remain `OscillatorLaneRasterizer` for fixed time
frames, `Transform` for polar FFT/IFFT, `TrimeshBlockwiseDsp` for the current
non-cyclic spectral mesh field, and `Buffer` for binary operations. Preparation
only translates immutable compiled step/output references into compact slot
indices and exclusive DSP instances. The stable endpoint is this
oscillator-region-owned executor; the flat graph processors remain the
deletion target once every spectral region and preview consumer routes through
the domain interfaces.

Nonzero latency compensation belongs to the future WindowedOverlapAdd policy;
the current parity policies require no compensation at mixed-strategy merges.
Placeholder deletion remains open. Full oscillator and Unison audio parity is
not yet claimed.

Depends on:

- `cycle-v2-voice-context-attachments.md` for voice-wide configuration;
- `cycle-v2-unison-parity.md` for the shared Unison layout and tuning contract;
- `shared-cycle-dsp-core.md` for mature rasterization and oscillator reuse;
- `cycle-v2-causal-update-graph.md` for immutable publication and revision
  identity.

This TDD refines the broad cycle-frame model in `node-graph-workflow.md`.

## Problem

Cycle V2 currently compiles one flat ordered list of node processors. A
`SignalPayload` has a signal domain and a sample block, plus an optional
traversal grid, but does not say whether the values represent:

- a normalized oscillator cycle;
- a fixed spectral frame;
- one pitch-clocked Unison lane on the sample timeline;
- an oscillator after its Unison lanes have been folded;
- a complete synth voice; or
- an already-polyphonic audio block.

Signal domain alone cannot answer whether two branches may be combined. Two
`TimeSignal` cycle fields at unrelated oscillator pitches do not share an x
coordinate and cannot be multiplied elementwise. Once both oscillators have
been rendered onto the common sample timeline, their blocks may be added or
multiplied regardless of pitch.

The missing distinction also blocks truthful Unison execution. Cycle 1 uses
two materially different strategies:

- a time-only path that rasterizes and chains cycles independently for every
  Unison lane;
- a spectral path that calculates one shared fixed cycle frame, processes it
  through FFT-domain magnitude and phase behavior, then phase-shifts,
  resamples, pans, and accumulates that frame separately for every lane.

Treating both paths as ordinary block processors would discard the mature
cycle scheduler. Treating the entire graph as one cycle would prevent useful
sample-domain operations such as amplitude or ring modulation between
non-pitch-aligned oscillators.

## Goals

- Identify independently executable oscillator regions in the authored graph.
- Keep signal domain separate from execution coordinate and ownership scope.
- Select the Cycle 1-equivalent time-only or spectral Unison lowering per
  oscillator region.
- Materialize every oscillator region onto the common sample timeline at an
  explicit compiled boundary.
- Permit block Add and Multiply between unrelated oscillator pitches within
  one synth voice.
- Fold each oscillator's Unison lanes before ordinary block arithmetic.
- Declare and compensate latency before block branches merge.
- Preserve an explicit reconstruction policy so genuine windowed overlap-add
  can replace cyclic frame crossfading without changing Unison configuration.
- Reuse the mature Cycle 1 voice rasterizer, pitch, resampling, phase, panning,
  and accumulation behavior rather than approximating it in graph orchestration.

## Non-Goals

- Arbitrary user-visible routing of individual Unison lanes.
- Lane-wise pairing such as `sum(A_i * B_i)`.
- Multiplication of already-polyphonic output as the default oscillator AM
  path.
- Implicit conversion between incompatible cycle clocks.
- Treating preview traversal grids as realtime oscillator execution.
- Implementing a new overlap-add algorithm in the first Cycle 1 parity slice.
- Copying Cycle 1 voice scheduling into `GraphCompiler` or a generic node
  processor.

## Authoritative Cycle 1 Behavior

### Strategy selection

`SynthesizerVoice::enablementChanged()` selects `SynthFilterVoice` when
spectral filtering or FFT phase processing is active and `SynthUnisonVoice`
otherwise.

`SynthUnisonVoice` uses `CycleBasedVoice::Chain`. For every Unison lane,
`renderChainedCycles()` updates that lane's pitch and detune, calculates its
variable output period, invokes `calcCycle(group)`, and appends the generated
samples to that lane's cycle buffer.

`SynthUnisonVoice::calcCycle()` uses the lane's persistent voice-cycle state,
Unison phase, scratch position, morph position, and pitch-derived sampling
interval while rasterizing every active time mesh. The time mesh is therefore
a cycle recipe instantiated under each lane, not one block shared by all
lanes.

### Spectral frame execution

`SynthFilterVoice` uses `CycleBasedVoice::Interpolate`. Its `calcCycle()`:

1. rasterizes active time layers into a fixed power-of-two cycle;
2. performs a forward FFT when time content exists;
3. applies spectral magnitude layers;
4. applies spectral phase layers;
5. performs the inverse FFT.

`renderInterpolatedCycles()` invokes that calculation once using the first
group, then gives every Unison lane its own angle delta, phase shift, pan,
resampler history, output frontier, and cycle buffer. It crossfades successive
shared cycle frames over the stored half-cycle and accumulates the resulting
lane blocks.

The first parity implementation must preserve these observable contracts even
if shared types and ownership differ. Cycle 1 class boundaries are references,
not a requirement to reproduce the same inheritance hierarchy.

## Terminology

- **Synth voice**: one active MIDI note and its note/envelope lifecycle.
- **Unison lane**: one detuned, phase-offset, panned oscillator instance inside
  a synth voice. Avoid calling this merely a voice in new APIs.
- **Oscillator region**: one maximal connected graph subgraph that produces an
  oscillator and must share a cycle execution strategy before materialization.
- **Cycle field**: values indexed by normalized oscillator phase under one
  declared cycle clock.
- **Spectral frame**: magnitude/phase values for one declared FFT frame shape,
  origin, and reconstruction policy.
- **Lane block**: one Unison lane rendered on the sample timeline.
- **Oscillator block**: the sum of all rendered Unison lanes for one oscillator
  region.
- **Synth-voice block**: the result after oscillator blocks and per-note
  envelope/control processing, but before polyphonic summation.
- **Polyphonic block**: the sum of multiple active synth voices.

These scopes are semantic. Equal vector lengths do not make two scopes
interchangeable.

## Execution Coordinates

Graph compilation resolves both a signal domain and an execution coordinate:

```cpp
enum class ExecutionCoordinate {
    Configuration,
    CycleField,
    SpectralFrame,
    SampleBlock
};
```

Names may change, but the dimensions must remain independent:

```text
signal domain:          time / magnitude / phase / control / envelope
execution coordinate:  cycle / spectral frame / sample block
ownership scope:       context / synth voice / oscillator region / lane
```

A cycle edge carries a clock key sufficient to prove coordinate compatibility:

```cpp
struct CycleClockKey {
    VoiceContextId context;
    OscillatorRegionId region;
    LaneScope laneScope;
    CycleFramePolicy framePolicy;
};
```

A spectral edge additionally carries a frame key containing at least FFT size,
window/reconstruction identity, frame origin, hop, and magnitude/phase layout.
The exact types may be interned plan IDs rather than copied structs.

Two operations may share a cycle or spectral coordinate only when their keys
are compatible. The compiler must not infer compatibility merely because both
edges are `TimeSignal` or have the same buffer length.

## Node Execution Traits

Node definitions advertise declarative execution traits. The compiler does
not grow a central switch containing oscillator algorithms.

Representative traits are:

- cycle generator;
- cycle-coordinate transform;
- spectral-frame transform;
- sample-block processor;
- control/envelope producer;
- oscillator materialization boundary;
- configuration-only attachment.

A node may support more than one coordinate. Generic Add and Multiply may
operate on compatible cycle fields or on sample blocks; their selected lowering
comes from the resolved inputs. A node-local implementation receives the
already-selected typed product and does not rediscover the strategy.

Current `TrimeshBlockwiseDsp::renderCycle()` is scaffolding: it samples one
normalized mesh cycle into a vector but does not implement pitch-clocked Cycle
1 scheduling. It must either become the prepared cycle-field producer used by
the mature voice scheduler or be deleted when that scheduler is integrated.
Its current `frameCount` block must not be treated as proof of oscillator audio
parity.

## Oscillator Region Discovery

An oscillator region begins at one or more cycle-producing sources scoped by
exactly one Voice Context. It includes connected cycle and spectral transforms
until the values are materialized on the common sample timeline.

The compiler:

1. resolves Voice Context configuration attachments;
2. assigns every voice-aware node to exactly one Voice Context;
3. resolves signal domain and candidate execution traits;
4. propagates compatible cycle/spectral coordinate constraints;
5. partitions maximal compatible oscillator regions;
6. selects a strategy and reconstruction policy for each region;
7. inserts a plan-level materialization/fold boundary;
8. schedules ordinary block processing and latency alignment after those
   boundaries.

Region discovery uses graph connectivity and declared traits. It must not use
canvas position, palette grouping, node title, or a list of special-case path
shapes.

One Voice Context may own several oscillator regions. Spectral processing in
one region does not force its sibling regions into spectral execution. Two
branches become one region only while they overlap in a compatible cycle or
spectral coordinate before materialization.

If unrelated oscillator clocks meet at a node that requires a cycle field,
compilation fails with a diagnostic identifying the incompatible branches and
the required block materialization boundary. It must not resample one cycle
field into the other's phase grid implicitly.

## Time-Only Lowering: Chained Per Lane

A region with no active spectral-frame processing selects
`ChainedPerLane`:

```text
for each active synth voice:
    for each Unison lane:
        derive instantaneous pitch + detune
        execute the region's cycle recipe with lane state
        render the variable-length cycle onto the sample timeline
        retain phase, scratch, resampler, and continuity state
    pan/scale and fold lane blocks into one oscillator block
```

The compiled region recipe is shared and immutable. Runtime state is per synth
voice, oscillator region, and Unison lane. Trimesh cycle state is also per
active mesh layer where required by the mature rasterizer.

Cycle-coordinate Add or Multiply inside this region is legal only when both
operands share the same cycle clock and lane. Control/envelope values may be
broadcast through an explicitly declared mapping. Non-pitch-aligned oscillator
signals are separate regions and cannot meet here.

The lowering reuses the shared `VoiceRasterizer` policies, Cycle 1 pitch and
detune calculation, established resampling, phase wrapping, scratch traversal,
pan law, and Unison level compensation.

## Spectral Lowering: Shared Frame, Per-Lane Reconstruction

A region containing spectral magnitude or spectral phase processing selects a
fixed-frame strategy equivalent to Cycle 1:

```text
for each active synth voice and required control-frame update:
    build one fixed power-of-two time cycle
    execute FFT -> spectral graph -> IFFT once

for each Unison lane:
    apply lane phase
    reconstruct/resample the shared frame at lane pitch + detune
    retain lane reconstruction history
    pan/scale lane block

fold lane blocks into one oscillator block
```

The spectral frame and immutable spectral configuration may be shared across
lanes. The following remain lane-local:

- output frontier and phase cursor;
- pitch/detune trajectory;
- initial Unison phase application;
- resampler history and spillover;
- reconstruction crossfade or overlap state;
- lane pan and accumulation destination.

The base power-of-two frame size follows the authoritative Cycle 1 contract
and is not changed by Unison detune. A future intentional divergence must
document its spectral-bin and pitch consequences.

## Spectral Reconstruction Policy

The oscillator-region strategy and its reconstruction policy are separate:

```cpp
enum class SpectralReconstructionPolicy {
    CyclicFrameCrossfade,
    WindowedOverlapAdd
};
```

### Cycle 1 parity policy

`CyclicFrameCrossfade` preserves the current half-cycle storage and crossfade
between evolving self-cyclic frames, followed by per-lane resampling. This is
the required first spectral parity policy.

### Future genuine overlap-add

`WindowedOverlapAdd` may use a larger, for example 2x, FFT window and carry the
IFFT tail into later hops instead of crossfading two self-wrapped frames. It is
not an implementation detail. Its contract includes:

- what time support populates the larger window;
- FFT size and harmonic/bin mapping;
- analysis and synthesis windows;
- hop size and constant-overlap-add normalization;
- frame origin and update cadence;
- initial latency and retained tail length;
- note-on priming, note-off draining, reset, and voice stealing;
- graph-generation replacement and stale-tail policy.

Prepared spectral content may be shared, but evolving reconstruction cursor,
overlap tail, phase, and resampler state remain per Unison lane when lanes
advance differently.

The policy publishes exact sample latency and tail length. It cannot be enabled
silently as an optimization while claiming bitwise Cycle 1 parity.

## Materialization And Unison Folding

Every oscillator region ends in a compiler-owned materialization step. This is
initially a plan/runtime concept rather than a mandatory user-facing node.
Materialization:

- converts cycle/spectral products into lane blocks;
- applies the region's reconstruction policy;
- applies per-lane phase, pan, and level semantics at the authoritative stage;
- folds all Unison lanes into one oscillator block;
- declares block latency and tail behavior.

The authored graph may visualize the boundary through edge treatment or
compiler inspection, but users should not need to add boilerplate render nodes
to connect an oscillator to ordinary audio processing.

Folding before ordinary block arithmetic defines the default multiple-
oscillator behavior:

```text
Multiply(A, B) = (sum over A's lanes) * (sum over B's lanes)
```

This means “multiply the two audible oscillator signals” and includes all
cross-lane products naturally. A lane-wise operation such as
`sum(A_i * B_i)` is a different feature and requires an explicit lane-aware
node and compatibility policy.

## Block Processing And Oscillator Overlap

Once materialized, oscillator blocks share the audio sample clock and may
overlap regardless of pitch. Add and Multiply remain ordinary vectorized block
operations.

For two oscillator regions A and B:

```text
Voice Context
    |-- region A -- strategy A -- materialize/fold -- block A --+
    |                                                         Multiply
    `-- region B -- strategy B -- materialize/fold -- block B --+
```

The Multiply occurs inside each synth voice before its volume envelope and
before polyphonic summation. Direct multiplication of bipolar oscillators is
ring modulation. Conventional amplitude modulation is expressible as:

```text
carrier * (1 + depth * modulator)
```

Multiplication after polyphonic summation would introduce cross-note products
and is not the default oscillator-modulation lowering.

The same Add/Multiply UI node may also operate in compatible cycle or spectral
coordinates. Compiler diagnostics and inspection must reveal the selected
coordinate; equal names do not imply equal runtime representation.

## Latency And Tail Alignment

Block length equality does not prove time alignment. Every materialized region
publishes latency in samples plus any finite or indefinite tail contract.

Before Add or Multiply merges block branches, the compiler:

- computes cumulative latency from the latest common scope boundary;
- inserts or configures bounded compensation on earlier branches;
- includes reconstruction latency, resampler latency, transform carry, and
  explicit effect latency;
- rejects a merge when latency is unknown or cannot be compensated within the
  declared realtime policy.

For example:

```text
chained region -------- compensation delay ---+
                                               Multiply
spectral OLA region --- declared OLA latency --+
```

Latency compensation belongs to the compiled plan. Individual Add/Multiply
processors receive aligned blocks and contain no branch-history lookup.

Tail draining is also scoped. Note-off must permit a spectral reconstruction
tail to finish according to envelope/voice policy without reviving a stopped
voice or leaking state into a reused voice index.

## Plan Representation And Ownership

The flat top-level execution plan may contain region steps whose internals are
compiled subplans:

```cpp
struct OscillatorRegionPlan {
    OscillatorRegionId id;
    VoiceContextId context;
    OscillatorExecutionStrategy strategy;
    SpectralReconstructionPolicy reconstruction;
    CycleFrameContract cycleFrame;
    std::vector<CompiledRegionOperation> operations;
    SampleLatency outputLatency;
};
```

This is illustrative. The stable requirements are:

- high-level graph orchestration schedules prepared region executors;
- domain algorithms remain behind oscillator/rasterizer/transform interfaces;
- one compiled region recipe is immutable and shareable;
- mutable runtime state is indexed by synth voice, region, and lane;
- configuration attachment dependencies do not become signal operations;
- ordinary block nodes see only materialized, aligned payloads.

The execution trace exposes region identity, strategy, reconstruction policy,
materialization, lane count, cumulative latency, and chosen coordinate for
diagnostics and automation.

## Preview And Analysis Products

Traversal grids and compact previews may illustrate cycle, spectral, and block
products, but they do not select the realtime strategy. The same authored node
may have a cycle-field preview and a sample-block runtime product.

Preview requests explicitly identify:

- region and Voice Context;
- execution coordinate being visualized;
- audition note and duration;
- selected Unison configuration;
- spectral reconstruction policy when relevant.

Unison phase paths continue to show the audible per-lane phase trajectory. A
spectral strategy may annotate its reconstruction policy, but must not replace
the phase calculation with frame indices or renderer-only motion.

## Realtime And Complexity Contracts

- Region discovery, strategy selection, buffer planning, FFT preparation,
  rasterizer preparation, and latency compensation planning occur off the
  realtime thread.
- Runtime performs no graph search, topology inspection, allocation,
  serialization, UI publication, or mutex wait.
- The maximum Unison lane count bounds lane-state arrays at preparation time.
- Time-only cost is proportional to executed cycle work per active lane.
- Shared spectral-frame calculation runs once per required frame generation,
  not once per Unison lane; lane reconstruction remains proportional to lane
  count.
- Block arithmetic uses `Buffer`/`VecOps` operations.
- Split-block processing is equivalent to one contiguous block, including
  reconstruction, resampling, compensation, and tails.

## Negative Boundaries

- Do not identify oscillator regions from node layout or node-name switches.
- Do not treat all `TimeSignal` payloads as the same execution coordinate.
- Do not combine unrelated oscillator cycles by matching vector indices.
- Do not run the entire Voice Context in spectral mode because one sibling
  oscillator region contains an FFT.
- Do not calculate the shared spectral frame once per Unison lane.
- Do not share mutable resampler, phase, or reconstruction-tail state between
  lanes or synth voices.
- Do not fold all oscillator regions together before user-authored block Add or
  Multiply.
- Do not perform oscillator AM after polyphonic voice summation by default.
- Do not hide OLA latency or tail state inside IFFT or Multiply without a plan
  contract.
- Do not use traversal-grid compatibility as proof of realtime clock
  compatibility.
- Do not preserve the placeholder Wave Source or current Trimesh block adapter
  merely to satisfy narrow execution tests.

## Implementation Slices

1. Add declarative execution-coordinate traits and compiler diagnostics while
   retaining current runtime behavior.
2. Resolve Voice Context scope and partition oscillator regions without a
   second graph topology.
3. Define immutable prepared oscillator requests and runtime state ownership;
   adapt the shared `VoiceRasterizer` and mature pitch/resampling primitives.
4. Implement `ChainedPerLane` and prove Cycle 1 time-only Unison parity.
5. Implement shared fixed spectral frames with `CyclicFrameCrossfade` and prove
   Cycle 1 spectral/Unison parity.
6. Materialize/fold independent oscillator regions and route ordinary block Add
   and Multiply inside each synth voice.
7. Add latency propagation and compensation for mixed-strategy block merges.
8. Delete the placeholder Wave Source oscillator behavior, transitional
   Trimesh block execution, and any flat-plan compatibility scaffolding.
9. Design and implement `WindowedOverlapAdd` as an intentional post-parity
   reconstruction policy with its own golden tests and latency contract.

Each implementation slice follows the repository design, implementation,
refactor, style, semantic-test, and commit loop. The TDD remains in progress
until transitional adapters and deletion targets are gone.

## Verification

### Compiler and regions

- A time-only Trimesh oscillator compiles as one `ChainedPerLane` region.
- Adding FFT-domain magnitude or phase processing selects the shared spectral
  strategy for that region only.
- A sibling time-only oscillator under the same Voice Context remains chained.
- Configuration attachments affect region configuration but allocate no signal
  buffer and add no runtime operation.
- Two incompatible cycle clocks meeting before materialization produce a
  focused compile diagnostic.
- Compatible cycle and spectral operations retain their exact frame keys.

### Cycle 1 parity

- Time-only golden vectors cover every Unison order, lane detune/phase/pan,
  scratch trajectory, pitch-envelope movement, split-block boundary, and mesh
  chaining state.
- Spectral golden vectors cover time-to-FFT input, magnitude processing, phase
  processing, IFFT output, half-cycle crossfade, lane resampling, and final
  accumulation.
- The spectral frame calculation count is independent of Unison lane count;
  reconstruction state count equals the active lane count.
- One-lane configurations agree at the defined time-only/spectral boundaries
  where the mature products are expected to agree.

### Block overlap and AM

- Two unrelated oscillator pitches materialize independently and Multiply
  sample by sample without a cycle-clock error.
- Bipolar sine golden vectors produce the expected sum/difference ring-
  modulation components.
- `carrier * (1 + depth * modulator)` produces the expected carrier and
  sidebands.
- Two Unison oscillator inputs compute
  `(sum A lanes) * (sum B lanes)`, not implicit lane pairing.
- The operation occurs independently per synth voice before polyphonic sum.

### Latency and lifecycle

- Mixed chained/spectral branches align an impulse at Add and Multiply inputs.
- Resampler, crossfade, and future OLA history survive arbitrary audio block
  partitioning.
- Note-off, reset, voice stealing, and plan replacement clear or drain the
  correct region/lane tails without leaking into another voice.
- Realtime tests prove no allocation and bounded prepared storage for maximum
  polyphony, region count, and Unison order.

### Future OLA

- Window and hop satisfy the declared constant-overlap-add normalization.
- A 2x frame has a documented harmonic/bin mapping.
- Tail length and latency equal the compiled contract.
- Split processing matches contiguous processing through note-on and note-off.
- Enabling OLA is recorded as an intentional sonic-policy choice rather than
  reported as Cycle 1 bitwise parity.

## Completion Criteria

- The compiler distinguishes signal domain, execution coordinate, and runtime
  ownership scope.
- Every cycle-producing branch belongs to one explicit oscillator region and
  one Voice Context.
- Time-only and spectral regions lower to the two mature Cycle 1 Unison
  strategies without copied algorithms.
- Unison configuration is shared while mutable lane state is isolated.
- Each oscillator folds its lanes before ordinary block Add/Multiply.
- Non-pitch-aligned oscillators can be multiplied inside one synth voice.
- Mixed execution strategies merge only after sample materialization and
  latency compensation.
- Reconstruction policy is explicit, with Cycle 1 crossfade parity and a
  non-disruptive path to genuine windowed overlap-add.
- Placeholder oscillator/runtime behavior and transitional flat-plan adapters
  are deleted.
