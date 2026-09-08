# Cycle V2 Live Spectral-Frame Modulation

## Status

Implemented on 2026-09-08. Prepared spectral regions now resolve live controls
and scratch state on the shared synthesis-cycle timeline, rerasterize evolving
frames at cycle frontiers, and share each frame across Unison lanes. The
runtime is byte-identical across host block partitions and repeatable across
notes and fresh processes.

Fresh differential renders still differ from Cycle 1 at the captured
effect-free final voice-output boundary. That remaining end-to-end parity work
is retained in `docs/TDD/audio-bugs.md`; the prior frozen-frame cause is
resolved and is no longer hidden behind that final waveform metric.

This document is intentionally independent of the completed harmonic-region
correction. The implementation at the start of this work rendered one prepared
spectral frame after note reset and did not reproduce Cycle 1's evolving
per-cycle rasterization.

## Goal

Make Cycle V2's prepared spectral oscillator consume the same live modulation
state, at the same synthesis-cycle boundaries, as Cycle 1. Time, magnitude, and
phase meshes must evolve with voice time, key, velocity, MIDI controls, and
scratch envelopes without making the result depend on host block size.

The completed implementation must preserve the prepared region's realtime
properties: no audio-thread allocation, no graph mutation, no UI/model access,
and no duplicate implementation of modulation, rasterization, spectral shaping,
or cyclic reconstruction algorithms.

This TDD removes one major source of V1/V2 audio divergence. It does not by
itself claim final-output byte parity while separately documented gain,
resampling, onset, effect-state, and Cycle 1 determinism differences remain.

## Problem Statement

`GraphAudioExecutor` already renders default modulation sources into blockwise
control buffers from `AudioVoiceContext`. Ordinary `TrimeshAudioProcessor`
instances consume those buffers, smooth their morph targets, and render the
current mesh state.

Prepared oscillator regions bypass those ordinary processors. During
`SpectralOscillatorFrameRenderer::prepare()`, each mesh operation retains a
`TrimeshConfiguration` containing a static morph. On the first call after
reset, `SpectralOscillatorRegionRuntime::renderSharedFrame()` renders that
configuration once. The runtime then reuses the frame for the lifetime of the
note.

Consequences include:

- voice-time motion is frozen;
- key and velocity mappings are evaluated as preset defaults rather than live
  note state;
- timed controller changes do not reach the prepared frame;
- scratch-envelope traversal does not advance spectral meshes;
- current/previous frame interpolation never represents evolving authored
  content;
- results can sound static even when the graph and editor previews evolve.

`filter-saw` is the minimal deterministic reproducer. It contains one time mesh
and one subtractive magnitude mesh, with no phase layer, effects, unison, or
guide noise. Its exact port remains substantially different from Cycle 1 after
the harmonic-region correction, and short-window comparisons change with voice
time. `dunk-2` and `pwm` are broader audible evolution regressions.

## Authoritative Implementations

The implementation must reuse these existing owners rather than reproduce
their behavior in an adapter:

- `cycle/src/Audio/Voices/SynthesizerVoice.cpp` owns Cycle 1 note lifecycle,
  routed key/velocity state, envelope initialization, and oscillator selection.
- `cycle/src/Audio/Voices/CycleBasedVoice.cpp` owns Cycle 1 cycle-frontier
  advancement, future/current frame scheduling, per-lane interpolation, and
  frame-to-output resampling.
- `cycle/src/Audio/Voices/SynthFilterVoice.cpp` owns the mature ordering of time
  rasterization, forward FFT, magnitude layers, phase layers, harmonic cutoff,
  and inverse FFT.
- `ModulationSource::renderAudioBlock()` owns Cycle V2 evaluation of voice time,
  key, velocity, pressure, mod wheel, and timed MIDI controllers.
- `TrimeshAudioProcessor` and `SmoothedMorphPosition` own Cycle V2's existing
  mapping from control inputs to a mesh morph. Prepared execution must share or
  extract this behavior; it must not implement a second morph policy.
- `Rasterization::ScratchPositionPolicy` owns scratch-domain and primary-axis
  semantics.
- `TrimeshBlockwiseDsp`, `OscillatorLaneRasterizer`, `SpectralLayerCore`, and
  `CyclicFrameLaneRenderer` own mesh sampling, range shaping, spectral
  compositing, and frame reconstruction.
- `RealtimeGraphRenderer` owns normalized voice-time advancement and MIDI voice
  lifecycle. `GraphAudioExecutor` owns prepared graph execution and graph-buffer
  routing.

Before implementation, characterize the precise Cycle 1 update sequence for a
cycle crossing a host-block boundary. Record whether each routed parameter is
sampled at the current frontier, future frontier, or through an existing
smoothed property. That characterization is part of the design evidence and
must not be replaced by an assumed block-start policy.

## Architectural Boundary

Introduce a narrow, read-only prepared-oscillator process context. It should
carry the current `AudioVoiceContext`, the segment's absolute/block-relative
sample coordinates, and addressed control/attachment buffers already produced
by `GraphAudioExecutor`. It may translate buffer ownership and timing metadata;
it must not evaluate modulation or rasterize meshes itself.

At preparation time, bind each prepared mesh operation to its existing graph
input addresses:

- yellow, red, and blue absolute morph inputs;
- scratch processing attachment;
- the operation's static parameter morph as fallback;
- voice context and lifecycle seed;
- output domain and primary axis.

At render time, a shared morph resolver must apply the same input selection and
smoothing policy used by `TrimeshAudioProcessor`. The spectral renderer receives
the resolved `MorphPosition` and scratch coordinate for a specific synthesis
frontier and delegates actual rasterization to the existing DSP objects.

This is an input/lifecycle adapter around mature behavior. It must not contain
node-kind switching beyond the oscillator recipe operations already owned by
`SpectralOscillatorFrameRenderer`, and it must not grow a parallel control-graph
executor.

If arbitrary explicit control subgraphs cannot safely supply cycle-frontier
values through the existing graph buffers, the prepared-region compiler must
reject that region and use an exact existing execution path. It must not freeze
the value, sample only at block start, or silently ignore the connection.

## Cycle-Frontier Contract

Prepared frames are generated on the center oscillator's synthesis-cycle
timeline, matching Cycle 1's shared future-frame cadence:

1. A note-on resets the frame clock, morph smoothing, scratch position, current
   and previous frames, lane carry, and lifecycle-seeded rasterizers.
2. Before a frame is needed, the runtime resolves control values at that
   frame's authoritative Cycle 1-equivalent frontier.
3. The time mesh is rasterized, then magnitude and phase processing run in
   graph order, followed by the existing harmonic cutoff and IFFT.
4. The former current frame becomes previous; the newly rendered frame becomes
   current.
5. Each lane consumes the same frame pair while retaining its own detune,
   phase, cycle clock, half-frame carry, and output resampling state.
6. A frame is rendered once per shared frontier, not once per lane and not once
   per host block.
7. Rendering a request as one block or as any partition of blocks produces
   byte-identical output and identical frame-render counts.

The runtime must not render a future frame using control events that have not
yet become visible. If whole-cycle output buffering crosses the current host
block, the frame is still resolved at its cycle frontier and the resulting
audio may be buffered; control evaluation itself may not look beyond the known
timeline.

## Modulation Semantics

- Voice time uses `normalizedVoiceTime` and its per-sample increment from
  `RealtimeGraphRenderer`; it is not recomputed from block count.
- Key and velocity use the existing `ModulationSource` normalization and the
  actual voice's note/velocity.
- Mod wheel, channel pressure, and MIDI CC use the existing timed-event buffers.
  A change affects the first authoritative frame frontier at or after its sample
  position.
- Static constants remain valid fallbacks when a morph input is unconnected.
- Existing smoothing must advance by elapsed samples between frame frontiers,
  including a frontier crossing a block boundary.
- Scratch uses its authored envelope output and
  `ScratchPositionPolicy::resolve()` for the mesh domain/primary axis. It does
  not substitute voice time when the attachment is enabled and sampleable.
- Pitch-envelope processing remains owned by the oscillator lane clock. This
  work must not merge pitch and mesh-morph timelines.
- Lifecycle seeds remain stable for repeated offline renders. Frame refreshes
  must advance random/guide state exactly once per authoritative rasterization.

## Realtime and Ownership Requirements

- Allocate operation bindings, control-address tables, frames, morph state, and
  scratch storage during prepared-graph publication.
- Do not allocate, lock, parse models, traverse `NodeGraph`, or publish graph
  state on the audio thread.
- Prepared state is per synth voice. Immutable meshes/configurations may be
  shared; frame clocks, smoothing, rasterizers, RNG state, and carry buffers may
  not be shared between active voices.
- A graph publication replaces prepared configurations at the existing safe
  boundary. It must not splice a new mesh into a frame already being rendered.
- Diagnostic capture may observe frame products through a preallocated hook,
  but diagnostics must not become the production data path.

## Negative Boundaries

- Do not update the morph once per host block. That makes audio block-size
  dependent and is not Cycle 1 parity.
- Do not rerender once per output sample. Cycle 1's authoritative cadence is the
  synthesis-cycle frontier.
- Do not reuse preview traversal grids as realtime audio frames.
- Do not copy `ModulationSource`, `SmoothedMorphPosition`, scratch, rasterizer,
  FFT, or cyclic-compositor logic into the prepared runtime.
- Do not special-case Filter Saw, PWM, Dunk 2, or converted preset node IDs.
- Do not make every unison lane rerasterize identical shared graph content.
- Do not add audio-thread allocation as temporary scaffolding.
- Do not loosen parity thresholds to bless static or blockwise approximation.
- Do not claim completion from visible editor animation; assert rendered audio
  and captured mature DSP boundaries.

## Implementation Slices

### 1. Characterize Cycle 1 frontier timing

Add focused tests or an automation-only diagnostic hook around the existing
Cycle 1 frame boundary. Capture frontier sample, resolved morph, scratch value,
frame index, and current/previous transition for a deterministic evolving mesh.
Keep the hook observational and preallocated.

Completion evidence:

- a cycle crossing a block boundary has an unambiguous expected sequence;
- two block-size partitions report the same frontier sequence;
- a timed controller and scratch transition have specified inclusion rules.

### 2. Add typed prepared-process context

Replace the loose `midiNote`, `velocity`, and pitch-buffer argument list with a
typed context that retains the full voice and segment timing while referencing
already-rendered control buffers. Thread it through `PreparedOscillatorRegion`,
`GraphAudioExecutor`, and both prepared runtime implementations without changing
audio output.

Completion evidence:

- existing static/chained/spectral tests remain byte-identical;
- note events inside a block preserve their exact offsets;
- no new allocation occurs during `process()`.

### 3. Share morph resolution

Extract the narrow morph-input selection and smoothing behavior currently
embedded in `TrimeshAudioProcessor` into a reusable per-voice component. Bind
prepared mesh operations to graph control/attachment addresses during
preparation.

Completion evidence:

- ordinary and prepared mesh paths resolve the same morph sequence for the same
  controls and elapsed-sample sequence;
- static fallback, key, velocity, voice time, and timed controller tests cover
  complete note sequences;
- unsupported explicit control topology prevents prepared-region selection.

### 4. Refresh single-lane spectral frames

Give `SpectralOscillatorRegionRuntime` one shared frame clock. At each Cycle
1-equivalent frontier, resolve all prepared mesh operations, shift current to
previous, and render the next frame through `SpectralOscillatorFrameRenderer`.
Reuse `CyclicFrameLaneRenderer` for transition and carry semantics.

Completion evidence:

- a yellow-varying magnitude mesh produces more than one distinct frame;
- static content remains byte-identical to its pre-change render;
- 64, 127, 256, and 512-sample host partitions produce byte-identical output;
- repeated notes reset to byte-identical frame and audio sequences.

### 5. Restore scratch and timed-control semantics

Feed prepared operations their existing scratch attachment and timed control
values at cycle frontiers. Cover transitions before, on, and after a frontier.

Completion evidence:

- scratch-driven Filter Saw differs from voice-time traversal when authored to
  do so;
- a controller event's effect begins at the characterized frontier;
- disabling scratch restores the default modulation path;
- no event is replayed across a zero-length or zero-internal-sample segment.

### 6. Restore shared-frame unison behavior

Generate each shared graph frame once while allowing each lane to keep its own
clock, detune, pan, phase, resampling spillover, and previous-half carry. Follow
Cycle 1's existing single-frame versus multi-lane transition semantics.

Completion evidence:

- frame-render count is independent of unison order;
- mono and hard-panned lanes use the same frame content before lane transforms;
- block partition and repeated-note equality remain exact;
- no lane reads a frame transition produced for a different frontier.

### 7. Differential preset validation

Regenerate candidate graphs from fresh Cycle 1 canonical exports and run fresh
processes with fixed seeds. Validate in this order:

1. static Saw negative control;
2. Filter Saw magnitude evolution;
3. PWM time-domain evolution;
4. Dunk 2 short-lived time regions;
5. Japan Drum multi-layer magnitude/phase evolution;
6. one scratch-envelope preset;
7. one deterministic unison preset.

For each preset, test MIDI 36, 48, 60, and 72, at least two note lengths, two
consecutive notes in one voice slot, and host block sizes 64, 127, 256, and 512.
First require byte repeatability within each engine, then compare captured stage
products and final raw floats.

## Test Plan

### Unit and runtime tests

- Morph resolution matches ordinary Trimesh processing for constant, voice
  time, key, velocity, mod wheel, pressure, and MIDI CC sources.
- Frame refresh count and frontier positions match a hand-authored deterministic
  schedule.
- Current/previous frame transitions are correct at note start and after each
  refresh.
- A static graph produces exactly one unique frame even if the runtime elects
  to rerender; an evolving graph produces the expected ordered frame hashes.
- Splitting one render request at every possible position around a frontier
  yields identical concatenated samples.
- Scratch primary-axis/domain combinations use existing policy results.
- Graph publication and note stealing reset or transfer only the state defined
  by the existing voice lifecycle contract.
- Prepared render calls perform zero allocations after preparation.

### Integration tests

- Filter Saw's early, middle, and late windows correspond strongly to Cycle 1
  and no longer alternate between accidental matches and large mismatches.
- PWM has an observable, directionally correct duty-cycle evolution in its
  cyclogram.
- Dunk 2's short time-domain cubes affect only the expected initial cycles.
- Japan Drum shows evolving frame hashes in both engines and repeatable output
  across fresh processes.
- Repeating a fully released note produces the same frame-hash sequence and raw
  output as the first note.
- Changing host block size does not change raw output or frame hashes.

### Differential stage capture

For the first remaining failing preset, compare these mature boundaries without
normalization or time warping:

1. rasterized time frame;
2. forward-FFT magnitude and phase;
3. post-layer magnitude and phase;
4. reconstructed fixed IFFT frame;
5. pitch-clocked cyclic output;
6. final pre-effect voice output.

Each record includes sample/cycle frontier, MIDI note domain, frame size,
channel/lane identity, morph, scratch value, payload format, and SHA-256. The
first unequal boundary determines the next fix.

## Completion Criteria

- Prepared spectral frames respond to every supported live modulation source at
  the characterized Cycle 1 frontier cadence.
- Filter Saw, PWM, Dunk 2, and Japan Drum audibly and measurably evolve.
- Output and frame hashes are independent of host block partitioning.
- Two consecutive fully released notes and two fresh processes are byte
  repeatable with fixed seeds.
- Static spectral reference and pure FFT round-trip regressions remain exact.
- Single- and multi-lane execution share frames with no duplicate
  rasterization.
- No audio-thread allocation, lock, graph traversal, or UI/model access is
  introduced.
- The first remaining V1/V2 difference is reported at a captured mature DSP
  boundary; no missing live-modulation behavior remains hidden behind a final
  waveform metric.
- Relevant entries in `docs/TDD/audio-bugs.md` are resolved or narrowed to a
  different documented subsystem.

## Completion Evidence

- `PreparedOscillatorProcessContext` carries the voice, segment, control, and
  attachment timing needed by prepared renderers without graph/model access.
- `TrimeshMorphResolver` is shared by ordinary and prepared Trimesh execution.
  Unsupported external topology prevents a consumer from being absorbed into
  the prepared oscillator region rather than freezing its input.
- `SpectralOscillatorRegionRuntime` refreshes frames on its shared center-lane
  cycle clock. Frame generation is independent of lane count; pitch, phase,
  pan, carry, and cyclic resampling remain lane-local.
- The deterministic preset runtime matrix covers Saw, Filter Saw, PWM, Dunk 2,
  and Japan Drum at MIDI 36, 48, 60, and 72, note lengths of 1024 and 4096
  samples, and host blocks of 64, 127, 256, and 512 samples. Output channels,
  scratch signals, and frame-render counts are byte-identical to the 512-sample
  reference partition (4720 assertions).
- Focused control tests cover static fallback, voice time, key, velocity,
  pressure, modulation wheel, MIDI CC, timed events, scratch enable/disable,
  repeated-note reset, frame sharing, and allocation-free processing. The full
  oscillator-region suite passes (5761 assertions).
- `scripts/test_cycle_v2_time_evolution.py` validates visible Dunk 2 spectral
  evolution, PWM duty-cycle motion, scratch influence, two fully released
  notes in one process, and fresh-process WAV identity. Its final run reported
  a 5.614 dB Dunk 2 evolution, 0.263 PWM duty excursion, 0.316 scratch/no-
  scratch duty difference, 0.018 maximum repeat-note duty difference, and
  byte-identical fresh-process WAV files.
- Existing static spectral reference, fixed-frame FFT, and realtime allocation
  regressions remain passing. The complete test suite passes all 879 cases.
  Modified DSP hot paths contain no new scalar `std::<math>` operations in
  their inner loops.
- Fresh Filter Saw and Japan Drum differential runs are deterministic within
  each engine, but their effect-free final voice outputs remain unequal. This
  is boundary 6 of the differential capture plan. Filter Saw correlations are
  `0.94219`, `0.91977`, `0.88743`, and `0.84886` at MIDI 36, 48, 60, and 72;
  Japan Drum correlations are `0.22121`, `0.34862`, `0.24436`, and `0.23510`.
  The shared fixed-frame rasterization/FFT reference tests remain exact, so the
  next parity investigation must add cross-engine captures for boundaries 1–5
  rather than attributing the residual to missing live frame refresh.

## Deletion Targets

- Remove the one-shot `frameReady` assumption as the lifetime policy for an
  active spectral note; retain only state that describes whether the initial
  frame pair exists.
- Remove static `TrimeshConfiguration::morph` as the sole runtime morph source
  for prepared mesh operations. It remains only the unconnected-input fallback.
- Remove any temporary prepared-only morph evaluator after shared morph
  resolution is extracted.
- Remove temporary frame diagnostics from production builds once stable test
  hooks or automation-only capture owns the observation boundary.
- Remove any fallback that silently accepts unsupported live control topology
  by freezing it.

## Resolved Design Questions

- Does Cycle 1 sample smoothed mesh properties at the current or future frame
  frontier in every compositing mode?
- When a controller event and cycle frontier share a sample offset, which is
  applied first?
- How does Cycle 1 advance spectral scratch when a rendered cycle extends past
  the current host block?
- Which frame transitions are shared by detuned unison lanes, and which state is
  lane-local?
- Can existing graph control buffers represent every supported explicit control
  topology without lookahead, or must some regions deliberately remain on the
  ordinary exact path?

The authoritative Cycle 1 code and the characterization tests resolve these as
follows: smoothing and envelopes advance to the future frame frontier before
rasterization; events dispatched at a segment boundary are visible at that
frontier; scratch advances by the same elapsed frontier distance; the rendered
frame transition is shared while lane clocks and output state remain local;
and unsupported external control topology ends prepared-region admission at
the oscillator boundary.

## Cycle 1 Timing Characterization

`CycleBasedVoice::renderInterpolatedCycles()` establishes the authoritative
ordering used by spectral voices:

1. The initial future frame is sampled at voice sample zero. It receives a
   one-sample smoothing/envelope advance before `calcCycle()`.
2. A later shared frontier advances by the center oscillator period multiplied
   by the configured control stride. The floating cumulative position is kept
   for interpolation and is truncated to form `futureFrame.frontier`.
3. `absVoiceTime`, parameter smoothing, scratch/pitch envelopes, and lane pitch
   deltas advance by the elapsed frontier distance before `calcCycle()` samples
   time, magnitude, and phase meshes.
4. `calcCycle()` runs once for `groups.front()`. Every unison lane then consumes
   the resulting current/previous frame pair while advancing its own detuned
   cumulative position, phase, carry, resampler, and output ring buffer.
5. A local scratch envelope therefore exposes the value reached by the same
   frontier advance. A global scratch buffer is sampled at the floating
   frontier translated into the current host block.
6. MIDI is dispatched in sample-offset segments before voice rendering. A
   controller event at a segment boundary is visible to the frame whose
   frontier begins that segment; an event after the frontier is not.

Cycle V2 uses a per-cycle shared cadence because its compiled voice context has
no separate Cycle 1 control-stride setting. This is the exact cadence for the
ported deterministic presets under test and avoids inventing a hidden blockwise
control rate. Pitch-envelope processing remains lane-local.
