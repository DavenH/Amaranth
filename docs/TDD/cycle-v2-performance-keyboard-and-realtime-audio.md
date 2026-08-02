# Cycle V2 Performance Keyboard And Realtime Audio

## Status

Implemented (2026-08-02).

## Problem

Cycle V2 can compile and render graphs for previews and bounded automation
captures, but the standalone application does not own an audio device callback,
a realtime note queue, or synth voice allocation. `NodeWorkspace` currently
hosts only the graph canvas. Consequently, an onscreen keyboard cannot yet send
ordinary MIDI note events through the active graph to the system audio output.

The requested control is a compact, directly playable keyboard showing about
one octave. It must work without opening an editor or expanding a node. Pressing
a key must start a real synth note, releasing it must end that note, and the
standalone audio callback must deliver the resulting signal to the selected
audio output device.

This is not satisfied by changing preview state, calling the existing offline
`captureAudio` operation, or displaying a pressed key while no audio callback is
running.

## Decision

Add a floating **Performance Keyboard** to the workspace chrome, above the
canvas but outside the graph document. It is not a graph node.

This ownership is intentional:

- the keyboard has no ports and does not participate in graph topology;
- showing, moving, or playing it must not change the graph revision, preset,
  undo history, compilation, or DSP configuration;
- it must remain immediately playable without expansion; and
- the same keyboard should audition whichever graph is currently active.

The keyboard emits ordinary JUCE MIDI note-on and note-off messages into a
shared realtime event ingress. Hardware MIDI input uses the same ingress. A new
standalone audio engine owns the audio device, MIDI collection, voice pool,
prepared graph runtimes, and polyphonic summation. It consumes an immutable,
prepared graph generation published from the control side and renders it only
through the existing `GraphAudioExecutor` and oscillator-region runtime.

The first UI presents the white and black keys spanning C through the following
C, thirteen semitone positions in total. Its default range is MIDI 60-72. Note
names reuse `AmaranthMidiKeyboard::getText` so Cycle's established octave-label
convention remains consistent.
The range may be transposed by octave controls without resizing the widget.

## Goals

- Keep approximately one octave visible and playable at the normal workspace
  size.
- Support pointer press, hold, drag between keys, and release without opening
  another UI.
- Turn UI gestures into ordinary timestamped MIDI note events.
- Accept hardware MIDI through the same event and voice path.
- Render the active compiled graph in the standalone audio callback.
- Deliver non-silent stereo samples to the selected OS audio output while a
  playable graph and held note are active.
- Preserve sample offsets, note number, velocity, channel, note lifecycle, and
  per-channel controller state.
- Keep compilation, preparation, allocation, locking, graph mutation, and UI
  publication off the realtime thread.
- Make device state, note state, event drops, active voices, and output levels
  observable to focused automation.

## Non-Goals

- Making the keyboard a connectable or serializable node.
- Replacing the graph Output node or adding a hidden output route.
- Reimplementing Cycle 1 synthesis, mesh traversal, envelope playback, Unison,
  or oscillator-region DSP.
- Using preview traversal or offline audio capture as the live synth engine.
- A full piano, computer-keyboard mapping, sustain pedal UI, pitch/mod wheels,
  MPE, aftertouch, latch mode, arpeggiation, recording, or a sequencer in the
  first slice.
- A plugin-host audio path. This TDD covers the Cycle V2 standalone; a later
  plugin target should inject host MIDI and use the same voice/render core
  behind a different device adapter.
- Guaranteeing that an external amplifier, interface, or speaker is powered.
  Product acceptance ends at verified non-silent samples delivered by the
  running OS audio callback to its configured output channels.

## Authoritative Implementations

The implementation must reuse these sources rather than reproduce their
behavior:

- `juce::MidiKeyboardState` and `juce::MidiKeyboardComponent` are authoritative
  for keyboard note state, black/white key hit testing, drag transitions, and
  note-on/note-off production.
- `cycle/src/UI/Widgets/MidiKeyboard.*` is the mature Cycle 1 interaction
  reference. Cycle V2 may share or narrowly adapt the Amaranth/JUCE keyboard
  component, but must not import Cycle 1 singleton, console, sample-editor, or
  audition-key behavior.
- `lib/src/Audio/AudioHub.*` is the mature standalone device-lifecycle
  reference: `AudioDeviceManager`, `AudioSourcePlayer`, `MidiMessageCollector`,
  `MidiKeyboardState`, callback registration, device setup, and symmetric
  shutdown. Cycle V2 should extract/share a narrow device shell where practical
  or implement the same JUCE lifecycle behind a Cycle V2-specific owner. It
  must not route through Cycle 1 `SynthAudioSource`.
- `cycle-v2/src/Runtime/MidiControlState.*` remains authoritative for
  normalizing retained channel controllers and channel pressure into prepared
  voice controls.
- `cycle-v2/src/Runtime/GraphAudioExecutor.*` and
  `PreparedOscillatorRegion.*` remain authoritative for executing a compiled
  graph for one synth voice. The audio engine allocates voices and sums their
  outputs; it does not duplicate graph or oscillator processing.
- `GraphCompiler`, `GraphPresentationModel`, and the causal update graph remain
  authoritative for control-side graph compilation and configuration
  publication. The audio callback must never compile or read mutable
  `NodeGraph` state.
- `CycleV2Automation::captureAudio` remains the bounded offline diagnostic. It
  may share MIDI scheduling utilities with the realtime engine, but it is not a
  substitute for callback acceptance.

If the reusable keyboard base is trapped behind Cycle 1 domain dependencies,
extract the JUCE styling and interaction-neutral layer. Do not copy the mature
key geometry or drag state machine into a new Cycle V2 component.

## Target Architecture

```text
PerformanceKeyboard (message thread)       OS MIDI callback
              |                                  |
              +------ ordinary MidiMessage ------+
                                  |
                    bounded realtime MIDI ingress
                                  |
                         audio-device callback
                                  |
                 sample-offset event normalization
                                  |
            voice allocator + MidiControlState per block
                                  |
       prepared GraphAudioExecutor instance per active voice
                                  |
                        polyphonic stereo sum
                                  |
                    output gain / safety limiter
                                  |
                  OS audio output channel buffers

GraphDocument -> compile/configure off realtime -> immutable prepared generation
                                                   |
                                  block-boundary generation adoption
```

### Workspace And Widget Ownership

`NodeWorkspace` owns the floating keyboard host and lays it out relative to the
visible workspace, not canvas world coordinates. Pan and zoom therefore do not
move or scale the keyboard. The graph canvas remains visible beneath it and
does not receive pointer gestures captured by a key.

The host supplies a narrow `MidiEventSink` to the widget. The widget does not
know about `NodeGraph`, compilation, executors, voices, devices, or automation
reports. It renders keyboard state obtained from `MidiKeyboardState` and emits
MIDI messages through the sink.

The initial placement is bottom-centre with a compact background and enough
margin that the first and last key can be hit reliably. Exact dimensions are a
shared layout constant used by rendering, hit testing, and automation target
inspection. If canvas status chrome already occupies that region during
implementation, place the keyboard in a dedicated bottom performance strip;
do not put it in world space or overlap graph sockets.

### Interaction Contract

- Primary pointer down on a key sends note-on on MIDI channel 1 with the key's
  MIDI note number and velocity.
- Vertical position controls velocity using JUCE keyboard semantics, clamped to
  a small nonzero minimum through 1.0. The visible pressed state uses the exact
  emitted note and velocity state.
- Dragging while held releases the previous key and presses the newly entered
  key at the event boundary. Black keys retain hit-test priority in their
  visible area.
- Pointer up sends note-off for the currently held pointer note.
- Pointer cancellation, component hiding/destruction, focus loss that cancels
  the gesture, device stop, graph replacement, and application shutdown release
  all notes owned by the onscreen keyboard.
- Leaving the widget while the button remains down does not create a stuck
  note. The gesture either retains capture until release or explicitly ends the
  note at the boundary, matching the chosen JUCE behavior.
- The first slice supports one pointer-owned note at a time. The audio engine
  is polyphonic so hardware MIDI and future multitouch do not require a second
  render architecture.
- Octave-down and octave-up controls move the base by 12 semitones within MIDI
  0-127. Changing the displayed octave releases the widget-owned note first.
- The widget does not grab typing focus from graph shortcuts and text editors.
  Pointer ownership, not keyboard focus, governs note release.
- UI note events use the audio device clock mapping supplied by the ingress.
  They must not be backdated to an earlier block. The normal UI scheduling
  latency is at most the next callback block plus device latency.

### MIDI Event Ingress

Introduce a bounded, allocation-free ingress that accepts `MidiMessage` plus
source identity and monotonic timestamp from message-thread and MIDI-callback
producers. The consumer maps timestamps to offsets in the current audio block,
preserves ordering for equal timestamps, and clamps late events to offset zero.

The queue capacity and overflow policy are explicit. Overflow drops the newest
event, increments a diagnostic counter, and schedules a bounded recovery state
for the affected source/channel so a dropped note-off cannot leave a stuck
voice. Recovery is applied by the audio thread without locks or allocation.

Hardware MIDI from `AudioDeviceManager` and onscreen MIDI must not feed separate
voice allocators. Source identity exists only to support source-scoped note
cleanup; channel/note semantics remain standard MIDI.

### Voice Ownership And Polyphony

Add a fixed-capacity `RealtimeVoicePool`, prepared off the audio thread. Eight
voices are sufficient for the initial standalone contract and should be a
named configuration constant rather than a hidden executor default.

Each slot owns:

- one stable voice index;
- its own `GraphAudioExecutor` or an executor design proven to isolate all
  mutable per-voice processor and oscillator-region state;
- a preallocated `AudioVoiceContext` and event/control storage;
- MIDI channel, note number, velocity, start order, and lifecycle state; and
- preallocated stereo render buffers when the executor output cannot be summed
  directly.

Note-on allocates an idle slot, or steals the oldest active slot when full.
Stealing emits Reset for the old identity before NoteOn for the new identity at
the same sample offset. Note-off addresses the matching channel and note; it
does not stop an unrelated retriggered voice. Repeated notes therefore require
an explicit identity/order policy and tests.

Current oscillator regions ignore NoteOff and rely on envelope/tail ownership
for eventual silence. Before audible acceptance, the voice owner must have a
truthful completion policy derived from the compiled voice/envelope contract.
It may retain a released slot while a prepared tail is active, then reset and
return it to idle. It must not guess completion from a UI timer. If the current
runtime exposes no reliable tail/completion signal, that missing boundary is a
blocking extraction to add below the relevant domain processor; do not ship a
fixed-duration approximation as final voice lifecycle behavior.

`MidiControlState::beginBlock()` ingests each block's controller/pressure
events once. It then populates every applicable voice without destinations
parsing MIDI. Note number, velocity, normalized voice time, lifecycle events,
and MIDI channel are supplied by the voice owner.

### Prepared Graph Publication

`GraphPresentationModel::compileResult()` is mutable message-thread state and
its plan address is currently retained by prepared executors. The audio callback
must not hold a raw pointer into that snapshot while a later refresh replaces
it.

Introduce an immutable `PreparedAudioGraphGeneration` built entirely off the
realtime thread. It owns the `GraphExecutionPlan` for its full lifetime,
execution spec, revision, prepared voice runtimes, and any bounded
non-realtime-preparation products. Publication transfers a ready generation to
the audio engine through the repository's established lock-free or
block-boundary exchange pattern.

The audio callback:

- adopts only a complete prepared generation at a block boundary;
- never calls `GraphCompiler`, `prepareExecution`,
  `serviceNonRealtimePreparation`, serializer code, or preview execution;
- keeps the previous valid generation sounding if a new graph fails to
  compile or prepare, while UI status truthfully reports the mismatch;
- resets/reconciles active voices on generation replacement according to an
  explicit policy; the initial policy is all-notes-off plus bounded reset at
  the adoption boundary; and
- defers destruction of the retired generation to a non-realtime owner.

Configuration-only edits use the same generation contract initially. A later
optimization may publish narrower immutable configurations, but it must prove
that processor adoption and object lifetime remain realtime-safe before it
avoids full generation preparation.

### Standalone Audio Device

Add a Cycle V2 application-level `StandaloneAudioEngine` whose lifetime begins
before `NodeWorkspace` and ends after the workspace has released widget-owned
notes. It owns `AudioDeviceManager`, callback registration, MIDI input
registration, the realtime ingress, voice pool, prepared generation exchange,
and callback diagnostics.

The device shell follows `AudioHub` lifecycle behavior:

1. restore the last valid device setup from application properties where
   available;
2. initialize zero input and two output channels by default;
3. prepare the voice pool for the device's maximum callback size and sample
   rate before registering the callback;
4. register audio and MIDI callbacks only after preparation succeeds;
5. clear every output channel before mixing;
6. remove MIDI and audio callbacks before releasing voices or closing the
   device; and
7. expose device failure as visible workspace status and automation state.

The engine maps mono graph output equally to both output channels and preserves
stereo graph output. Extra device output channels are cleared. A small fixed
headroom gain is applied before a shared, existing safety limiter if an
authoritative limiter exists. If none exists, use fixed headroom and hard
finite/sample-range guards only; do not add an unreviewed nonlinear DSP
algorithm in the callback.

No callback operation may allocate, acquire a mutex, wait, log synchronously,
touch a JUCE `Component`, mutate the graph, or publish a dynamic UI object.
Meters and counters use atomics or a bounded snapshot copied by the message
thread.

### Device And Graph State Presentation

The keyboard host shows one concise state adjacent to the keys:

- `Audio ready` when a device and prepared graph generation are active;
- `Preparing audio` while a valid graph is being prepared;
- `Graph cannot play` when no valid generation exists; or
- `Audio device unavailable` with access to the device settings/error details.

A pressed-key highlight is not evidence that audio is ready. When no playable
generation or device exists, key gestures may still update MIDI state for
diagnostics, but the unavailable state remains visible and automation reports
zero delivered blocks. There is no silent fallback to offline capture.

## Thread And Lifecycle Boundaries

| Owner | Thread | Responsibilities | Forbidden |
| --- | --- | --- | --- |
| `PerformanceKeyboard` | JUCE message | Key UI, octave range, pointer gestures, source cleanup | Graph edits, voice allocation, audio rendering |
| graph/presentation owner | message + existing worker | Compile, configure, prepare generations, reject stale work | Device callback rendering |
| MIDI device callback | device/MIDI callback | Timestamp and enqueue bounded MIDI events | Voice or graph mutation, allocation |
| `StandaloneAudioEngine` | audio callback | Drain events, update controls/voices, render and sum, clear/write device buffers | Locks, allocation, compilation, UI access, preparation |
| retirement/preparation owner | non-realtime worker/message | Service preparation and destroy retired generations | Mutating an adopted generation |

Application shutdown and device restart use this order:

1. disable new UI and MIDI ingress;
2. enqueue/apply all-notes-off and stop callback delivery;
3. unregister MIDI and audio callbacks;
4. retire prepared graph generations and voice runtimes off realtime; and
5. destroy the workspace and device manager.

## Persistence

The keyboard is application chrome, so graph JSON stores none of its state.
Application properties may store visibility, base octave, MIDI input choices,
and JUCE audio device setup. They must not store pressed notes, active voices,
queue contents, controller state, or prepared graph data.

Opening or saving a graph while a note is held cannot serialize the gesture.
Loading a different graph releases active notes before adopting the new
prepared generation.

## Automation And Observability

Extend the Cycle V2 control surface with stable semantic operations rather than
pixel-only checks:

- inspect the keyboard target, visible note range, held UI note, and velocity;
- pointer-down, pointer-drag, and pointer-up on named key targets such as
  `PerformanceKeyboard.Note60`;
- inspect audio device readiness, sample rate, block size, callback count,
  active graph generation, active voice identities, dropped event count, and
  recent output peak/RMS;
- capture a bounded ring of samples copied from the live callback by a
  non-realtime consumer; and
- assert that captured samples came from callback generation and block IDs,
  not `GraphPresentationModel::captureAudio`.

The live capture tap is diagnostic-only, fixed-capacity, and downstream of the
polyphonic sum delivered to the device buffers. It must be disabled or nearly
zero-cost when unused. It does not become a parallel renderer.

## Semantic Tests

### Widget Interaction

- MIDI-60 pointer down emits exactly one channel-1 note-on with nonzero
  velocity; pointer up emits exactly one matching note-off.
- Pressing a black-key visual region chooses the black key rather than the
  white key beneath it.
- Dragging MIDI 60 to MIDI 62 emits note 60 off before note 62 on and leaves
  only note 62 held.
- Pointer cancellation, hiding, destruction, device stop, and graph load leave
  no widget-owned note held.
- Octave controls move the visible range by exactly 12 notes and clamp safely.
- A gesture creates no graph revision, undo entry, compilation, or serialized
  data change.
- Canvas pan/zoom and editor expansion do not move, scale, obscure, or disable
  the floating keyboard.

### MIDI Ingress And Voices

- UI and hardware sources produce the same ordered MIDI event representation
  and reach the same allocator.
- Events before, inside, and after a callback interval map to the documented
  offsets without reordering equal-time events.
- Eight simultaneous notes own eight isolated prepared voices and sum their
  output; a ninth applies the documented oldest-voice steal sequence.
- Note-off releases only the matching note identity, including repeated-note
  and stolen-voice cases.
- Controller and channel-pressure events are ingested once and reach every
  applicable channel voice through `MidiControlState`.
- Queue overflow increments diagnostics and bounded recovery prevents a stuck
  note.
- Device stop, graph replacement, and shutdown reset all voices.

### Graph Publication And Realtime Safety

- The callback never observes a partially compiled or partially prepared plan.
- A stale prepared generation is rejected; a failed new graph does not destroy
  the last valid generation until the stated replacement policy runs.
- Retired plan and processor destruction occurs off the audio thread.
- Device block sizes up to the prepared maximum render without resizing.
- Instrumented callback execution reports zero allocation, mutex acquisition,
  graph access, preview execution, serialization, synchronous logging, and
  non-realtime preparation.
- Per-voice executor state remains isolated during simultaneous and stolen
  notes.

### Audio Semantics

- A playable reference graph plus MIDI-60 note-on produces finite, non-silent
  samples through the normal `GraphAudioExecutor` voice path.
- The rendered pitch uses MIDI note 60 and the emitted velocity, rather than a
  fixed preview voice.
- A graph with no Output route delivers silence and reports `Graph cannot
  play`; it does not route an arbitrary intermediate node.
- Stereo output remains stereo, mono output is copied to both channels, and
  unused device channels are cleared.
- Note-off follows the authoritative Envelope/tail lifecycle and eventually
  returns the voice to idle without a fixed UI-duration approximation.
- Output remains finite and inside the documented safety range under maximum
  configured polyphony.

## Focused UI Automation Fixture

Add a fixture under `scripts/fixtures/` that launches the Cycle V2 standalone
with a known audible preset and a real configured output device or a CI virtual
device, then:

1. waits for `audioDeviceReady` and a prepared graph generation;
2. asserts that the visible keyboard range is MIDI 60-72;
3. sends pointer down to `PerformanceKeyboard.Note60` and holds across multiple
   live callbacks;
4. asserts MIDI 60 is active in one voice and callback-delivered peak/RMS exceed
   conservative non-silence thresholds;
5. drags to `PerformanceKeyboard.Note62` and verifies the complete note-60-off/
   note-62-on transition;
6. releases the pointer and verifies note 62 release plus eventual voice-idle
   state;
7. captures a screenshot showing the compact keyboard and its pressed state;
8. records the live callback sample artifact and device/generation diagnostics;
   and
9. quits cleanly with no active notes or callback-after-destruction failure.

The existing offline `captureAudio` command may be run as a supporting graph
sanity check, but its result cannot satisfy any live callback assertion.

When no CI audio device exists, the fixture must fail or be explicitly skipped
as `audio device unavailable`; it must not silently pass against offline
rendering. Local macOS acceptance uses the selected physical output or a named
loopback device and records the device name in the report.

## Acceptance Criteria

The feature is complete only when all of the following are true:

- The Cycle V2 workspace visibly presents a compact MIDI 60-72 keyboard that is
  playable without adding or expanding a node.
- Pressing and holding a visible key produces an ordinary MIDI note-on with the
  correct note and velocity; releasing it produces the matching note-off.
- That event allocates a synth voice and renders the active compiled graph
  through `GraphAudioExecutor` in the running standalone audio callback.
- Non-silent, finite samples from that held note are written to the configured
  OS output channels in realtime and are observed downstream of the live
  polyphonic sum.
- The focused live-device fixture proves the callback, generation, voice,
  note, and output-level sequence. Offline capture alone is explicitly
  insufficient.
- Note release, drag, cancellation, device restart, graph replacement, and
  shutdown leave no stuck voices.
- UI/hardware MIDI share one ingress and voice path.
- Realtime instrumentation proves no allocation, lock, graph mutation,
  compilation, preparation, UI access, or retired-object destruction occurs in
  the callback.
- Keyboard gestures and application settings do not alter preset JSON, graph
  revisions, or undo history.
- Device or graph failure is visible and cannot be mistaken for successful
  playback.

The human acceptance gesture is deliberately simple: launch Cycle V2 with the
reference preset, press the first C on the onscreen keyboard, hear the
synthesizer in realtime, and release it without a stuck note. The automated
callback evidence must pass before that manual check is reported as successful.

## Negative Architecture Boundaries

- Do not add `NodeKind::Keyboard`, a palette entry, graph ports, graph edges, or
  keyboard serialization.
- Do not make `NodeWorkspace`, `NodeCanvas`, or `MainWindow` implement synth
  voice allocation or DSP.
- Do not call `GraphPresentationModel::captureAudio` from a key gesture.
- Do not pass mutable `GraphExecutionPlan` or `NodeGraph` references to the
  audio callback.
- Do not call `GraphAudioExecutor::prepareExecution` or
  `serviceNonRealtimePreparation` in the callback.
- Do not share one mutable executor/oscillator processor across simultaneous
  voices unless an explicit refactor proves state isolation.
- Do not copy Cycle 1 `SynthAudioSource`, voice, keyboard hit-testing, or
  device-manager state machines into Cycle V2.
- Do not use a timer-generated tone, fixed waveform, fake nonzero buffer, or
  test-only signal as acceptance evidence.
- Do not bless a fixed note duration as a replacement for Envelope/tail voice
  completion.
- Do not let a queue overflow, hidden widget, device stop, or graph load leave
  an unreleased note.

## Expected Implementation Surface

The initial complete feature is expected to add approximately 900-1,500 lines
of production C++ excluding tests and automation fixtures:

- 150-250 lines for the shared/styled keyboard widget and workspace host;
- 150-250 lines for bounded MIDI ingress and scheduling;
- 250-400 lines for voice allocation, lifecycle, and polyphonic mixing;
- 250-400 lines for device ownership and prepared-generation exchange; and
- small integration additions to application startup, workspace state, and
  automation inspection.

No new production implementation file should normally exceed 350 lines. A
larger audio-engine class, DSP code in the widget/workspace, repeated node-kind
branches, or a large Cycle 1 compatibility adapter is evidence that ownership
is mixed and requires refactoring before acceptance.

Implementation review must report:

- production lines added/removed and largest changed files;
- whether keyboard geometry was reused or copied;
- event queue capacity, producer model, overflow recovery, and measured drops;
- voice count, steal policy, repeated-note identity policy, and tail-completion
  authority;
- how plan lifetime, preparation, adoption, and retirement avoid the realtime
  thread;
- proof that `GraphAudioExecutor`, oscillator regions, Envelope playback, and
  `MidiControlState` were reused unchanged or the exact justified extraction;
- every callback allocation/lock instrumentation result; and
- live device name, sample rate, block size, callback metrics, and fixture
  artifact paths.

## Delivery Slices

1. **Keyboard interaction boundary**: extract/reuse the JUCE keyboard surface,
   add the floating workspace host and semantic automation targets, and prove
   exact note-on/off/drag/cancel events without graph mutation.
2. **Realtime ingress and voice owner**: add the bounded shared MIDI queue,
   fixed voice pool, note identity/steal behavior, `MidiControlState`
   integration, and lifecycle tests against the existing executor.
3. **Prepared generation exchange**: make graph plan/configuration lifetime
   immutable and prepare every voice off realtime, including stale rejection
   and non-realtime retirement.
4. **Standalone device wiring**: add application-owned JUCE audio/MIDI device
   lifecycle, callback rendering, channel mapping, device status, and settings
   persistence.
5. **Live acceptance fixture**: drive the actual key gesture through multiple
   callbacks, assert audible output metrics and release, capture artifacts, and
   distinguish the result from offline capture.
6. **Refactor and boundary audit**: reread the production diff, extract mixed
   responsibilities, remove scaffolding, inspect hot loops for scalar
   `std::<math>`, run style/clang-tidy where available, and commit the complete
   slice.

Each slice follows the repository engineering loop. A working key highlight,
an isolated executor test, or an offline non-silent buffer is progress, not
completion. Do not report the feature fixed or complete until the live callback
acceptance sequence passes.

## Deletion And Stable End State

- Any temporary direct `NodeWorkspace` callback into the audio engine is
  deleted once the narrow `MidiEventSink` and status view are in place.
- Any temporary raw plan pointer or message-thread executor handoff is deleted
  in the prepared-generation slice before device callback enablement.
- Any duplicated Cycle 1 keyboard styling is replaced by an extracted shared
  style/base component before completion.
- Test-only callback taps remain bounded diagnostics downstream of the real
  device render; fake renderers and temporary tones are deleted.
- The stable end state has one graph compiler/publication path, one MIDI
  ingress, one voice allocator, one graph DSP renderer, and distinct adapters
  for UI MIDI, hardware MIDI, and standalone device output.

## Implementation Review

The completed implementation follows the ownership in this design. The
keyboard is workspace chrome and reuses `AmaranthMidiKeyboard`,
`MidiKeyboardState`, and JUCE key geometry and drag handling. It adds no node
kind, ports, graph branches, serialization, or copied key hit-testing. The
widget only translates keyboard-state notifications to a `MidiEventSink`.

The shared queue contains 512 events and uses a bounded multi-producer,
single-consumer ring. UI and hardware messages retain source, channel, MIDI
data, monotonic timestamp, and sequence. A fixed 1,024-entry renderer-side
scheduling buffer orders timestamp/sequence pairs, clamps late messages to the
block start, and retains future messages for later callbacks. Queue overflow
drops the newest event, increments the diagnostic counter, and requests
source-scoped all-notes-off recovery. The live fixture measured zero drops.

The renderer owns eight stable voice slots. An idle slot is preferred; the
oldest voice is reset and stolen when all slots are active. Repeated note-offs
release the oldest matching outstanding source/channel/note identity.
`GraphAudioExecutor`, prepared oscillator regions, and `MidiControlState` are
reused as the render and controller authorities. Envelope playback gained only
a narrow active-tail query through `NodeAudioProcessor`; its playback
algorithm was not copied or replaced. Released voices return to idle when that
authoritative processor state reports no active tail, rather than after a UI
timer or fixed release duration.

Each immutable prepared generation owns its execution plan and the executor
prepared for every stable voice index. Preparation and non-realtime servicing
run outside the callback. The callback adopts a complete pending pointer only
at a block boundary, resets voices under the documented replacement policy,
and publishes the previous generation to a retirement slot. The message-thread
timer reclaims retired ownership, so plan and processor destruction do not run
on the audio thread. Failed graph publication leaves the previous prepared
generation owned while the workspace reports that the current graph cannot
play.

The production diff added 1,634 lines and removed 10 lines under `cycle-v2/src`
before this review. The largest files are `RealtimeGraphRenderer.cpp` at 341
added lines, `StandaloneAudioEngine.cpp` at 310, and `NodeWorkspace.cpp` at 210
additions and 4 removals. The slight increase over the estimate is the bounded
live-callback capture and semantic automation surface required to distinguish
device output from offline rendering. No production implementation exceeds
350 added lines, and the audit found no new `NodeKind` switch or compatibility
adapter.

Realtime instrumentation covers a prepared graph render and the complete
voice-mixing path. Both report zero allocations and zero lock acquisitions.
The hot-loop audit found no scalar `std::<math>` operation inside a
per-sample/bin/pixel loop; `Buffer` operations perform clearing, summation,
headroom, clipping, norms, and peak reduction. The remaining `std::isfinite`
checks run once per output channel and `std::sqrt` once per rendered block or
non-realtime capture result.

## Verification

- `CycleV2_tests`: 435 test cases and 6,653 assertions passed.
- Focused audio-device/realtime suite: 4 test cases and 28 assertions passed.
- Focused keyboard suite: 2 test cases and 17 assertions passed.
- Standalone Debug and test targets built with `--parallel 10`.
- `git diff --check` passed. `clang-tidy` was unavailable in the configured
  environment.
- The focused fixture passed every keyboard, device, voice, output-level,
  drag, release, and eventual-idle assertion on `MacBook Pro Speakers` at
  44,100 Hz with a 512-frame device block.
- Its 500 ms callback capture contains 22,050 frames from callbacks 45-88,
  with peak `0.0704063` and RMS `0.0332317`. The report records
  `source: audioDeviceCallback`, graph revision 2, and zero dropped events.
- Local artifacts: `/private/tmp/cycle-v2-performance-keyboard-report-final.json`,
  `/private/tmp/cycle-v2-performance-keyboard-live.wav`, and
  `/private/tmp/cycle-v2-performance-keyboard.png`.
- A native OS capture caught the OpenGL canvas covering the initial sibling
  overlay. The final layout reserves a strip outside the OpenGL component;
  `/private/tmp/cycle-v2-keyboard-after-os.png` verifies that the keyboard
  remains visible after context startup.

The filtered launch log contains the already-recorded JUCE `Settings.cpp:223`
and `Settings.cpp:224` assertions. They remain tracked in `ui-bugs.md`; they did
not interrupt device startup, callbacks, rendering, capture, or clean fixture
completion.
