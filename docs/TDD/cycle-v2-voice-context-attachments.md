# Cycle V2 Voice Context And Voice Attachments

## Status

Implemented on `cycle2/voice-context-attachments`. Typed attachment routing,
the compiled Voice Context boundary, configuration-only Unison and Modulation
Triple products, Envelope purpose, runtime modulation defaults, shared
pitch-phase integration, transient Unison editor feedback, and the focused
Voice Context editor are complete. The compact summary shows its highest-value
voice properties in explicit language, and the expanded editor exposes the
global preview Voice Length as a continuously updating slider. Shared socket
and attachment-icon presentation is tracked separately by
`cycle-v2-port-icon-presentation.md`.

Audible time-only and spectral oscillator lowering is intentionally owned by
the downstream `cycle-v2-oscillator-region-compilation.md` TDD. It depends on
this completed configuration boundary and is not a completion criterion of
this document.

Supersedes the current `Voice Context -> Unison -> source` signal-chain model
in `cycle-v2-unison-parity.md`. It extends the control-routing contract in
`cycle-v2-dynamic-envelope-modulation.md` and uses the edit/invalidation model
in `cycle-v2-causal-update-graph.md`. Oscillator-region discovery and runtime
lowering are specified in `cycle-v2-oscillator-region-compilation.md`.

## Problem

Cycle V2 currently models Voice Context as a silent processor with no inputs
and one `DomainContext` output. Unison has `DomainContext` input and output
ports and a passthrough audio processor. That topology implies that Unison
transforms a stream. It does not: Unison is immutable voice configuration
consumed when a voice context prepares oscillator lanes.

The current model also has no owner for three related voice defaults:

- the default yellow/red/blue modulation triple inherited by compatible nodes;
- the pitch envelope that changes oscillator frequency over a note;
- the unison layout that fans each synth voice into oscillator lanes.

Without one explicit owner, implementations would have to discover nearby
nodes, infer meaning from port IDs, copy configuration through signal buffers,
or install hidden global routing. None of those approaches gives multiple
voice contexts deterministic and independently testable behavior.

Envelope nodes have a second ambiguity. One `EnvelopeSignal` output currently
serves scratch attachment and ordinary control use, while Cycle 1 distinguishes
scratch, pitch, and volume/control envelope purposes. Cycle V2 needs that
purpose to be persistent, visible, and part of graph validation. The complete
four-purpose contract is defined in
`cycle-v2-envelope-purpose-routing-and-scaling.md`.

Finally, the current Unison editor repaints on every slider callback but builds
its plot from the last document-bound node snapshot. During a compound drag,
that snapshot can lag the transient slider value. A visualization that updates
only when the gesture commits violates the continuous authoring contract.

## Decision

Keep Unison as a distinct node and make it a typed configuration attachment to
Voice Context.

This preserves a recognizable Unison authoring surface, permits one Unison
configuration to be shared by multiple voice contexts, and lets one voice
context share its configuration across multiple oscillator branches. Unison
does not participate in signal execution and is not a passthrough processor.

Voice Context owns resolution of voice-wide defaults and publishes one
immutable compiled voice plan. It does not own the Unison parameters or
Envelope mesh; it holds resolved references to their immutable products.

## Authoritative Implementations

- Cycle 1 unison layout, tuning, phase, pan, and voice gain:
  `cycle/src/Audio/Effects/Unison.*`,
  `cycle/src/Audio/Voices/SynthUnisonVoice.*`, and
  `cycle/src/Audio/Voices/CycleBasedVoice.*`.
- Cycle 1 pitch-envelope tuning and Unison visualization:
  `CycleBasedVoice::getAngleDelta` and
  `cycle/src/UI/VisualDsp/UnisonPhaseColumnRenderer.h`.
- Cycle 1 envelope purpose and lifecycle behavior:
  `cycle/src/Inter/EnvelopeInter2D.*`, the Envelope rasterizers, and
  `EnvelopePlaybackEngine` shared through the existing Cycle V2 envelope TDDs.
- Cycle V2 modulation source/triple evaluation:
  `cycle-v2/src/Nodes/Control/ModulationSource.*` and
  `cycle-v2/src/Nodes/Control/ModulationTriple.*`.
- Cycle V2 graph validation, compilation, causal editing, and immutable
  configuration publication remain the orchestration boundary.

Mature tuning, envelope playback, mesh traversal, and unison layout behavior
must be extracted or adapted narrowly. The Voice Context compiler must not
reimplement those algorithms.

## Graph Model

### Connection kinds

Signal flow and configuration attachment are different edge kinds:

```cpp
enum class ConnectionKind {
    Signal,
    ConfigurationAttachment,
    ProcessingAttachment
};
```

The exact representation may differ, but a generic `bool attachment` is not a
sufficient stable end state. A configuration edge:

- establishes configuration dependency and sharing;
- does not allocate a signal buffer;
- does not create a runtime processor invocation;
- is compiled before the consuming Voice Context plan;
- is immutable for one published graph generation.

`ProcessingAttachment` retains the established scratch-envelope semantics: it
binds prepared envelope behavior to a compatible processor and is distinct
from both sample/control dataflow and voice configuration.

Attachment compatibility is typed metadata. Do not infer it from node kind,
display label, port ID, palette group, or cable colour.

### Voice Context ports

Voice Context has three inputs and one output:

| Port | Connection | Type | Cardinality | Meaning |
| --- | --- | --- | --- | --- |
| `modulation` | Configuration attachment | Modulation triple | zero or one | Default yellow/red/blue controls |
| `pitch` | Signal | Pitch envelope | zero or one | Per-voice pitch trajectory |
| `unison` | Configuration attachment | Unison configuration | zero or one | Per-voice oscillator-lane layout |
| `context` | Output | Domain context | fan-out | Resolved voice plan for voice-aware sources |

The `context` edge is not an audio sample stream. It scopes downstream
voice-aware graph branches to the compiled voice plan. One context may feed
multiple oscillator/source branches. Each branch observes the same voice note,
pitch envelope, default modulation policy, and Unison layout.

Every voice-aware node resolves to exactly one Voice Context. Nodes reached by
two different contexts are invalid until an explicit context-merge semantic is
designed. A pitch Envelope connected to a Voice Context belongs to that
context even though its signal is evaluated before the context publishes
oscillator controls. A scratch Envelope inherits the unique context of its
attachment target. The compiler reports ambiguous or context-free default
modulation instead of choosing a context by traversal order.

Unconnected inputs use explicit defaults:

- modulation uses the canonical Modulation Triple defaults;
- pitch is constant at the Cycle 1 neutral pitch-envelope value;
- unison is enabled with one centred, zero-detune, zero-phase lane.

Voice Context keeps domain, octave, pitch, portamento, and oversampling until
each is migrated to the compiled voice plan. Polyphony is synth-level voice
allocation and is not a Voice Context property. Unison order remains owned by
the attached Unison node. Serialized legacy `voices` values are accepted and
discarded during graph loading rather than retained as hidden node state.

### Unison node

Unison has no signal input. It exposes one typed `unison` configuration output
that is connectable only to a Voice Context `unison` input. The node publishes
the existing immutable `UnisonNodeConfiguration` and retains its dedicated
compact preview and effect-style editor.

The compiler folds the selected immutable Unison configuration into the
consumer's voice plan. It must remove the current Unison passthrough audio
processor, `DomainContext` input/output pair, signal buffer, and execution step.
One Unison node may attach to multiple Voice Context nodes. Each consumer owns
its own runtime oscillator-lane state; the shared configuration owns no phase
history.

### Modulation Triple

Modulation Triple retains its yellow, red, and blue `ControlSignal` outputs for
explicit connections. It additionally exposes one immutable aggregate
configuration output for the Voice Context `modulation` attachment.

The compact node presents these products through one right-side authoring
socket. Dropping that socket on a Voice Context modulation input authors the
single aggregate configuration edge. Dropping it directly on a Trilinear Mesh
or Envelope remains the explicit per-axis override gesture. The aggregate
output must not also appear as a second bottom socket.

Using only the aggregate attachment does not schedule the Modulation Triple as
a three-buffer runtime node. If any explicit yellow/red/blue signal output is
used, its existing processor may execute for those live outputs. Both products
must consume the same immutable source configurations.

Compatible modulation inputs declare a semantic default slot:

```cpp
enum class DefaultModulationSlot {
    None,
    Yellow,
    Red,
    Blue
};
```

For each such input, resolution is:

```text
effective axis = explicit connected signal
               ?? Voice Context default axis
```

This precedence is per axis. Connecting an Envelope's red input overrides only
red; its unconnected blue input still inherits the Voice Context blue default.
A node with no declared default slot receives no implicit modulation. Add,
Multiply, effect parameters, and arbitrary `ControlSignal` ports must not
inherit defaults merely because their signal type is compatible.

Default resolution belongs in the compiled voice plan or a shared modulation
input resolver. Individual node processors consume already-resolved inputs;
they do not search graph ancestry or query a singleton Voice Context.

## Envelope Purpose

Envelope purpose, migration, presentation, polarity, logarithmic scaling, and
scratch execution are defined by
`cycle-v2-envelope-purpose-routing-and-scaling.md`. In particular, the stable
selector has four purposes: `control`, `volume`, `pitch`, and `scratch`.

This TDD consumes the `pitch` product after the Voice Context port is
available. It does not redefine Envelope output semantics or provide a
temporary generic-control route while that port is absent.

## Voice Context Presentation

The compact Voice Context keeps the established start-domain selector and
shows its three inputs as distinct, legible ports. It does not repeat cable
attachment state as labels or rows: the graph topology already communicates
which Modulation Triple, pitch Envelope, and Unison nodes are attached.

The inputs form one evenly spaced stack on the left. Socket geometry, colour,
semantic icons, and the reserved interior icon gutter follow
`cycle-v2-port-icon-presentation.md`. The node height accommodates the complete
port stack instead of allowing ports to run through its boundary.

Below the start-domain selector, the compact summary uses one restrained line:
`Octave <value> · <duration> <second/seconds>`, followed by `· Glide` only when
glide is enabled. It reads voice length from the global preview context rather
than serializing it as a Voice Context parameter. Fractional durations retain
only meaningful decimal places. Summary typography matches the
`Waveform / Spectral` labels instead of shrinking to fit additional prose.

The summary uses complete, immediately understandable labels and units.
It does not show attachment state, Unison parameters, oversampling, or
synth-level polyphony.

The expanded editor presents only Voice Context-owned controls: source domain,
octave, pitch, portamento, and oversampling. It does not embed or
navigate redundant attachment rows. Unison retains its own graphic and editor.

It also presents the global preview Voice Length with Cycle 1's authoritative
`exp(8 * unit - 3)` mapping (approximately 0.05 to 148.41 seconds), shared
through `CycleDsp::voiceLengthSeconds` and its inverse. This is audition context
rather than graph configuration: it is
not serialized into the Voice Context node and does not create graph revisions
or undo entries. Movement updates the compact Voice Context summary and all
Unison previews immediately. The current duration is shown in seconds beside
the slider using Cycle 1's rounded value and the compact `s` unit.

Octave, Voice Length, Pitch, and Oversampling share the same down-drag-up
interaction contract. The graph-backed controls publish one graph undo entry
when the gesture ends; Voice Length remains global preview state. Voice Length
shows labelled duration ticks, Octave shows labelled value ticks, and Pitch
shows its current value in semitones. Preview cache invalidation is
signature-scoped so dragging Voice Length does not discard unrelated node
sprites.

The expanded editor uses one three-column control grid: right-aligned names,
equal-width slider tracks, and left-aligned current values. Domain labels the
waveform/spectral selector. Oversampling participates in the same draggable,
single-undo gesture contract as Octave and Pitch, and Portamento follows all
slider rows. The editor is 20 percent taller than the first Voice Length layout
so tick labels and rows do not compete vertically.

## Compiled Voice Plan

Compilation produces an immutable plan per Voice Context:

```cpp
struct CompiledVoiceContext {
    VoiceContextConfiguration voice;
    ModulationTripleConfiguration defaultModulation;
    PreparedPitchEnvelopeReference pitchEnvelope;
    UnisonGroupConfiguration unison;
    UnisonVoiceLayout lanes;
};
```

Names and ownership may change, but the separation must remain:

- authoring nodes own serializable parameters/models;
- configuration attachments contribute immutable configuration;
- the Voice Context compiler resolves defaults, explicit overrides, and
  attachment cardinality;
- per-synth-voice runtime state owns note lifecycle and pitch-envelope cursor;
- per-Unison-lane runtime state owns oscillator phase and accumulation state;
- downstream oscillators consume a prepared view and never mutate authoring
  configuration.

Configuration dependency order is separate from sample/control execution
order. A configuration attachment must not create a fake buffer or force a
node into the realtime step list.

The compiled Voice Context is an input to oscillator-region compilation, not
one monolithic oscillator executor. Each region selects its own time-only or
spectral strategy while consuming the same resolved note, pitch-envelope,
modulation, and Unison configuration. Spectral processing in one oscillator
region must not change the execution strategy of a sibling region.

## Pitch, Unison, And Preview Accuracy

Voice Context owns the audition context used by voice-related previews:

- preview MIDI note;
- preview voice duration;
- the selected/attached pitch envelope;
- the attached or default Unison configuration.

These values are supplied explicitly to the Unison renderer; Unison does not
query `NodeCanvas` or global settings. With no pitch envelope, each detuned
voice remains the current straight wrapped path. With a pitch envelope, the
relative phase is the integral of the same instantaneous oscillator-frequency
difference used by audio:

```text
phase_i(t) = initialPhase_i
           + integral(0..t,
               f(note + pitchEnvelope(s), detune_i)
             - f(note + pitchEnvelope(s), 0)) ds
```

The implementation must reuse shared pitch-envelope sampling and oscillator
tuning. It must not approximate the envelope by its sustain value or add a
renderer-only tuning formula. Analytical straight segments remain valid for a
constant pitch; time-varying pitch may use a bounded prepared trajectory whose
resolution and integration error are specified and tested.

One Voice Context feeding multiple oscillators yields one shared explanatory
Unison preview. Oscillator-specific Unison overrides are a future graph
feature and must not be smuggled into the first attachment implementation.

## Continuous Authoring Feedback

Every accepted slider movement updates the Unison visualization before the
gesture ends. Gesture end commits history and durable publication; it is not a
visual refresh boundary.

The renderer consumes the current immutable transient node snapshot produced
by `beginNodeParameterEdit` / `updateNodeParameterEditValue`, or an equivalent
editor-local configuration built through the authoritative parameter
normalizer. Calling `repaint()` while continuing to render the stale bound
document node does not satisfy this contract.

For each effective movement:

- readout and expanded plot update synchronously;
- the compact preview receives the same transient revision in the next
  coalesced presentation pass;
- integer order changes add/remove paths immediately;
- width, phase, and jitter move paths continuously;
- no undo entry or durable graph revision is created until commit;
- the final commit does not recompute an already-current visual product.

This follows `cycle-v2-causal-update-graph.md`: movement and commit are
different phases of one gesture, and product fingerprints prevent duplicate
work.

## Threading And Realtime Boundaries

- Configuration attachment resolution, envelope preparation, and lane-plan
  construction occur off the realtime audio thread.
- Published voice plans are immutable and adopted only at an explicit safe
  boundary.
- Note, envelope cursor, portamento, and oscillator phase state are per synth
  voice; Unison oscillator state is additionally per lane.
- Modulation defaults are evaluated per voice where their source is per voice.
- No graph traversal, attachment lookup, allocation, serialization, mutex
  wait, or UI publication occurs in a realtime process callback.
- Oscillator-region lowering must state whether a Voice Context plan
  replacement makes active voices retain or restart pitch-envelope,
  portamento, and lane state. That downstream adoption policy must be tested
  rather than inherited accidentally.

## Negative Boundaries

- Do not model Unison as a `DomainContext` signal transform or audio
  passthrough.
- Do not use signal buffers to transport immutable configuration.
- Do not make Voice Context discover nodes by proximity, palette group, port
  ID, or graph-wide singleton lookup.
- Do not inject default modulation into undeclared generic control inputs.
- Do not flatten pitch, scratch, and generic envelopes into one ambiguous
  output domain.
- Do not copy envelope playback, oscillator tuning, or Unison layout into the
  Voice Context compiler or preview renderer.
- Do not combine the Unison editor into Voice Context merely because Voice
  Context consumes it.
- Do not make gesture end the first point where a truthful visualization sees
  the edited value.

## Implementation Slices

1. Introduce typed connection/attachment metadata and migrate existing scratch
   attachments without changing their behavior.
2. Add Voice Context modulation, pitch, and Unison inputs plus the compiled
   immutable voice-plan boundary.
3. Convert Unison from `DomainContext` passthrough to a configuration-only
   attachment and remove its runtime signal step.
4. Add Modulation Triple aggregate attachment output and explicit per-axis
   default-slot metadata; implement override resolution.
5. Implement the Envelope purpose and pitch-facing slices from
   `cycle-v2-envelope-purpose-routing-and-scaling.md`.
6. Integrate prepared pitch-envelope sampling and shared oscillator tuning into
   the voice plan and Unison preview trajectory.
7. Route transient effect-editor snapshots to expanded and compact Unison
   previews so every slider movement is visible.

Each slice receives its own refactor, style, semantic-test, automation, and
commit pass. Passing schema tests does not permit a fake runtime adapter to
survive into the next slice.

## Follow-up Dependency

`cycle-v2-oscillator-region-compilation.md` consumes the compiled Voice Context
to partition oscillator regions, lower time-only and spectral Unison
strategies, define active-voice plan adoption, materialize oscillator blocks,
and prove audible Cycle 1 parity. Those responsibilities remain outside this
configuration-attachment TDD so that the dependency direction stays explicit.

## Verification

### Graph and attachments

- Unison connects to Voice Context `unison` and to no signal-processing port.
- Unison fan-out shares immutable configuration while runtime lane state stays
  isolated per Voice Context and synth voice.
- Configuration attachments allocate no graph signal buffer and create no
  realtime execution step.
- Duplicate attachments to a single-cardinality input are rejected before
  publication.
- Saved graphs round-trip connection kind and attachment type.

### Modulation defaults

- With no explicit input, declared yellow/red/blue axes use the attached Voice
  Context Modulation Triple.
- A direct red connection overrides red only; yellow and blue still inherit.
- Nodes and ports without default-slot metadata receive no implicit value.
- Two Voice Contexts with different triples produce independent effective
  values through the same downstream node configuration.

### Envelope purpose

- Control, volume, pitch, and scratch choices round-trip and resolve to their
  declared connection semantics.
- Changing purpose removes incompatible edges atomically and undo restores the
  purpose and edges together.
- Legacy scratch attachments migrate to scratch; ordinary envelopes migrate to
  control.
- Compact raster tests distinguish all three purposes without relying only on
  colour.
- Pitch neutral centre, live `[0.01, 0.99]` clamp, endpoint presentation, and
  unit-to-semitone mapping match Cycle 1.

### Voice plan and preview

- Unconnected Voice Context inputs produce the documented defaults.
- Attached pitch is prepared through the shared Envelope playback engine and
  changes preview phase through the shared oscillator-tuning mapping without
  mutating the preview MIDI note.
- The immutable Unison layout carries detune, phase, pan, and gain exactly once
  for each downstream lane-state owner.
- Constant-pitch Unison paths match the existing straight-line golden values.
- A nonconstant pitch envelope bends/integrates paths and matches sampled audio
  phase within the declared error bound.

### Interaction

- Automation drags each Unison control through at least three intermediate
  values and observes distinct plot revisions before pointer-up.
- Expanded and compact plots show the same transient configuration.
- One drag is one undo action; commit performs no duplicate plot computation.
- Envelope purpose is legible in compact and expanded automation state and in
  an OS-level screenshot.

## Completion Criteria

- Voice Context is the explicit owner of resolved voice-wide defaults and
  publishes an immutable compiled voice plan.
- Its modulation, pitch, and Unison inputs have typed, truthful connection
  semantics.
- Unison remains a distinct reusable node but has no fake signal ports,
  passthrough processor, or runtime buffer.
- Modulation Triple inheritance and per-node override precedence are explicit
  per axis and covered by semantic tests.
- Envelope purpose is persistent, visually unmistakable, migration-safe, and
  determines valid connection semantics.
- Pitch-envelope and Unison behavior share Cycle 1's mature tuning, playback,
  and layout cores.
- Unison previews update from every effective slider movement and accurately
  reflect Voice Context note, duration, pitch envelope, and Unison state.
- The compiled boundary permits multiple oscillator branches to reference one
  immutable Voice Context plan; oscillator-region lowering owns their separate
  mutable lane state.

## Implementation Evidence

- `53e2df40` introduces typed attachments, Voice Context compilation,
  configuration-only Unison and Modulation Triple products, Envelope purpose,
  serialization migration, and continuous Unison feedback.
- `d98710f9` resolves per-axis Voice Context modulation defaults while preserving
  explicit-input precedence.
- `dd1f691c` makes the compact Envelope purpose selector interactive.
- `70519e85` prepares attached pitch-envelope playback and supplies it to both
  compact and expanded Unison previews.
- The default-modulation preset slice replaces repeated per-axis edges in the
  three ported Cycle 1 presets with one typed Modulation Triple attachment to
  Voice Context. The shared right-side socket authors both that default edge
  and deliberate explicit per-node bundles without exposing a duplicate bottom
  socket. Context assignment follows signal flow, Envelope sidechain consumers,
  and scratch attachment targets so volume and scratch Envelopes inherit the
  same defaults as context-fed meshes.
- The compact Voice Context summary reads global preview duration and shows
  octave, glide, and voice length without exposing oversampling or polyphony.
- Cycle V2 verification passes 5,352 assertions in 388 test cases.
- `scripts/fixtures/cycle-v2-agent-voice-context-attachments.json` verifies the
  four-node attachment topology, compact summary, expanded Voice Context
  presentation, complete Octave/Pitch/Voice Length/Oversampling drag gestures,
  and their resulting graph or preview state. Filtered launch logs are
  `/private/tmp/cycle-v2-voice-context-layout-logs.txt`. The reviewed
  OS-level control capture is
  `/private/tmp/cycle-v2-voice-context-layout-os.png`.
