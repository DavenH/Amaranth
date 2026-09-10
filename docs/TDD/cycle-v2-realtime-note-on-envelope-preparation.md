# Cycle V2 Realtime Note-On Envelope Preparation

## Status

Implemented on 2026-09-10.

## Decision

Cycle V2 must materialize a note's routed envelope cross-section synchronously
at the note-on sample offset. Voice activation must not wait for a worker or
begin with a persistent/base-morph envelope while note-specific preparation is
pending.

The synchronous path must be lock-free, allocation-free, and bounded by
capacity established before the graph generation is accepted for audio. This
TDD governs initial note-on preparation. Authoring edits and graph compilation
remain non-realtime, and active-note live morph adoption remains governed by
`cycle-v2-dynamic-envelope-modulation.md`.

## Authoritative Behavior

Cycle 1 establishes the audible contract:

- `SynthAudioSource::processBlock()` calls `Synthesiser::renderNextBlock()` on
  the audio callback;
- JUCE dispatches a note-on to `SynthesizerVoice::startNote()` at its sample
  offset;
- `startNote()` routes key and inverse velocity, then calls
  `initialiseEnvMeshes()` before rendering the note; only legacy envelope
  layers marked `dynamic` consume those routed values, while static layers
  retain their existing cross-section;
- `initialiseEnvMeshes()` materializes every active volume, pitch, and scratch
  envelope at the routed red/blue cross-section; and
- the first sample after the event therefore observes the routed envelope.

The legacy repository performs the same sequence with direct
`calcCrossPoints()` calls. Cycle 1's timing and routed-value semantics are
authoritative. Its mutex acquisition, UI-owned modulation routing, mutable
mesh access, and allocation-capable rasterizer call are implementation defects
and must not be copied.

The mature behavior to reuse unchanged is held in the shared rasterization
pipeline:

- `TrilinearMeshSlicer` for the red/blue mesh cross-section;
- intercept reduction, ordering, degeneracy, and padding policies;
- envelope marker and sustain/loop policies;
- curve preparation and waveform baking policies;
- guide-curve deformation, scaling, and deterministic seed behavior; and
- `EnvelopePlaybackEngine` for note, sustain, loop, and release lifecycle.

No Cycle V2 node-local version of those algorithms is permitted.

## Problem

`EnvelopeSignalProcessor` currently owns an immutable base preparation. At
note-on it reads routed red/blue controls and publishes a request to the
non-realtime preparation exchange. The voice has already started by the time
that result can be serviced and adopted. A note whose key or velocity changes
its envelope cross-section therefore emits initial audio from the authored
base morph.

Guitar 3 G demonstrates the discrepancy. At MIDI 48/frame zero, Cycle 1's
static scratch envelope begins at its legacy 0/0 cross-section with coordinate
`0.00553`, while Cycle V2 begins at `0.22683` from the imported 0.5/0.5
preparation. The converter now represents that static legacy ownership with
explicit constant inputs; dynamic Cycle V2 envelopes use the routed note-on
path specified here. Delaying the note until the requested result is
ready would make scheduling latency dependent on worker timing and is outside
the product contract.

Calling the existing `EnvRasterizer::renderWaveformOnly()` from the callback is
also unacceptable. Its general-purpose data structures can resize, and the
legacy call chain can acquire UI/modulation locks. Exact parity therefore
requires an extracted realtime materialization core rather than reuse of the
legacy orchestration.

## Target Design

Preparation is split into a non-realtime plan and a realtime materialization:

```text
Envelope mesh + authored policy + guide-curve snapshot
        |
        | graph-generation preparation, non-realtime
        v
RealtimeEnvelopePlan
  immutable topology and policy inputs
  exact required workspace capacities
  no UI/model ownership
        |
        | note-on effective red/blue + deterministic voice seeds
        | synchronous, at the MIDI sample offset
        v
RealtimeEnvelopeMaterializer
  shared mature slicing/policy core
  voice-owned fixed-capacity workspace
        |
        v
PreparedEnvelopePlaybackView -> EnvelopePlaybackEngine::noteOn()
```

The accepted graph generation owns an immutable `RealtimeEnvelopePlan` for
each envelope configuration. Each prepared voice owns independent workspace
and result storage sized for that plan. Note-on supplies only scalar routed
controls, lifecycle identity, and already-derived deterministic seeds.

The materializer writes into the inactive side of a voice-local double buffer.
Only after successful completion does it swap the active prepared view and
start/reset envelope playback. No partially written result is observable.
Because note-on and voice rendering occur serially on the same audio callback,
this swap needs no mutex or cross-thread publication primitive.

### Capacity Contract

Graph-generation preparation calculates and validates upper bounds for:

- sliced intercept and reduction storage;
- display/render padding and envelope curves;
- baked waveform samples and loop representation;
- guide-curve regions and offset seeds; and
- any ordering or policy scratch storage.

Voice-pool preparation allocates those capacities before the generation is
published to audio. A configuration whose requirements exceed supported hard
limits fails generation preparation with a diagnostic; the previous complete
generation remains active. The callback must never grow a container, allocate
overflow storage, truncate geometry, use a lower-resolution approximation, or
defer activation.

Capacity is a property of the immutable plan, not a guessed per-note maximum.
Normal note-on work is bounded by that accepted plan's declared topology and
waveform limits.

### Shared Rasterization Core

Existing rasterization policies currently accept owning vectors and mutable
`EnvRasterizer` state. Where those interfaces permit implicit growth, extract
storage-neutral operations that accept caller-provided bounded spans or a
fixed-capacity writer. `EnvRasterizer` must call the extracted core for its
ordinary non-realtime/UI use; the realtime materializer calls the same core
with voice-owned storage.

The extraction may translate storage, request, and result views. It must not
copy interpolation, curve, topology, marker, guide, or waveform-baking logic.
The stable end state is one behavioral core with two storage/lifecycle hosts,
not a permanent parity implementation beside `EnvRasterizer`.

### Event Ordering

For a note-on at offset `k` within a host block:

1. render prior active voices through sample `k - 1`;
2. resolve that voice's key, velocity, and connected absolute red/blue inputs;
3. materialize each required initial envelope into its prepared voice storage;
4. initialize playback against those completed views; and
5. render the new voice beginning at sample `k`.

The effective morph is not smoothed from the authored base value at note-on.
It is the routed value at the event boundary, matching Cycle 1. An unconnected
axis uses its authored base value. Note-off, loop, release, and traversal rules
remain those of the shared playback engine.

## Thread And Ownership Boundaries

The non-realtime preparation owner may:

- snapshot graph/model geometry and guide curves;
- calculate capacity requirements and reject unsupported configurations;
- allocate immutable plans and per-voice workspace; and
- initialize shared lookup tables before audio begins.

The audio callback may:

- read the adopted immutable plan;
- write only the selected voice's preallocated inactive workspace;
- run the bounded shared materialization core; and
- swap that voice's active result before rendering its first sample.

The audio callback may not:

- acquire a mutex or call UI/model code;
- allocate, resize, destroy shared ownership, or initialize global tables;
- read a mutable `Mesh`, `NodeGraph`, or guide-curve provider;
- compile or publish a graph/configuration;
- wait for non-realtime work; or
- emit base-morph audio as a fallback for a routed note.

## Realtime Complexity

Let `C` be the number of cubes in the accepted envelope plan, `I` its maximum
cross-section intercept count, and `W` its maximum baked waveform length.
Note-on materialization is bounded by the mature pipeline's equivalent of
`O(C + I log I + W)`, using fixed-capacity storage. There is no dependence on
host block size, worker scheduling, UI state, or prior request backlog.

The implementation must instrument elapsed preparation work and capacity high-
water marks in tests. A hard realtime duration cannot be guaranteed on every
host, but accepted plans must have explicit finite bounds and must not contain
unbounded retry or dynamic-growth paths.

## Failure Semantics

Capacity or topology failure belongs to non-realtime graph preparation. Once a
generation is adopted, note-on materialization is expected to succeed for
every valid morph in `[0, 1]`.

An invariant violation discovered on the callback must fail closed: report it
through preallocated diagnostics and keep the affected voice silent. It must
not run an allocating fallback, activate late, or use the wrong cross-section.
Tests must make this path unreachable for every accepted plan.

## Semantic Tests

### First-Sample Parity

- Guitar 3 G at MIDI 48 produces the Cycle 1 scratch coordinate at frame zero,
  including the existing `0.00553` diagnostic point.
- Multiple notes with different key and velocity values each use their routed
  cross-section on their first rendered sample.
- A note-on at a nonzero block offset changes output exactly at that offset;
  host block partitioning does not change the rendered bytes.
- Unconnected red or blue axes use their authored values independently.
- Repeated identical notes with fixed seeds produce byte-identical envelope
  buffers and deterministic final audio.

### Mature-Core Equivalence

- Across boundary and interior red/blue values, the realtime materializer and
  `EnvRasterizer` produce identical intercepts, marker indices, loop metadata,
  waveform sizes, and waveform bytes from the same immutable input.
- Fixtures cover volume, pitch, scratch, sustain/release, loops, guide curves,
  logarithmic/bipolar scaling, degenerate slices, and low-resolution curves.
- Empty or unsampleable envelopes preserve existing neutral/silence semantics.

### Realtime Boundary

- Instrumented note-on processing performs zero heap allocations and zero
  mutex acquisitions.
- Note-on performs no graph/model/UI access, shared-pointer destruction, table
  initialization, snapshot publication, or non-realtime service call.
- Accepted maximum-capacity fixtures do not resize and report bounded high-
  water marks.
- Oversized configurations are rejected before publication; no voice can reach
  an overflow fallback.

### Lifecycle

- Playback starts only after the routed result is complete, without additional
  sample latency.
- Note-off, release normalization, looping, retrigger, and declick behavior are
  unchanged after adopting the per-note result.
- Polyphonic simultaneous note-ons use isolated workspaces and do not alter one
  another's prepared results.

## Implementation Slices

1. Characterize current and legacy Cycle 1 note-on ordering and add the Guitar
   3 G first-sample regression before changing production behavior.
2. Inventory every allocation, resize, lock, mutable owner, and static lazy
   initialization reached by `EnvRasterizer::renderWaveformOnly()`.
3. Extract storage-neutral shared rasterization operations from the mature
   policies. Keep `EnvRasterizer` on that core and prove byte equivalence.
4. Introduce immutable `RealtimeEnvelopePlan`, exact capacity calculation, and
   generation-rejection diagnostics.
5. Add per-voice preallocated double-buffered workspace during ordinary voice-
   pool preparation.
6. Materialize the routed initial envelope synchronously at the note-on event
   boundary and remove the note-on request/adoption transition.
7. Add realtime instrumentation, maximum-capacity, polyphony, block-split, and
   end-to-end Cycle 1/Cycle V2 parity tests.
8. Delete note-start compatibility state from
   `EnvelopePreparationExchange`; retain only pieces justified by the separate
   future live-adoption contract.

## Expected Production Change

Expected new or substantially changed production code:

- a shared bounded envelope materialization core and storage/result views in
  `lib/src/Curve/Rasterization/`;
- immutable plan and capacity types near Cycle V2 envelope configuration;
- voice-owned workspace preparation in the existing prepared-generation/voice
  lifecycle; and
- narrow note-on orchestration changes in `EnvelopeSignalProcessor` and its
  caller.

The expected production change is approximately 500-900 lines plus focused
tests, with the largest algorithmic change in the shared rasterization library.
Repeated rasterization policy code, a large `NodeKind` switch, or substantial
algorithmic growth inside `EnvelopeSignalProcessor` is evidence that the
design has been violated and requires review.

## Deletion Targets

- Note-on use of `LatestEnvelopePreparationRequest` and
  `PreparedEnvelopeExchange`.
- The adoption transition used solely to mask a late initial morph result.
- Any test that blesses base-morph output before routed preparation arrives.
- Any direct Cycle V2 copy of shared mesh slicing or envelope preparation.

The exchange may remain for a later active-note live-adoption policy only if
that responsibility is explicit and no note-start behavior depends on it.

## Completion Criteria

- The first sample of every accepted note uses the routed key/velocity/control
  envelope cross-section at that MIDI event's sample offset.
- Voice activation is never delayed for envelope preparation.
- Note-on materialization is allocation-free, lock-free, capacity-bounded, and
  independent of non-realtime scheduling.
- Cycle 1 and Cycle V2 exercise one shared rasterization behavior rather than
  parallel implementations.
- Capacity overflow is rejected before a graph generation reaches audio.
- Mature loop, sustain, release, scaling, guide-curve, and deterministic seed
  semantics remain intact.
- Guitar 3 G advances past the documented scratch-envelope mismatch and can be
  used to attribute the next parity discrepancy.
- All deletion targets are complete, and the implementation review records
  production diff size, largest files, new type branches, extracted mature
  sources, realtime instrumentation, and focused parity evidence.

## Implementation Review

The completed production change adds 646 lines and removes 463 lines across
the shared rasterization host, Cycle V2 envelope orchestration, and the Guitar
3 G conversion boundary, including deletion of the 243-line worker preparation
exchange. The largest new file is `EnvelopeMaterialization.cpp` at 379 lines;
the largest edited production file is `EnvelopeSignalProcessor.cpp`, which is
93 lines smaller after deleting the worker request/adoption and transition
path. No `NodeKind` branch was added.

The extracted shared behavior remains in `TrilinearMeshSlicer`, the intercept,
marker, sustain, padding, resolution, curve-preparation, waveform-bake, guide,
and playback policies. `EnvRasterizer` and `RealtimeEnvelopeMaterializer` now
call the same materialization function. The realtime host owns two prepared
result slots, reserves the accepted plan's intercept, curve, guide-region,
colour-point, and waveform capacities before audio, and swaps only after a
complete successful bake. Diagnostics record attempts, failures, elapsed
nanoseconds, and capacity high-water marks.

Focused evidence covers byte-identical mature-host/realtime intercepts,
curves, loop metadata, and waveform samples; capacity rejection; exact
nonzero-offset activation; independently authored input fallback; voice-state
isolation; zero callback allocations and mutex acquisitions; and Guitar 3 G's
frame-zero `0.00553` point. The canonical Cycle 1 export also established that
Guitar 3 G's volume and scratch layers are `dynamic=false`; the converter now
pins those legacy static envelopes to 0/0 explicitly instead of accidentally
feeding the graph-wide modulation triple. A fresh MIDI 48 paired capture is
byte-identical through both frame-zero magnitude rasters and effective morph
triples; it attributes the next discrepancy to a uniform `1.087450` gain
difference in spectral range shaping.
