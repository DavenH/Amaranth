# Shared Cycle DSP Core

## Purpose

Cycle 2 must not reimplement mature Cycle 1 DSP algorithms locally when the
behavior is algorithmically complex. Shared behavior belongs in a UI-free
Cycle DSP layer in `AmaranthLib` and must be consumed by thin Cycle 1 and
Cycle 2 adapters.

The goal is to make "same node, same DSP" the default. Divergence is allowed
only when it is an explicit product decision with a documented rationale,
lifecycle difference, and dedicated tests.

Cycle 1 is the primary source for behavioral characterization, but existing
behavior is not automatically correct. Characterization must distinguish
intentional compatibility behavior from defects that should be fixed for both
applications.

## Problem

Cycle 2 has repeatedly grown approximate node-local implementations for effects
and signal transforms. That creates several risks:

- visual previews and audio output disagree with Cycle 1 behavior
- traversal-grid processing becomes a second algorithm instead of a second
  execution mode
- fixes in Cycle 1 DSP do not reach Cycle 2
- delay history, reverb tails, oversampling latency, and envelope lifecycle can
  drift silently
- runtime classes become cluttered with node-specific algorithm details
- adapters can appear to share DSP while applying incompatible reset,
  parameter-update, channel, or timing policies

Approximate ports are acceptable only as short-lived scaffolding. They must be
tracked as incomplete and must not be presented as parity implementations.

## Shared Boundary

Place shared Cycle DSP code in a dedicated namespace and folder within
`AmaranthLib`, under `lib/src/Audio/CycleDsp/` unless the module belongs in an
existing authoritative library subsystem. Do not create another CMake target
unless extraction exposes a concrete dependency cycle or independent linking
requirement.

Shared headers may depend on repository library types such as:

- `Buffer` and non-owning multichannel buffer views
- `VecOps`, FFT, convolution, oversampling, curves, meshes, and rasterizers
- narrow DSP-owned value types such as process specifications and module
  configuration snapshots

Shared headers must not depend on:

- Cycle 1 or Cycle 2 adapter, model, graph, timing, or parameter types
- `SingletonRepo`, document ownership, app undo, or transport ownership
- JUCE components, widgets, canvases, editors, or other UI classes
- application services reached through globals or singleton accessors

Cycle 1 and Cycle 2 adapters map local parameters, timing, buffers, and model
snapshots into the shared interfaces. Adapter code owns mapping and lifecycle
orchestration only; it must not contain a second implementation of the domain
algorithm.

## Processor Families

Do not force all modules through one processor abstraction. Use common buffer,
configuration, state, and lifecycle conventions across these families:

- **Streaming effects:** Delay, Reverb, IR Modeller, and Waveshaper process
  ordered time-domain samples and may retain history between calls.
- **Framed transforms:** FFT/IFFT own framing, overlap, cyclic/acyclic behavior,
  and time/frequency domain conversion.
- **Rasterized generators and state machines:** Envelope and Guide Curve turn
  model snapshots and lifecycle input into control or traversal output.

Each family may expose a specialized interface, but shared algorithmic behavior
must remain below both application adapters.

## Sharing Is The Default

Cycle 1 and Cycle 2 are two applications of the same synthesizer, not two DSP
products. Any behavior that changes generated samples, control signals,
spectra, timing, latency, tails, or rasterized curves is presumed shared until
a documented product decision proves otherwise. The burden is on divergence,
not reuse.

This includes more than conventional effects:

- curve evaluation, interpolation, mesh slicing, intercept construction,
  padding, guide-curve application, waveform baking, and rasterizer sampling
- oscillator and wavetable generation, morph traversal, phase handling,
  oversampling, unison generation, and voice-level filtering
- envelopes, loop/sustain/release policy, guide curves, scratch/control
  generation, deterministic noise, and modulation transfer
- Delay, Reverb, IR, Waveshaper, Equalizer, Phaser, Chorus, Unison, filters, and
  future effects
- FFT/IFFT framing, overlap, windows, resampling, convolution, and spectral
  transforms
- analysis algorithms such as pitch tracking and spectrogram generation when
  both products expose the same analysis feature

Existing code location does not determine ownership. A mature implementation
under `cycle/src/` is the behavioral authority to characterize, but its
signal-affecting core should move into `AmaranthLib` once UI, singleton, and
document dependencies are adapted away. Conversely, code already under
`lib/src/` is not automatically a good shared boundary if it still reaches
application services or combines configuration, execution state, and UI
presentation.

### What Remains Application-Specific

- Cycle 1 parameter objects, `SingletonRepo` aliases, updater wiring, document
  ownership, and audio-source/voice scheduling
- Cycle 2 graph nodes, ports, plan compilation, configuration publication,
  traversal-grid routing, and node/voice identity
- host/plugin buffer adaptation and application-specific channel routing
- editor components, selection, undo, repaint/invalidation, and visualization
  composition
- serialization envelopes belonging to a document or graph format; the
  immutable domain snapshot they decode into may still be shared
- deliberately qualitative previews, provided their difference from audio DSP
  is named and tested

Adapters may map parameters, timing, channels, lifecycle events, and immutable
model snapshots into shared types. They must not evaluate curves, synthesize
kernels, implement filters, perform interpolation, or reproduce state-machine
semantics locally.

### Expected End State

`AmaranthLib` owns the domain configurations, algorithms, prepared data, and
mutable execution-state types. Cycle 1 and Cycle 2 each own small adapters that:

1. translate application state into immutable shared configuration,
2. publish or install that configuration at the application's safe boundary,
3. provide per-stream mutable state and bounded scratch,
4. translate buffers and lifecycle events, and
5. report shared latency, tail, and failure information back to the app.

Cycle 2 classes named `*Dsp` or `*SignalProcessor` should therefore trend toward
orchestration-only adapters. If such a class contains domain math, curve
evaluation, kernel construction, interpolation, or a mature state machine, that
logic is a candidate for the shared layer.

### Migration Waves

1. **Rasterization substrate:** finish making mesh slicing, waveform baking,
   envelope/FX/point-list sampling, guide policies, and prepared rasterizer data
   immutable and application-neutral.
2. **Voice generation:** extract `VoiceMeshRasterizer`, `GraphicRasterizer`,
   `E3Rasterizer`, time-column behavior, morph/phase policy, oscillator transfer,
   unison, and voice filters behind shared configuration/state contracts.
3. **Remaining effects:** complete Delay, Reverb, IR, and Waveshaper, then move
   Equalizer, Phaser, Chorus, Unison, and shared filter behavior through the
   same characterize-extract-migrate sequence.
4. **Control generators:** complete Envelope and Guide Curve snapshots,
   lifecycle, deterministic noise, attachments, and scratch/control semantics.
5. **Framed/spectral DSP:** share FFT framing and overlap policy where product
   behavior matches, then spectral editing and resampling contracts.
6. **Analysis:** expose pitch, spectrogram, and related analysis through shared
   request/result types when Cycle 2 consumes them.

Each wave migrates Cycle 1 first or in lockstep because Cycle 1 remains the
authority for DSP discrepancies. Cycle 2 must not land an approximation while
waiting for extraction.

## Streaming Contract

Streaming processors must separate preparation, immutable configuration,
mutable execution state, and processing. The exact repository types may vary,
but the contract should have this shape:

```cpp
struct ProcessSpec {
    double sampleRate {};
    int maximumBlockSize {};
    ChannelLayout channelLayout {};
};

struct DelayConfig {
    double delaySeconds {};
    float feedback {};
    float spin {};
    int spinIterations {};
    float wet {};
};

class CycleDelay {
public:
    void prepare(const ProcessSpec& spec);
    void setConfiguration(const DelayConfig& config);
    void reset();
    void process(MultichannelBufferView<float> buffer);

    int latencySamples() const;
    TailLength tailLength() const;
};
```

The concrete multichannel view and channel-layout types should reuse or extend
the repository's audio buffer conventions rather than introduce parallel
storage. The interface must define supported layouts, independent versus
coupled channel state, mono-to-stereo policy, and unsupported-layout behavior.

### Realtime Requirements

- `prepare` establishes the sample rate, maximum block size, channel layout,
  and all memory bounds. It runs off the audio thread.
- Configuration that requires allocation, coefficient construction, kernel
  generation, rasterization, or oversampling setup is built off the audio
  thread and published as an immutable snapshot.
- `process` performs no allocation, locking, UI access, singleton access,
  logging, or expensive configuration rebuild.
- Every concurrently processed stream owns its mutable history and scratch
  memory. Immutable configuration may be shared when publication and lifetime
  are safe.
- Scratch storage is fully reserved during preparation, either in the
  processor state or through a bounded processing arena. Processing APIs must
  not make scratch ownership optional or ambiguous.
- Processing must accept any block size from zero through the prepared maximum
  without changing the result relative to equivalent contiguous samples.

### Lifecycle Requirements

Every stateful module must document:

- what `reset` clears and whether reset output is deterministic
- how transport discontinuities and graph recompilation reset or preserve state
- whether bypass advances, freezes, clears, or drains history
- how end-of-input flushing exposes finite or infinite tails
- which parameter changes preserve, transform, crossfade, or reset history
- what happens when sample rate, maximum block size, or channel layout changes

Processors with latency must report it in samples. Stateful effects must report
a finite tail length or an explicit infinite/unknown marker. Adapters use these
values for scheduling, bypass, traversal rendering, and graph lifecycle.

## Configuration And State

Separate the following ownership explicitly:

- **Immutable configuration:** curves, kernels, coefficients, transfer tables,
  mesh snapshots, channel policy, and parameter-derived timing values.
- **Mutable execution state:** delay lines, filter memories, convolution tails,
  overlap/carry buffers, phase accumulators, and envelope position.
- **Scratch memory:** bounded temporary buffers owned by one execution state or
  supplied by a prepared work arena.

Tempo and time-signature mapping belongs in an adapter or a small shared pure
mapping function. The streaming core should receive parameter values expressed
in its processing units, such as delay seconds, rather than reaching into an
application transport.

Cycle 2 realtime audio and traversal rendering must use separate mutable
execution states. They may share immutable configuration. Traversal rendering
must never reset, advance, or otherwise mutate realtime audio history.

## Traversal Time Contract

Traversal storage layout does not by itself define DSP time. Before a stateful
time-domain processor is applied to a traversal grid, the adapter must provide:

- the axis that represents chronological time
- origin, step, and unit for that axis
- the effective sample interval or an explicit resampling rule
- the chronological iteration order
- discontinuity boundaries between ranges

State carries only across cells that are contiguous in DSP time. It resets at
the start of a render, at a declared discontinuity, or when the grid has no
meaningful time axis. A non-time grid must not acquire delay or reverb history
merely because its values happen to be stored in columns.

When grid resolution differs from the processor sample rate, the adapter must
resample to and from the DSP timeline using an existing repository resampler or
interpolation facility. It must not reinterpret one grid cell as one audio
sample without an explicit product decision.

An intentionally qualitative preview may omit oversampling, latency, or full
tail rendering only when the module documents the difference and tests the
preview contract separately. Such a preview is not described as block/traversal
parity.

## Characterization And Extraction Policy

For each mature Cycle 1 implementation:

1. Locate the existing algorithm and all related library primitives.
2. Record UI, singleton, document, transport, and application-state dependencies.
3. Capture accepted Cycle 1 behavior with deterministic golden vectors before
   changing Cycle 1 call sites.
4. Classify suspicious behavior as intentional compatibility, a shared defect
   to fix, or an unresolved product decision.
5. Define module configuration, state, channel, parameter-transition, latency,
   tail, bypass, reset, and traversal policies.
6. Extract the UI-free core without copying the algorithm into Cycle 2.
7. Make Cycle 1 consume the extracted core and verify the characterization
   contract.
8. Make Cycle 2 consume the same core through its adapter.
9. Remove obsolete adapter-local algorithms and approximations.

Golden vectors should cover the algorithm's accepted product behavior, not
incidental object layout or adapter scaffolding. When a confirmed Cycle 1 bug
is fixed during extraction, add a regression test for the corrected shared
behavior and document the intentional golden-vector change.

## Priority Modules

### Delay

Delay is the first extraction because its state and timing behavior are easier
to characterize than Reverb convolution.

Current status:

- `CycleDsp::CycleDelay` is shared through `AmaranthLib`.
- Cycle 1 owns left and right shared processor states; Cycle 2 owns separate
  block and traversal states using the same core.
- Focused tests cover history carry, block partitioning, reset equivalence, and
  traversal isolation. Broader golden-vector characterization remains open.
- Cycle 1 ring sizing previously aliased the current write when the requested
  delay length exactly equalled its power-of-two buffer size. The shared core
  classifies this as a defect and reserves capacity strictly greater than the
  longest delay.

Target:

- extract delay-line and spin-feedback behavior from `CycDelay`
- move tempo, sample-rate, and time-signature mapping into pure adapter mapping
- preserve independent channel histories and defined stereo behavior
- preallocate all delay and spin storage during preparation/configuration
- define delay-time, feedback, spin, and wet update transitions
- keep traversal and realtime execution state separate

Tests:

- Cycle 1 characterization vectors and parameter mapping parity
- deterministic mono and stereo impulse responses
- feedback and spin behavior with numeric tolerances
- single-block versus split-block equivalence
- reset, bypass, discontinuity, and parameter-transition behavior
- traversal time mapping and state isolation from realtime audio
- no allocation or lock during processing

### Reverb

Current status:

- Cycle 1 and Cycle 2 use shared deterministic stereo-capable kernel generation
  from `CycleDsp::buildReverbKernel`.
- Cycle 1 and Cycle 2 use the shared streaming `ConvReverb` state. Cycle 2 owns
  separate block and traversal instances so preview rendering cannot mutate
  audio history.
- Cycle 1 streaming state is implemented by the UI-free `ConvReverb` in
  `AmaranthLib`. Reuse its two-stage streaming algorithm behind the shared
  Reverb core; do not duplicate it in a new class.
- Cycle 2 builds deterministic kernels during graph compilation, publishes
  immutable revisions, and prepares independent realtime/traversal convolution
  state before processing. Kernel changes currently reset tails rather than
  crossfade them.

Target:

- extract deterministic kernel configuration and streaming convolution state
- preserve room-size, damping, high-pass, wet, width, and channel behavior
- build kernels off the audio thread and define the kernel-swap transition
- report convolution latency and tail length
- use bounded prepared scratch and independent traversal/audio state

Tests:

- deterministic kernel and impulse-response golden vectors
- mono/stereo width and channel-layout behavior
- single-block versus split-block streaming equivalence
- kernel changes preserve, crossfade, or reset state according to policy
- tail flushing, bypass, latency, traversal timing, and state isolation
- no allocation or lock during processing

### IR Modeller

Current status:

- Cycle 2 uses `FXRasterizer` plus the shared streaming `BlockConvolver`, with
  separate block and traversal state.
- Cycle 1 and Cycle 2 share the authoritative impulse-length, post-gain, and
  cubic prefilter mappings. Both apply the prefilter to the impulse in the
  frequency domain before convolution.
- Both audio adapters use the shared Cycle 1 raster helper: sample the bipolar
  rasterizer perfectly at 2x, then filter and downsample with `Oversampler(8)`.
- A shared-core golden vector locks the complete curve-to-kernel path, including
  oversampler transients and the frequency-domain prefilter.
- Cycle 2 adapter tests cover post-gain and prefilter policy, disabled
  passthrough, channel metadata, split-block equivalence, first-column traversal
  equivalence, and complete minimum-kernel tail output.
- The macOS Accelerate oversampler uses bounded, preallocated FIR history and
  scratch buffers; the former out-of-bounds filtering made IR kernels
  nondeterministic and was exposed by the split-block fixture.
- Cycle 2 now publishes graph-wide immutable, revisioned configurations for IR,
  Reverb, Waveshaper, and Envelope. Retained per-node/per-voice processors adopt
  them through explicit execution preparation while keeping realtime and
  traversal state separate. The implemented boundary and module transition
  policies are recorded in `cycle-v2-dsp-configuration-publication.md`.

Target:

- share exact curve-to-impulse rasterization with Cycle 1
- use a streaming convolution state rather than truncating block-local output
- define high-pass, post-gain, channel, kernel-update, latency, and tail policies
- keep editable `Panel2D` curve semantics exact through immutable mesh snapshots

Tests:

- curve-to-impulse golden vectors (shared core covered; adapter fixtures remain)
- high-pass, post-gain, channel metadata, and disabled/bypass behavior (Cycle 2 covered)
- split-block equivalence and complete tail flushing (Cycle 2 covered)
- block/traversal equivalence only where the traversal time contract permits it

### Waveshaper

Current status:

- Cycle 1 and Cycle 2 share rasterizer/table semantics and the same `Oversampler`
  implementation for realtime audio.
- Cycle 2 traversal and Cycle 1 graphic processing intentionally bypass
  oversampling because columns are independent visualization slices; this is a
  documented qualitative preview rather than an audio-equivalent render.
- `Oversampler` can be constructed without application ownership and accepts
  explicit bounded scratch memory. The legacy Cycle 1 constructor remains as
  an adapter that obtains its existing pool from `SingletonRepo`.
- Cycle 2 builds and publishes immutable transfer tables, gain mappings, and
  oversampling specifications during graph compilation. Retained processors
  reserve oversampling scratch during execution preparation.

Target:

- extract transfer, pre/post gain, AA/oversampling, and latency behavior
- build transfer tables and oversampling configuration off the audio thread
- define table-swap behavior and channel policy
- document whether traversal is audio-equivalent or an intentionally
  qualitative non-oversampled preview

Tests:

- transfer-table golden vectors and pre/post gain
- oversampling, alias-reduction, latency, and block-size independence
- table changes and reset behavior
- explicit traversal-preview contract

### Envelope

Current status:

- Envelope UI/rasterizer semantics have been partially bridged.
- `EnvRasterizer` is a plain AmaranthLib object with no `SingletonRepo`
  ownership dependency. Cycle 1 preserves its named global services through
  Cycle-owned composition adapters in `EnvRasterizerServices.h`.
- Cycle 2 persists editable envelope meshes as immutable JSON snapshots using
  the mature `EnvelopeMesh` serialization contract, including morph vertices
  and loop/sustain markers.
- Cycle 2 graph executors retain processors by node identity across blocks, so
  per-voice envelope and streaming-effect state has a stable owner.
- Cycle 2 envelope processing uses `EnvRasterizer` with sample-offset note-on,
  note-off, reset, and retrigger events. Processor ownership is isolated by
  node and voice identity, and active-note snapshot edits preserve validated
  playback position.
- Cycle 2 parses, validates, and rasterizes envelope snapshots during graph
  compilation. Prepared rasterizer data is published immutably and deep-copied
  into private per-voice playback state while preserving validated active-note
  position.

Target:

- share envelope rasterization, loop/sustain markers, log mode, morph axes,
  note lifecycle, and release behavior
- represent editable data as immutable snapshots separate from per-voice state
- define behavior when a snapshot changes during an active note
- keep control generation distinct from streaming-effect interfaces
- define a Cycle 2 envelope snapshot and lifecycle adapter that maps explicitly
  to `EnvelopeMesh`, `MeshLibrary::EnvProps`, and `EnvRasterizer` state

Tests:

- loop seam continuity and sustain/release marker stability
- note-on, note-off, reset, retrigger, and active-note edit behavior
- log/linear rendering and red/blue morph behavior
- vector output multiplication across traversal grids
- independent per-voice and traversal state

### FFT/IFFT

Current status:

- Cycle 2 uses the shared `Transform` primitive. Its blockwise class owns the
  single graph-runtime framing and half-cycle carry policy, while gridwise
  processing delegates each column to that same class; no duplicate FFT
  algorithm or second framing implementation was found.

Target:

- audit whether framing, chunking, overlap, and cyclic/acyclic policy are
  duplicated or application-specific
- extract only shared framed-transform policy; do not create another FFT
  implementation
- define frame size, hop size, window, padding, latency, and discontinuity rules

Tests:

- time-to-frequency and frequency-to-time golden vectors
- cyclic and acyclic overlap behavior
- split-input equivalence, latency, reset, and discontinuity handling
- block/traversal consistency when both use the same framed timeline

### Guide Curve

Current status:

- Guide panel and preview behavior are UI-bridged.
- Runtime signal semantics are not yet treated as shared core behavior.
- Cycle 2 exposes noise/DC/phase parameters but has no curve snapshot or
  `GuideCurveProvider` adapter, and its audio role currently publishes no
  output. Add that model boundary before signal processing; do not synthesize a
  placeholder curve locally.

Target:

- audit guide sampling, noise, DC, phase, and attachment behavior
- reuse existing guide sampler and rasterization policies
- extract only signal-affecting model/rasterization behavior that is duplicated
- make noise seeding and snapshot-update behavior deterministic and explicit

Tests:

- guide sampling golden vectors
- deterministic noise, DC, and phase behavior
- attachment and snapshot-update behavior where applicable

## Test Strategy

Tests should protect behavior at three boundaries:

1. **Core tests:** deterministic golden vectors, numeric tolerances, state
   transitions, channel behavior, and lifecycle contracts.
2. **Streaming invariants:** single-block versus split-block equivalence across
   zero, one, irregular, and maximum prepared block sizes; reset reproducibility;
   latency and tail reporting; and allocation-free processing.
3. **Adapter tests:** exact parameter/timing/unit mapping, supported layout
   conversion, correct state ownership, and proof that traversal cannot affect
   realtime history.

Prefer direct output comparisons over qualitative assertions such as "contains
a tail." Tests must fail if an adapter replaces the shared core with a local
approximation, but they should not duplicate the shared algorithm in test code.

## Work Plan

Complete one module end-to-end before starting the next:

1. Establish the `AmaranthLib` Cycle DSP folder/namespace and common lifecycle
   value types only as required by Delay.
2. Characterize Delay and classify suspicious legacy behavior.
3. Define Delay policies and extract its shared core.
4. Migrate Cycle 1, then Cycle 2, to the shared Delay core.
5. Add core, streaming-invariant, adapter, traversal, and realtime-safety tests.
6. Remove obsolete Delay approximation code and update this checklist.
7. Repeat the same gated sequence for every signal-affecting module, beginning
   with Reverb, IR Modeller, Waveshaper, Envelope, and the rasterization/voice
   substrate.
8. Continue through Equalizer, Phaser, Chorus, Unison, voice filters,
   oscillator generation, FFT/IFFT, Guide Curve, and shared analysis features.

## Progress Checklist

- [x] Establish shared namespace/folder and Delay-required configuration/state boundary.
- [ ] Characterize and classify Cycle 1 Delay behavior.
- [ ] Document Delay lifecycle, channel, update, latency, tail, and traversal policies.
- [x] Extract Delay core and migrate Cycle 1.
- [x] Migrate Cycle 2 Delay and remove its local algorithm.
- [ ] Complete Delay golden-vector, channel, adapter, traversal-time, and realtime-safety tests.
- [x] Remove `ConvReverb`'s unused application/singleton ownership dependency.
- [ ] Characterize, extract, migrate, and test Reverb.
- [ ] Characterize, extract, migrate, and test IR Modeller.
- [ ] Characterize, extract, migrate, and test Waveshaper.
- [ ] Characterize, extract, migrate, and test Envelope.
- [x] Remove `EnvRasterizer`'s application/singleton ownership dependency.
- [x] Add Cycle 2 envelope mesh snapshot persistence.
- [x] Retain Cycle 2 node processors across graph audio blocks.
- [x] Integrate Cycle 2 envelope snapshots and lifecycle with `EnvRasterizer`.
- [x] Publish immutable Cycle 2 configurations for Reverb, IR, Waveshaper, and Envelope.
- [ ] Inventory Cycle 1 rasterizer/voice-generation dependencies and define shared snapshots.
- [ ] Move application-neutral `VoiceMeshRasterizer`, `GraphicRasterizer`, `E3Rasterizer`, and time-column policy into `AmaranthLib`.
- [ ] Characterize, extract, migrate, and test Equalizer, Phaser, Chorus, and Unison.
- [ ] Extract shared oscillator, morph/phase, voice-filter, and voice-unison behavior.
- [x] Audit FFT/IFFT framed-transform policy and extract only if duplicated.
- [x] Audit Guide Curve signal behavior and extract only if duplicated.
- [ ] Add Cycle 2 Guide Curve snapshot/provider adapter before runtime sampling.
- [ ] Remove or clearly label every remaining approximate adapter.

## Definition Of Done

This TDD is complete when:

- Cycle 1 and Cycle 2 share the algorithmic core for every complex node listed
  above, or a tested product-level divergence is documented.
- Shared processors have explicit preparation, configuration, state, channel,
  reset, bypass, discontinuity, latency, tail, and parameter-transition policy.
- Realtime processing is bounded, allocation-free, lock-free, and independent
  of UI and application services.
- Traversal rendering uses an explicit DSP timeline and cannot mutate realtime
  execution state.
- Golden-vector and streaming-invariant tests detect behavioral drift across
  block sizes and adapters.
- Runtime graph executors and node processors contain adapter/orchestration
  logic only, not mature DSP algorithms.
