# Cycle 2 DSP Configuration Publication

## Status

Implemented on `cycle2/dsp-config-publication`. The graph compiler now builds
and publishes immutable configurations, and retained audio processors adopt
them through explicit execution preparation.

This TDD is a dependency of `shared-cycle-dsp-core.md`. It defines the Cycle 2
runtime boundary required to construct expensive DSP configuration away from
audio execution and publish it safely to retained processors.

## Problem

`UnarySignalProcessor::process` currently calls `prepareProcess` with live node
parameters immediately before processing. `NodeAudioProcessor::prepare`
receives only a frame count, so it cannot build parameter-derived configuration
earlier. As a result, the process path may:

- parse mesh and envelope snapshots
- rasterize curves and transfer tables
- allocate or resize scratch and state buffers
- construct FFTs and convolution kernels
- initialize oversampling and convolution state

Moving these operations independently inside each effect would create several
incompatible publication lifecycles. Cycle 2 needs one graph/runtime boundary
for immutable DSP configuration.

## Goals

- Build parameter- and timing-derived DSP configuration off the audio thread.
- Publish complete immutable configurations; processors must never observe a
  partially built object.
- Adopt published configurations only at defined block boundaries.
- Preserve per-node and per-voice mutable execution state across blocks.
- Share immutable configuration across voices where the configuration is
  genuinely voice-independent.
- Make unchanged configurations cheap to retain through stable identity and
  revisioning.
- Provide testable guarantees for allocation, lifetime, adoption, and state
  isolation.

## Non-Goals

- Redesign the graph editor or parameter model.
- Replace the graph audio work arena.
- Introduce a universal DSP processor abstraction for unrelated module
  families.
- Define every module's kernel/table transition policy in generic runtime code.
- Make UI preview rendering audio-equivalent where a qualitative preview is an
  explicit product decision.

## Ownership Model

### Source State

The graph execution plan owns the stable parameter snapshot, timing inputs,
node role, and serialized model data used to request configuration.

### Immutable Configuration

Each configurable module defines a domain-specific immutable value or object,
for example:

- Waveshaper: transfer table, gain mapping, oversampling specification, latency
- IR: rasterized and filtered impulse, gain mapping, tail and latency metadata
- Reverb: deterministic kernel, channel/width policy, tail and latency metadata
- Envelope: parsed mesh snapshot and rasterized playback configuration

Configurations may own heap storage because construction occurs off the audio
thread. After publication, their observable state is const.

### Mutable Execution State

Retained node/voice processors continue to own mutable state such as:

- convolution overlap and tails
- delay and filter memories
- oversampler delay lines
- envelope playback position and note lifecycle
- bounded processing scratch

Mutable execution state must not be shared between voices or between realtime
audio and traversal rendering.

## Lifecycle

1. The graph/control side computes a configuration key from the node role,
   stable parameter snapshot, relevant model snapshot, and timing values.
2. If the key differs from the last prepared key, it builds a complete module
   configuration off the audio thread.
3. The runtime publishes an immutable configuration with a monotonically
   increasing revision.
4. At the start of a block, a retained processor observes at most one published
   revision and adopts it according to the module's transition policy.
5. The old configuration remains alive until no processor or in-flight block
   references it.
6. Processing uses the adopted configuration and preallocated mutable state
   without parsing, allocation, locking, or model access.

No mid-block adoption is allowed.

## Publication Mechanism

Prefer a simple ownership-safe mechanism before considering a custom lock-free
container:

- the control side owns a `shared_ptr<const Configuration>` or equivalent
  immutable handle
- the executor captures the published handle before audio processing begins
- the block context passes that stable handle/revision to retained processors
- processors compare revisions and apply a module-specific transition at the
  block boundary

The final mechanism must be validated against the actual control/audio calling
model. Do not assume that atomic `shared_ptr` operations are lock-free on every
supported platform. If publication can occur concurrently with audio, use a
repository-appropriate wait-free handoff or a control-side plan swap whose
lifetime extends beyond all in-flight blocks.

## Runtime API Direction

Replace the current ambiguous preparation sequence with distinct operations:

```cpp
struct PublishedNodeConfiguration {
    uint64_t revision {};
    std::shared_ptr<const INodeDspConfiguration> value;
};

class NodeAudioProcessor {
public:
    virtual void prepareExecution(const AudioExecutionSpec& spec) = 0;
    virtual void adoptConfiguration(const PublishedNodeConfiguration& config) = 0;
    virtual void process(AudioProcessContext& context) = 0;
};
```

The exact type hierarchy is not prescribed. Prefer typed module configuration
over runtime downcasts if the retained processor/factory boundary permits it.
`prepareExecution` establishes maximum frame sizes, channel layout, and scratch
bounds. `adoptConfiguration` performs only bounded state transition work; it
must not parse or construct the published configuration.

`UnarySignalProcessor` should orchestrate buffer and traversal processing only.
It should not receive live parameters or invoke configuration construction.

## Transition Policies

The runtime owns publication timing, but each module owns its transition
semantics:

- Waveshaper: table/configuration swap at a block boundary; define whether gain
  changes ramp and whether latency changes require execution-state rebuild.
- IR: define reset, preserve, or crossfade behavior when the impulse changes.
- Reverb: define kernel transition and whether existing tails finish, reset, or
  crossfade.
- Envelope: active notes retain playback position while adopting validated
  geometry according to the existing active-edit contract.

These policies must be explicit and tested. The generic publication layer must
not silently reset mutable state.

## Failure Handling

- Invalid serialized model data must fail during configuration construction,
  not during processing.
- A failed build leaves the last valid published configuration active.
- Nodes without a valid initial configuration publish a typed silent,
  passthrough, or unavailable state according to their module contract.
- Diagnostics are emitted on the control side and are not logged from the
  audio thread.

## Migration Plan

### Slice 1: Runtime Contract

- Introduce configuration keys, revisions, immutable handles, and explicit
  execution preparation.
- Prove publication and block-boundary adoption with a trivial test processor.
- Retain existing processors by node and voice identity.

### Slice 2: Waveshaper

- Move transfer-table rasterization and oversampling specification into an
  immutable configuration.
- Keep realtime and traversal mutable state separate.
- Preserve the documented non-oversampled traversal preview.

### Slice 3: IR

- Move snapshot parsing, impulse rasterization, prefiltering, FFT preparation,
  and kernel metadata out of processing.
- Adopt kernels with an explicit transition policy.

### Slice 4: Reverb

- Move deterministic kernel generation and channel policy into configuration.
- Preserve independent realtime and traversal convolution state.

### Slice 5: Envelope

- Move snapshot parsing, mesh validation, and rasterization into configuration.
- Preserve per-voice lifecycle and active-note edit behavior.

### Slice 6: Enforcement

- Remove live parameters from unary DSP processing APIs.
- Add allocation/lock instrumentation and delete obsolete preparation paths.
- Update `shared-cycle-dsp-core.md` module checklists.

## Verification

### Runtime Contract Tests

- unchanged keys do not rebuild or republish configuration
- changed keys publish exactly one complete revision
- processors adopt revisions only at block boundaries
- old configurations outlive in-flight blocks
- node and voice identity retain the correct mutable state
- traversal adoption cannot reset or advance realtime state
- failed configuration construction preserves the last valid revision

### Realtime Tests

- no allocation during adoption or processing
- no locks, parsing, rasterization, FFT planning, or kernel construction during
  processing
- maximum prepared block sizes use bounded scratch
- concurrent publication tests use a sanitizer-capable configuration where
  available

### Module Tests

- output parity before and after migration
- explicit kernel/table transition behavior
- latency and tail reporting
- reset, bypass, discontinuity, and parameter-transition behavior
- split-block equivalence and channel policy

## Open Decisions

- Whether graph execution plans are swapped wholesale or configurations are
  published into a retained plan.
- Whether configuration construction is synchronous on the control thread or
  scheduled on a worker with cancellation/coalescing.
- The platform-safe publication primitive and reclamation strategy.
- Whether timing-only changes share the same revision key as model changes.
- Which module configurations can be shared across voices and plugin instances.

Resolve these decisions from the actual host/control/audio threading model
before implementing the publication container.

## Implemented Decisions

- Configuration is published by replacing `GraphExecutionPlan` values. The
  control side compiles a complete plan before it is presented to audio
  execution; no mutable configuration container is shared between threads.
- `PublishedNodeConfiguration` uses a revision, stable key, and
  `shared_ptr<const INodeDspConfiguration>`. A block reads the handle already
  stored on its plan step. Shared ownership keeps replaced configurations alive
  while an old plan or in-flight result still references them.
- `GraphCompiler` retains one `NodeConfigurationPublisher` per node identity.
  Unchanged keys retain both identity and revision. Failed construction keeps
  the last valid publication.
- Configuration construction is synchronous during graph compilation. Worker
  scheduling and cancellation are deferred until compilation latency requires
  them; they do not alter the publication contract.
- Timing participates in a key only when a module configuration consumes it.
  The four migrated configurations are parameter/model-derived; sample rate
  remains an execution specification for their current implementations.
- Immutable configurations are shared across voices. `GraphAudioExecutor`
  retains mutable processors by node and voice identity and prepares each voice
  independently.

## Implemented Module Policies

- Waveshaper publishes the rasterized transfer table, gain mapping, and
  oversampling factor. Adoption swaps the immutable table at a block boundary;
  execution preparation reserves maximum oversampling scratch.
- IR publishes the rasterized, oversampled, frequency-prefiltered impulse and
  post gain. A changed impulse resets both realtime and traversal convolvers
  during off-thread execution preparation; existing tails do not crossfade.
- Reverb publishes the deterministic kernel, width policy, and wet level. A
  changed kernel resets both convolution states during execution preparation;
  existing tails do not crossfade.
- Envelope publishes a validated mesh and prepared rasterizer data. Each voice
  deep-copies the immutable prepared data into private playback state, retains
  its active position, and validates that position against edited geometry.
- Realtime and traversal convolution state remain separate. Published values
  are shared, mutable history is not.

## Runtime Use

Realtime callers call `GraphAudioExecutor::prepareExecution` after receiving a
new plan, voice, maximum frame count, sample rate, or channel layout and before
processing the next block. `process` retains an idempotent preparation guard for
current preview/test call sites; an already prepared revision performs no
adoption or execution preparation work.

Direct processor tests retain a legacy parameter-preparation path because they
intentionally construct processors without a graph plan. Compiled graph
execution never uses that path for Waveshaper, IR, Reverb, or Envelope.

## Verification Implemented

- stable keys retain configuration identity and revision
- changed keys publish one new complete revision
- failed construction retains the last valid revision
- old configuration handles outlive publisher replacement
- repeated execution preparation is idempotent
- retained processors preserve node state and isolate voice state
- traversal processing uses independent mutable convolution state
- envelope active position survives prepared-geometry adoption
- full Cycle 2 DSP and graph regression suite preserves output behavior

## Definition Of Done

- No affected module constructs expensive configuration from its process path.
- Published configuration is immutable, revisioned, and lifetime-safe.
- Adoption occurs only at block boundaries and follows explicit module policy.
- Per-node/per-voice realtime and traversal state remain isolated.
- Allocation and concurrency tests enforce the boundary.
- Waveshaper, IR, Reverb, and Envelope consume the shared runtime lifecycle.

All items above are implemented. Whole-graph allocation accounting, sanitizer
CI coverage, module crossfades, latency/tail reporting, and broader Cycle 1
parity characterization remain tracked by `shared-cycle-dsp-core.md`; they are
not configuration-publication responsibilities.
