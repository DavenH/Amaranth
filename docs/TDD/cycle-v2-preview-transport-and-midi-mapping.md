# Cycle V2 Preview Transport And MIDI Mapping

## Status

Implemented (2026-09-13).

## Problem

Cycle V2's performance keyboard emits note velocity, but the audible voice must
also consume that value through the Mod Triple assigned to its Voice Context.
Changing an axis from velocity to constant or inverse velocity currently does
not reliably change the rendered morph position.

The keyboard also lacks Cycle 1's bounded preview transport: a single action
that plays the selected preview note for the authored voice duration and shows
the elapsed position. The traversal cursor can follow later, but the transport
must establish the same note, duration, and progress ownership now.

## Authoritative Implementations

- `ModulationSource::renderAudioBlock` owns velocity, inverse velocity,
  constant, key-scale, and voice-time evaluation. No second mapping formula is
  introduced.
- `GraphCompiler::compileVoiceContexts` and its default-modulation buffers own
  Mod Triple assignment to voice-scoped Trimesh inputs.
- `RealtimeGraphRenderer` owns per-voice MIDI velocity and the normalized voice
  clock. The transport emits ordinary MIDI through `MidiEventSink`; it does not
  invoke DSP or mutate the graph.
- Cycle 1 `PlaybackPanel::startPlayback`, `stopPlayback`, and `togglePlayback`
  are the interaction reference: restart at the beginning, audition one preview
  note, and stop at the duration boundary.
- `PreviewPitchResolver` owns the preview-note default and key-scale
  normalization. The selected note is transient workspace state and is supplied
  as preview context, not serialized into a Mod Triple constant.
- `juce::MidiKeyboardComponent` remains authoritative for key geometry and
  ordinary primary-button note gestures.

## Design

`PerformanceKeyboardPanel` gains a 28-pixel transport band above the existing
key bed. Its visible hierarchy is one centred play/stop button over a quiet
full-width progress track. The exact key bed geometry remains unchanged. The
button has a larger hit target than its glyph and the progress fill is a status
indicator, not a seek control in this slice.

The panel owns only the gesture clock. It receives callbacks for the current
preview note and maximum compiled Voice Context duration, emits note-on/off via
the existing sink, and reports progress from zero through one. Playback stops
and releases its owned note when the duration expires, the graph changes, the
panel hides, or playback is toggled off.

Right-clicking a visible key selects that key as the transient preview note
without sounding it. The selected key receives an outline/highlight distinct
from the pressed-note fill. `NodeCanvas` supplies the selected note to preview
rendering so every implicit key-scale input resolves from the same MIDI note.

For multiple Voice Contexts, the transport duration is the maximum positive
`voiceDurationSeconds` in the compiled plan. An empty plan falls back to one
second.

## Complexity

- Note start/stop and preview-key selection are O(1).
- The transport timer performs O(1) work per tick.
- Maximum duration is recomputed only when a compiled plan is published and is
  O(V) in Voice Context count, not per audio block or timer tick.
- A preview-note change may request the existing bounded preview product; it
  does not compile, serialize, clone the graph, or publish a durable edit.
- Realtime modulation remains O(B) per mapped source block and reuses existing
  preallocated buffers.

## Completion Criteria

- A realtime sequence test proves two note velocities produce distinct implicit
  morph inputs and that velocity, inverse velocity, and constant configurations
  produce their specified values through an attached Mod Triple.
- Play starts the selected preview note, progresses against the maximum compiled
  Voice Context duration, stops at one, and can be toggled off early.
- Right-click selection changes the preview note without producing a MIDI
  note-on; the key highlight and automation state agree.
- Key-scale preview inputs and audible playback use the selected preview note.
- Spacebar toggles the same transport when focus is not in a text editor.
- Focused interaction/geometry tests and a native automation fixture cover the
  complete gesture.
- The marquee uses the canvas-blue selection family, and spy cable annotation
  geometry scales with graph zoom.
- Standalone Debug, focused tests, `git diff --check`, hot-loop review, and
  production-size before/after captures pass.

## Completion Evidence

- `fa50d6b4` changes the marquee to the canvas-blue selection family and scales
  spy cable annotations with graph zoom.
- `a5afa206` refreshes attached Mod Triple mappings without recompiling the
  graph and proves velocity, inverse-velocity, and constant behavior through
  the realtime renderer.
- The preview transport slice adds right-click preview-note selection, mapped
  key-scale preview positioning, the play/stop bar, maximum Voice Context
  duration, Space toggling, and focused automation coverage.
- `CycleV2` and `CycleV2_tests` build with `--parallel 10`; the keyboard,
  preview-note, attached-modulation, realtime-velocity, key-scale, voice-length,
  marquee, and spy-scaling tests pass.
- `cycle-v2-agent-preview-transport.json` and the existing performance-keyboard
  fixture pass. Visual evidence is at
  `/private/tmp/cycle-v2-preview-transport.png`; the before capture is
  `/tmp/cycle-v2-before-transport.png`.
- The full 753-case Cycle V2 suite reaches 750 passes. Its three remaining
  failures are the existing African Horn canonicalization issue, the documented
  linked-axis Trimesh expectation, and the user's in-progress Astral preset
  content; none overlap this implementation.
