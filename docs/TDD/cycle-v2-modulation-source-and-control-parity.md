# Cycle V2 Modulation Source And Control Parity

## Status

In progress: the source/control runtime is implemented; compact single-source
and bundled triple-source authoring UX is being added.

This design extends the implemented contracts in:

- `cycle-v2-dynamic-envelope-modulation.md`;
- `cycle-v2-smoothed-morph-control.md`;
- `cycle-v2-node-definition-and-graph-model.md`;
- `cycle-v2-node-module-runtime.md`; and
- `cycle-v2-causal-update-graph.md`.

## Problem

Cycle v2 already models modulation destinations as graph inputs. Envelope nodes
have red and blue `ControlSignal` inputs, and Trilinear Mesh nodes have yellow,
red, and blue `ControlSignal` inputs. Their audio processors already distinguish
persistent base values from connected effective values. Realtime Trilinear Mesh
and Envelope morph positions use per-voice `SmoothedMorphPosition` state, and
dynamic Envelope preparation is bounded, coalesced, immutable, and adopted
without resetting playback.

The graph has no nodes that publish the performance controls which drove the
Cycle v1 modulation matrix. A user therefore cannot connect velocity, key
position, modulation wheel, aftertouch, another MIDI controller, or voice
progress to those destination inputs. The dormant Cycle v1 `ModMatrixPanel`
cannot fill that role: it is a UI-owned routing table with integer source and
destination ranges, a lock taken while routing, direct singleton calls into
voices and panels, and layer-index maintenance. Installing it beside the Cycle
v2 graph would create a second, hidden topology and two serialization
authorities.

Control signals also have two consumers with intentionally different
lifecycles:

- realtime audio evaluates MIDI and note state for each retained voice,
  preserves controller state between events, and smooths effective destination
  parameters over audio time;
- graphic preview and traversal need deterministic values from an explicit
  audition context and must not inherit a live voice's smoothing history or
  whichever MIDI event happened most recently.

The new capability must preserve Cycle v1's useful source and destination
semantics while making graph edges, node definitions, and the existing Cycle
v2 processor boundaries authoritative.

## Authoritative Implementations And Reuse Decision

### Cycle v1 and legacy behavioral reference

The most authoritative legacy sources are under
`/Users/daven/repos/amaranth-legacy`:

- `UI/Panels/ModMatrixPanel.{h,cpp}` defines the available sources, destination
  families, default assignments, one-source-per-destination-dimension rule,
  names, and preset persistence;
- `Audio/Synthesizer.cpp` normalizes MIDI CC values and routes CC 0 through 127;
- `Audio/Voices/SynthesizerVoice.cpp` publishes normalized key position,
  `1 - velocity`, and normalized channel aftertouch per voice;
- `Audio/SynthAudioSource.cpp`, `Audio/Voices/CycleBasedVoice.cpp`, and
  `Audio/Voices/SynthFilterVoice.cpp` deliver global or per-voice values to
  waveshape, spectral-magnitude, spectral-phase, volume-envelope,
  pitch-envelope, and scratch-envelope morph dimensions; and
- `Interaction/E2Interactor.cpp` plus the Envelope rasterizer path define the
  `Dynamic while live` behavior.

The corresponding carried Cycle v1 sources in this repository, principally
`cycle/src/UI/Panels/ModMatrixPanel.{h,cpp}` and `cycle/src/Audio`, are a useful
portability reference where they contain later validation and JSON work. They
do not supersede the legacy repository's product semantics.

The legacy behavior reused unchanged is:

- key position is normalized across Cycle's playable MIDI-note range;
- the velocity source named `1-Velocity` emits `1 - noteOnVelocity`;
- MIDI CC and channel aftertouch are normalized to `[0, 1]`;
- modulation wheel is the named form of MIDI CC 1;
- the same source may fan out to multiple destinations;
- a destination morph axis has at most one incoming source;
- waveshape and spectral layers accept yellow, red, and blue positions;
- volume, pitch, and scratch envelopes accept red and blue positions; and
- unconnected axes use their persistent authored value.

The legacy routing implementation is not reused. In Cycle v2, graph edges are
the only modulation routes, graph input multiplicity replaces matrix conflict
removal, and node identity replaces destination integer ranges and mutable
layer indices.

### Cycle v2 processing authority

The existing Cycle v2 implementations remain authoritative:

- `cycle-v2/src/Graph/NodeDefinition.cpp` owns port and parameter definitions;
- `cycle-v2/src/Runtime/GraphAudioExecutor.*` owns compiled per-voice graph
  execution and prepared processor lifetime;
- `cycle-v2/src/Runtime/SmoothedMorphPosition.h` owns realtime destination
  smoothing;
- `cycle-v2/src/Runtime/TrimeshNodeAudioProcessor.cpp` owns live Trilinear Mesh
  morph consumption;
- `cycle-v2/src/Nodes/Envelope/EnvelopeSignalProcessor.*` owns Envelope morph
  smoothing, request coalescing, non-realtime preparation, and adoption;
- `cycle-v2/src/Runtime/GraphPreviewExecutor.*` owns graphic graph evaluation;
  and
- `cycle-v2/src/UI/NodePalette.*` owns sidebar grouping and node creation
  presentation.

No modulation source, MIDI adapter, preview processor, or UI editor may copy
mesh interpolation, envelope rasterization, smoothing, or playback behavior.

## Goals

- Add a Modulation node under `Control` in the sidebar.
- Cover Cycle v1's performance-control sources: voice time, inverse velocity,
  key scale, mod wheel, channel aftertouch, and arbitrary MIDI CC.
- Also expose direct velocity because graph composition makes both polarities
  useful and Cycle v1 carried a distinct inverse-velocity identity.
- Replace Cycle v1 utility rows with an authored constant source mode rather
  than a hidden global utility bank.
- Make one source node reusable through graph fan-out.
- Deliver sample-offset-aware controller changes and note-on control state to
  each relevant voice without allocation or graph mutation on the audio thread.
- Keep destination scaling and smoothing in the destination processor.
- Give graphic preview an explicit, deterministic audition-control context.
- Serialize all authoring choices as ordinary typed node parameters and edges.
- Preserve existing Cycle v2 graph, processor, invalidation, and publication
  boundaries.

## Non-Goals

- Reintroduce a modulation-matrix dialog, routing table, or integer destination
  IDs.
- Import the Cycle v1 default matrix routes into newly authored Cycle v2
  graphs. Connections remain explicit.
- Apply modulation as offsets or depths inside destination nodes. Connected
  morph inputs remain absolute effective positions in `[0, 1]`.
- Add hidden multiple-source summation at an input. Users compose signals with
  graph math nodes before connecting them to a destination.
- Add LFOs, random generators, MPE dimensions, polyphonic expression, pitch
  bend, host automation sources, or tempo-synced modulators in the parity
  slice. These can become later source modes without changing destination
  contracts.
- Make preview reproduce a full MIDI performance or audio voice lifecycle.
- Smooth a source independently for every destination. Source output expresses
  control intent; each realtime destination owns its parameter response.
- Preserve Cycle v1 bugs such as UI-advertised but inactive voice-time routing,
  synchronous Envelope rerasterization on the audio thread, or the volume
  Envelope's inconsistent dynamic omission.

## Product Semantics

### Compact and triple-source authoring extension

The single Modulation node is a one-row canvas node. Its selected source is the
dominant label; `Modulation` is a compact type badge rather than the main
content. Source-specific details such as `CC 74` or a constant value remain
visible without opening the editor.

Add a distinct `modulationTriple` authoring node with three vertically stacked
source rows and three ordinary mono `ControlSignal` outputs:

```text
yellow -> yellow
red    -> red
blue   -> blue
```

The legacy defaults are retained: yellow is `voiceTime`, red is `keyScale`, and
blue is `modWheel`. Each row otherwise has the same source, controller, and
constant choices as a single Modulation node. The implementation reuses the
single-source evaluator and event-span renderer; it must not add a triplet
signal domain, three-channel control payload, destination-side unpacker, or
second routing topology.

On the canvas, the three outputs share a yellow/red/blue pie socket. Trilinear
Mesh presents a corresponding composite input target. One drag between these
composite targets performs a compound authoring gesture which creates or
replaces the three normal graph edges. The graph compiler, serializer, audio
executor, and destination processors continue to see independent edges.

When the matching yellow/red/blue edges connect the same triple source and
destination, the scene coalesces them into one visible cable with pie endpoints.
Selection and deletion apply to the visual bundle as a unit. Raw graph
inspection and persistence continue to expose all three constituent edges.
Individual single-source routing remains available through the existing
yellow/red/blue destination ports; the composite target is authoring and
presentation metadata only.

Envelope bundling is not part of this extension. Its red and blue inputs remain
individually routable because a three-axis gesture would conceal the unused
yellow source and provide less visual benefit than the Trilinear Mesh case.

### Modulation node

Add one node type with stable type ID `modulationSource` and display name
`Modulation`. It appears in the `Control` palette section and has one output:

```text
value: ControlSignal, mono, [0, 1]
```

Its typed parameters are:

| Parameter | Type | Meaning |
| --- | --- | --- |
| `source` | choice | `voiceTime`, `velocity`, `inverseVelocity`, `keyScale`, `modWheel`, `channelPressure`, `midiCC`, or `constant` |
| `controller` | integer 0...127 | Used only by `midiCC`; defaults to 1 |
| `constant` | number 0...1 | Used only by `constant`; defaults to 0.5 |

The compact editor shows the source choice and only the parameter relevant to
that source. Its title/subtitle identifies the selection, for example
`Modulation / CC 74` or `Modulation / 1-Velocity`. Source-specific decoration
is presentation derived from typed parameters, not independently serialized
state.

`modWheel` and `midiCC(controller = 1)` read the same underlying controller
state. They are two authoring descriptions, not two MIDI states. Loading
normalizes controller numbers before processors are constructed.

`constant` is the Cycle v2 replacement for a Cycle v1 utility control. Each
instance is local, visible, automatable through normal graph editing, and
serialized with the graph. There is no numbered singleton utility bank.

### Source values and scope

For note number `n`, playable range `[nMin, nMax]`, note-on velocity `v`, and
normalized controller value `c`:

```text
voiceTime        = constrain(elapsedVoiceTime / authoredVoiceDuration, 0, 1)
velocity         = constrain(v, 0, 1)
inverseVelocity  = 1 - velocity
keyScale         = constrain((n - nMin) / (nMax - nMin), 0, 1)
modWheel         = latest CC 1 value
channelPressure  = latest channel-pressure value
midiCC(k)        = latest CC k value
constant         = authored constant
```

The implementation must call the existing Cycle note-range normalization
helper or extract that normalization into a shared small control-domain helper;
it must not introduce a third formula. The denominator-zero case is explicitly
defined as zero.

Velocity, inverse velocity, key scale, and voice time are per voice. MIDI CC
and channel pressure are channel-scoped controller states copied into the
control view for every affected voice. They still produce a per-voice signal
block because the graph executor processes retained voice instances. A future
MPE source may refine that scope; this slice must not encode channel-wide state
as process-global mutable singleton access.

Voice time is specified as a working Cycle v2 source even though the Cycle v1
matrix exposed it while its explicit route call was commented out. It follows
the same normalized note-progress clock used by the authoritative voice
playback path. It does not derive time independently from block count in the
source processor. Reset, note-on, voice stealing, and retrigger therefore
produce the same progress transitions as the owning voice.

Before the first observed controller event, MIDI CC and channel pressure use
zero. Controller state persists across note-on and note-off in the same way as
MIDI channel state; Reset returns it to zero. A note beginning partway through
a controller gesture observes the current channel value at its note-on sample.

### Destination parity

Cycle v2 expresses former layer destinations as node instances:

| Cycle v1 destination | Cycle v2 destination |
| --- | --- |
| Waveshape layer yellow/red/blue | Trilinear Mesh `yellow`/`red`/`blue` inputs in a time-domain route |
| Spectral magnitude layer yellow/red/blue | Trilinear Mesh `yellow`/`red`/`blue` inputs in a magnitude-domain route |
| Spectral phase layer yellow/red/blue | Trilinear Mesh `yellow`/`red`/`blue` inputs in a phase-domain route |
| Volume Envelope red/blue | The Envelope node feeding the amplitude path |
| Pitch Envelope red/blue | The Envelope node feeding the pitch path |
| Scratch Envelope red/blue | The Envelope node attached to a Trilinear Mesh scratch input |

No destination registry enumerates these roles. Compatibility follows from
typed ports and graph placement. Any present or future node with a compatible
single `ControlSignal` input can consume a modulation source without editing a
modulation switch or source node.

A destination input accepts one graph edge. Connecting another edge uses the
normal graph replacement gesture or is rejected consistently with other
single-input ports. Multiple sources require explicit Add/Multiply or later
purpose-built control transform nodes. Values reaching morph inputs are
constrained by the existing destination contract.

## Control-State Boundary

### MIDI ingestion and retained state

Introduce a focused control-state owner at the MIDI-to-voice boundary. Names
are illustrative:

```cpp
struct VoiceControlState {
    int noteNumber {};
    float velocity {};
    float normalizedVoiceTime {};
};

struct ChannelControlState {
    std::array<float, 128> controllers {};
    float channelPressure {};
};

struct TimedControlEvent {
    enum class Kind { Controller, ChannelPressure };
    Kind kind {};
    size_t sampleOffset {};
    int channel {};
    int controller {};
    float value {};
};

struct AudioControlView {
    VoiceControlState voice;
    const ChannelControlState* channel {};
    Span<const TimedControlEvent> events;
};
```

The actual representation may combine lifecycle and control events, but it
must provide these semantics:

- MIDI bytes are normalized once at ingestion;
- events retain their sample offset within the host block;
- channel state is updated in event order;
- each voice receives its note state and the controller events applicable to
  its MIDI channel;
- the view is immutable for the duration of graph processing;
- storage is prepared outside realtime and reused; and
- source processors perform no MIDI parsing, map lookup, allocation, locking,
  or singleton access.

Extend `AudioVoiceContext` or give `AudioProcessContext` a strongly typed
control view. Do not encode these values as node parameters, graph edits, or a
generic string property bag. The graph execution plan remains topology; live
control is invocation state.

### Block production

A Modulation processor writes one mono `ControlSignal` block. Note-latched
sources are constant across a block unless a note lifecycle transition occurs
within it. Controller sources hold the value from the block boundary and apply
each applicable event at its exact sample offset, producing a piecewise
constant block. Voice time follows the authoritative normalized voice clock
sample by sample or in equivalent vector ramps supplied by that clock.

Source production may use `Buffer`/`VecOps` fill and ramp operations for spans
between events. Inner event loops only locate boundaries and issue blockwise
fills; they do not perform scalar per-sample math. Output storage comes from
the prepared audio work arena.

The source publishes raw normalized control. It does not create a
`SmoothedParameter`. Trilinear Mesh and Envelope retain the established
per-voice `SmoothedMorphPosition` behavior, including block-partition-
equivalent smoothing. Other destinations must state their smoothing policy as
part of their own parameter contract rather than silently inheriting a source
policy.

### Note and controller ordering

Events at the same sample use original MIDI-buffer order. A controller event
before note-on at offset `s` is visible to the new note at `s`; a controller
event after note-on begins affecting it after that event. Note-off does not
erase channel controller state. Voice stealing replaces note-scoped state but
does not reset channel state.

The execution contract must not process an entire block with its final MIDI
state when an event occurs in the middle. A semantic test with two controller
events and a note-on in one block fixes the ordering behavior.

## Realtime Destination Semantics

The existing absolute-position rule remains:

```text
target(axis) = connected ? source(axis) : persistedBase(axis)
effective(axis, t) = destinationSmoother(target(axis), t)
```

Each retained destination processor owns independent state per graph voice.
Fan-out from one source does not share a smoother. Disconnecting an input sets
the destination's smoothing target to its persisted base value; it does not
jump directly unless the destination's established reset/initialization
contract requires direct assignment.

Envelope behavior remains governed by the dynamic-envelope TDD:

- dynamic disabled: effective red/blue are latched for a note;
- dynamic enabled: smoothed current positions produce thresholded,
  cadence-bounded, newest-only preparation requests;
- preparation and allocation remain off realtime;
- adoption preserves playback state and uses the established continuity ramp;
  and
- volume, pitch, and scratch roles use the same Envelope node behavior.

No special source-to-Envelope path may bypass `EnvelopeSignalProcessor`.

## Graphic And Preview Semantics

### Explicit audition context

Add a stateless `PreviewControlContext` supplied by the preview request, not
read from audio state:

```cpp
struct PreviewControlContext {
    float voiceTime { 0.f };
    float velocity { 1.f };
    int noteNumber { defaultAuditionNote };
    float channelPressure { 0.f };
    std::array<float, 128> controllers {};
};
```

Document/UI defaults may populate this structure, but every preview execution
receives a complete immutable value. Changing it is a preview/audition edit,
not a persistent node-parameter edit and not an audio-configuration
publication. A preview may later expose selected live-voice inspection only
as an explicitly identified mode with a copied snapshot and voice/generation
identity.

The Modulation preview processor evaluates source choice through one shared
normalization/evaluation core also used by audio source processors. It does
not query MIDI devices, the plugin processor, the most recently active voice,
or destination smoothing state.

### Scalar preview versus traversal grid

For an ordinary compact-node preview, every source is shown at the scalar
value in `PreviewControlContext`.

For a graph traversal over normalized voice time:

- `voiceTime` emits the traversal's explicit time coordinate;
- note-latched and controller sources emit their audition-context scalar for
  every column; and
- downstream Envelope and Trilinear Mesh previews consume those explicit
  values through their existing stateless graphic paths.

Graphic traversal must not instantiate an audio voice, advance a destination
`SmoothedParameter`, run note-on/off playback, or simulate MIDI event history.
This intentional divergence means a graphic curve represents the requested
control field, while audio represents its smoothed temporal response. A
separate diagnostic trace may visualize smoothing only if it explicitly runs
a bounded DSP simulation and labels that product accordingly.

The preview of a destination with no connected source uses its persisted base
value. Preview results are deterministic for identical graph revision,
`PreviewControlContext`, and traversal request regardless of prior audio
activity.

## Graph, Serialization, And Invalidation

Register the node through `NodeDefinitionRegistry`, its audio and preview
factories through the module registry, and its palette entry through definition
metadata when that migration is available. Do not add parallel metadata to
more generic `NodeKind` switches than the current compatibility architecture
strictly requires. Any temporary `NodeKind::ModulationSource` branches are
enumerated during implementation review and deleted when registry-driven
palette/editor dispatch becomes authoritative.

Graph serialization stores the stable type ID, typed parameters, position, and
edges through the existing graph format. It stores no current MIDI value,
voice time, smoother value, event queue, or preview audition state in the node.
Existing Cycle v2 graphs require only definition normalization; there is no
modulation node to migrate into them.

Importing a Cycle v1 preset is a separate compatibility adapter. If undertaken,
it may translate each distinct matrix input into one source node and each
mapping into an edge, resolving legacy layer IDs to imported graph node IDs.
It must not survive as a runtime routing table. Unsupported or ambiguous
legacy destinations must be reported rather than silently dropped. This
adapter is not required for the initial parity slice.

Persistent source parameter changes use ordinary conflict-checked graph edits
and `GraphChangeSet` invalidation. Live MIDI/control changes:

- do not increment graph or node-model revisions;
- do not serialize, create undo history, compile topology, or publish new DSP
  configurations;
- dirty only the current realtime invocation; and
- do not trigger graphic preview unless an explicit audition context changes.

Changing a source choice or CC number does not reset downstream processor
state unnecessarily. The next prepared invocation supplies the new source
target, and destinations follow their established smoothing policy.

## Ownership And Threading

```text
MIDI buffer + voice lifecycle
        -> MIDI/control-state owner
        -> immutable per-block AudioControlView
        -> ModulationSourceAudioProcessor
        -> ControlSignal block
        -> existing destination processor
             -> per-voice smoothing
             -> Trilinear Mesh realtime sampling
             -> or bounded Envelope preparation/adoption

PreviewControlContext + traversal coordinate
        -> shared control-value evaluator
        -> ModulationSourcePreviewProcessor
        -> stateless ControlSignal/grid
        -> existing graphic destination processor
```

The MIDI/control-state owner owns channel state and prepared event storage.
The voice owner owns note number, velocity, and normalized progress. A source
processor owns only its prepared source selection and output workspace. A
destination processor owns smoothing and any domain-specific dynamic state.
The graph document owns source parameters and edges.

The audio thread may update preallocated channel state, partition spans, fill
prepared output buffers, and set existing destination targets. It may not
allocate, lock, serialize, mutate the graph, publish UI state, build a mesh or
Envelope, or wait for non-realtime work.

## Complexity Contracts

Let `B` be block samples, `E` applicable control events, `S` modulation source
nodes executed for a voice, `D` connected destinations, and `C` preview-grid
columns.

- MIDI normalization and retained-state update are `O(E)` per block.
- A constant/note-latched source is `O(B)` to materialize its output, or `O(1)`
  when the signal payload gains a constant-span representation.
- A controller source is `O(B + E)` with block storage, using span fills rather
  than scalar math per sample.
- Voice-time production is `O(B)` using the authoritative voice clock and
  vector/ramp operations.
- Routing cost is ordinary compiled graph execution, `O(S + D)` aside from
  payload production/consumption; there is no scan across all mappings.
- Preview scalar evaluation is `O(1)` per source and traversal is `O(C)` per
  source.
- No cost is proportional to the total number of MIDI controllers unless a
  complete fixed-size channel snapshot is intentionally copied outside the
  realtime hot path; ordinary event processing reads only the selected source.
- Live control causes no graph compile, serialization, snapshot publication,
  mesh preparation, or unbounded Envelope request queue.

## Semantic Tests

### Source parity

- Key scale matches the existing playable-range normalization at the lowest,
  middle, and highest supported notes.
- Velocity emits the note-on value and inverse velocity emits exactly one
  minus that normalized value.
- Mod wheel and CC 1 emit identical blocks from the same event stream.
- CC 0 and CC 127 are both addressable and values 0 and 127 map to 0 and 1.
- Channel pressure maps 0 and 127 to 0 and 1.
- Constant emits its authored value and is independent per node instance.
- Voice time begins at zero, follows the owning voice's normalized progress,
  reaches one according to its duration contract, and resets on the defined
  note/steal lifecycle.
- Controller and pressure state persist across note boundaries and Reset
  restores their defaults.

### Event timing and voice isolation

- A controller change at sample offset `s` leaves samples before `s` at the
  old value and samples from `s` onward at the new value.
- Two controller events in one block create the expected three constant spans.
- Controller/note-on events at the same offset obey original MIDI-buffer order.
- Two voices with different notes and velocities receive isolated key and
  velocity signals.
- Channel-wide CC changes reach all voices on that channel without changing
  note-scoped values.
- Voices on another MIDI channel retain their prior controller state.
- Voice stealing changes note-scoped source values while preserving applicable
  channel control state.

### Graph routing and destinations

- One source fans out to multiple compatible destinations through ordinary
  graph edges.
- A destination morph input cannot retain two incoming edges.
- Disconnecting one axis makes it smooth back toward its persisted base while
  other connected axes remain live.
- Explicit Add/Multiply composition produces the same value at every consumer;
  there is no hidden source combination.
- Time-, magnitude-, and phase-domain Trilinear Mesh instances consume yellow,
  red, and blue modulation with the existing per-voice smoothing contract.
- Volume-, pitch-, and scratch-role Envelope nodes consume red and blue through
  the same generic Envelope implementation.
- Dynamic-disabled and dynamic-enabled Envelope tests from the existing TDD
  pass with a real Modulation source node upstream.

### Graphic/audio separation

- An explicit preview context yields the same normalized scalar as audio for
  the equivalent note/controller state before destination smoothing.
- Changing live MIDI without changing preview context does not change graphic
  output.
- Changing preview context does not mutate graph parameters or audio state.
- Voice-time traversal emits the requested traversal coordinate; other source
  modes remain constant over columns.
- Stateless graphic destination output is independent of prior audio smoothing
  history.
- A realtime step approaches its destination target according to the existing
  smoothing tests while the graphic request immediately represents its
  explicit value.

### Persistence and invalidation

- Round-trip serialization preserves source mode, CC number, constant value,
  and fan-out edges, but contains no live control state.
- Loading rejects or normalizes out-of-range controller and constant values
  through the shared parameter schema.
- A MIDI event changes no graph revision, creates no undo record, and performs
  no compile or DSP-configuration publication.
- A source parameter edit produces one semantic graph edit and the expected
  preview/audio invalidation without resetting unrelated nodes.

### Realtime and architecture

- Instrumented realtime processing reports zero allocation, lock acquisition,
  serialization, graph mutation, UI publication, and Envelope preparation.
- Increasing the number of unrelated graph nodes or legacy destinations does
  not increase one source's routing scan cost.
- Source and preview processors contain no mesh, Envelope, interpolation,
  smoothing, or playback algorithm.
- Destination processors do not parse MIDI or query singleton controller
  state.
- Generic graph and palette infrastructure contains no MIDI source-specific
  routing table or destination-kind switch.

## UI Automation Acceptance

Add a focused Cycle v2 fixture that:

1. opens the `Control` sidebar group and verifies a `Modulation` entry;
2. creates a Modulation node and changes it to CC 1;
3. connects it to an Envelope red input and fans it out to a Trilinear Mesh
   blue input;
4. verifies cable attachment, stable selection, and the compact source label;
5. injects a MIDI CC event through the agent control boundary;
6. asserts the selected audition/live diagnostic value and downstream state
   using stable automation output rather than pixel colour alone; and
7. saves, reloads, and verifies the node parameters and both edges.

The fixture is product evidence for creation and routing. Focused C++ semantic
tests remain authoritative for sample timing, voice isolation, smoothing,
allocation, and graphic/audio separation.

## Expected Implementation Surface

The initial slice should remain narrow:

- one source-domain evaluator and typed source configuration;
- one audio processor and one preview processor;
- one prepared MIDI/control-view extension at the graph invocation boundary;
- one compact editor or generic typed editor support;
- one node definition, module registration, palette entry, and icon;
- focused graph/audio/preview tests and one automation fixture.

Expected new production code is approximately 500 to 900 lines excluding
tests and the SVG asset. No single new production implementation file should
normally exceed 300 lines. More than a few small compatibility branches in
generic graph, canvas, executor, or editor code is evidence that registry
metadata or a focused interface is missing and requires design review.

The implementation must report:

- production lines added and removed and the largest changed files;
- every new `NodeKind`, audio-role, preview-role, or source-mode switch branch;
- the shared normalization/evaluation code used by audio and preview;
- whether control events are copied, referenced, or partitioned per voice and
  the measured realtime capacity bound;
- proof that destination smoothing and Envelope preparation were reused
  unchanged; and
- any temporary compatibility code plus its deletion target.

## Delivery Slices

1. **Shared control semantics**: define typed source modes, normalization,
   preview context, realtime control view, and pure parity tests.
2. **Realtime source path**: publish prepared MIDI/note state, add the source
   audio processor, preserve sample offsets, and prove zero allocation.
3. **Graphic path**: add the stateless preview processor and explicit audition
   context, including voice-time traversal behavior.
4. **Graph and UI**: register the node, add it under `Control`, provide compact
   editing and iconography, and route through ordinary cables.
5. **End-to-end parity**: exercise all destination families, dynamic Envelope
   modes, persistence, and focused agent automation.
6. **Refactor and deletion audit**: inspect diff size and switches, remove
   scaffolding, verify no hidden routing topology exists, run style/clang-tidy
   checks, and record any blocked Cycle v1 import adapter separately.

Each slice follows the repository engineering loop and is committed only after
its refactor, style, semantic-test, and realtime-boundary evidence is complete.

## Completion Criteria

- A user can create a Modulation node from `Control` and select every Cycle v1
  parity source described above.
- Source nodes connect and fan out through ordinary `ControlSignal` graph edges
  to every applicable Trilinear Mesh and Envelope morph input.
- MIDI/note values are normalized once, preserve within-block event timing,
  and remain correctly scoped across voices and channels.
- Existing per-voice destination smoothing and dynamic Envelope preparation
  are reused without source-specific bypasses.
- Graphic output is deterministic from an explicit audition context and is
  independent of realtime smoothing and prior audio activity.
- Live control causes no graph mutation, compile, serialization, UI
  publication, allocation, lock, or preparation on the realtime thread.
- Graph persistence contains authoring state and routes but no live controller
  or voice state.
- Focused semantic and automation tests cover creation, routing, source parity,
  event timing, voice isolation, destination behavior, persistence, and the
  graphic/realtime divergence.
- No modulation matrix, integer destination registry, MIDI-aware destination,
  or duplicated mesh/Envelope/smoothing implementation remains in Cycle v2.
- Implementation review confirms the size/branch envelope or records and
  resolves the architectural reason for exceeding it.

## Implementation Review

The implementation follows the proposed ownership boundary. `ModulationSource`
contains the shared normalization/evaluation core used by both audio and
preview processors. `MidiControlState` normalizes JUCE MIDI messages once,
retains fixed 128-controller snapshots for 16 channels, and copies only the
applicable channel's ordered events into a voice-owned vector whose capacity is
reserved by `prepareVoice`. The configured bound is
`maximumEventsPerChannel`; overflow is dropped deterministically, counted, and
covered by a semantic test. Source processing then performs only vector fills,
ramps, and an event-boundary scan.

Voice time is supplied by the voice owner as a normalized start and per-sample
increment, then materialized with a clipped `Buffer` ramp. Preview time is an
explicit scalar or traversal coordinate in `PreviewControlContext`; it never
reads realtime state. Controller sources hold the block-start snapshot and
apply ordered events at their exact offsets. Live values are invocation state
and are absent from graph serialization, graph revisions, undo, and DSP
configuration publication.

Production and resource changes total 718 added lines and one removed line,
excluding tests, fixtures, and this TDD. The largest additions are:

- `ModulationSource.cpp`: 228 lines;
- the hosted editor addition in `ConcreteNodeEditors.cpp`: 163 lines;
- `MidiControlState.cpp`: 84 lines;
- `ModulationSource.h`: 55 lines; and
- `MidiControlState.h`: 39 lines.

The explicit compatibility branches introduced are:

- `NodeKind::ModulationSource` in graph identity, palette automation aliases,
  palette presentation, view capabilities, compact presentation, and hosted
  editor registration;
- `AudioModuleRole::ModulationSource` in configuration, factory, and role-label
  registries;
- `PreviewModuleRole::ModulationSource` in factory and automation role-label
  registries; and
- the eight `ModulationSourceMode` cases in ID conversion and shared value
  evaluation.

These branches select metadata or a domain processor; none route by
destination kind. The palette branch is a deletion target when palette entries
are fully definition-driven. The canvas automation aliases remain a stable
external command boundary. The hosted-editor and role registries are the
current authoritative extension points and have no transitional deletion
target.

`TrimeshNodeAudioProcessor`, `EnvelopeSignalProcessor`, and
`SmoothedMorphPosition` have no production diff in this work. Their established
absolute-input smoothing, dynamic preparation, adoption, and playback paths
are reused unchanged. The allocation-instrumented dynamic Envelope test now
uses a real Modulation node upstream, and ordinary graph-edit tests connect the
source to Envelope red/blue plus Trilinear Mesh yellow/red/blue while verifying
single-input replacement and fan-out.

The focused standalone fixture creates and edits the source, connects both
destination families, inspects the hosted editor and preview role, and verifies
parameters and edges after save/reload. Cycle V2's standalone shell has no live
MIDI device/voice callback and therefore does not synthesize a MIDI gesture in
UI automation. The authoritative MIDI-to-voice boundary is instead exercised
directly by semantic tests for normalization, channel isolation, retained
state, ordered sample offsets, capacity overflow, and realtime allocation.
This is a test-surface divergence, not a second runtime control path.

Verification evidence:

- focused `[modulation]`: 155 assertions in 23 test cases;
- full `CycleV2_tests`: 4,785 assertions in 347 test cases;
- Standalone Debug build completed with `--parallel 10`;
- the 16-command focused automation fixture completed with no failed command;
- `git diff --check` passed; and
- the modified DSP hot path contains no scalar `std::<math>` sample loop.

`clang-tidy` was not available in the development environment; compilation
with the repository warning settings and the explicit style/hot-loop audit are
the recorded substitutes.
