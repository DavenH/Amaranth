# Cycle V2 Unison Parity

## Status

In progress.

## Intent

Add Unison as a first-class Cycle V2 node with an effect-style compact and
expanded presentation. Preserve Cycle 1's group-mode voice layout exactly and
show each configured voice's phase drift over the global preview duration at
the preview note.

The display is explanatory, not decorative. A zero-phase, zero-detune voice is
horizontal at zero. Positive and negative detunes slope in opposite directions,
and every path wraps at the displayed one-cycle boundaries. Phase, pitch,
detune, and duration must affect the display through the same calculation used
to describe oscillator execution.

## Authoritative Implementations

- Cycle 1 group voice generation, parameter meanings, and jitter table:
  `cycle/src/Audio/Effects/Unison.*`.
- Cycle 1 oscillator tuning:
  `cycle/src/Audio/Voices/CycleBasedVoice::getAngleDelta`.
- Cycle 1 per-unison phase, pan, accumulation, and gain:
  `cycle/src/Audio/Voices/SynthUnisonVoice.*` and
  `cycle/src/Audio/Voices/SynthesizerVoice::getEffectiveLevel`.
- Cycle 1 graphic pitch/unison traversal:
  `cycle/src/UI/VisualDsp/UnisonPhaseColumnRenderer.h`.
- Shared application-neutral DSP location:
  `lib/src/Audio/CycleDsp`.
- Cycle V2 node schema, configuration publication, effect presentation, and
  editor hosting:
  `cycle-v2/src/Graph/NodeDefinition.cpp`,
  `cycle-v2/src/Runtime/NodeDspConfiguration.*`,
  `cycle-v2/src/Nodes/Effects/EffectPreviewRenderer.*`, and
  `cycle-v2/src/UI/ConcreteNodeEditors.cpp`.

The Cycle 1 algorithms above are authoritative. This work extracts their
application-neutral voice-layout and phase-drift calculations; it does not copy
or approximate them in Cycle V2 UI code.

## Shared Core

`CycleDsp::UnisonCore` owns:

- the maximum order and exact legacy jitter table;
- normalized order mapping;
- group-mode detune, phase, and pan generation;
- individual-mode detune-position mapping;
- the per-voice level compensation used by Cycle 1;
- frequency and signed wrapped phase-drift calculations for a MIDI note,
  detune in cents, initial phase, and elapsed seconds.

The group-layout core is immutable and allocation-free. Preview path splitting
returns bounded geometry outside the audio thread. The core owns no UI,
singleton, pending action, graph, or audio-thread state.

Cycle 1 keeps its existing `Unison` class as the UI/action/thread adapter.
`Unison` translates its parameter storage into shared-core inputs and copies
the calculated voice values into its existing audio and graphic snapshots.
That adapter's stable end state is narrow parameter/action translation; the
legacy jitter table and group-layout formulas are deleted from it.

Cycle V2 publishes an immutable unison configuration built from graph
parameters. Presentation and future oscillator execution consume that
configuration. Cycle V2 must not contain a second jitter table or local
detune/phase/pan formula.

## Node Semantics

Unison is voice-domain configuration, not a post-mix time-domain effect.
Placing it in the FX palette and giving it an effect-style editor does not
change that signal contract.

The earlier signal-chain shape:

```text
Voice Context -> Unison -> voice-aware source/processor
```

is superseded by `cycle-v2-voice-context-attachments.md`. Unison has no signal
input and publishes one typed immutable configuration attachment:

```text
Unison --configuration--> Voice Context --context--> voice-aware sources
```

Voice Context folds the configuration containing order and per-voice detune,
phase, pan, and gain into its compiled voice plan. A future compiled voice-lane
step follows `cycle-v2-oscillator-region-compilation.md`: time-only regions
execute their cycle recipe independently per Unison lane, while spectral
regions calculate one shared fixed cycle frame and reconstruct it independently
per lane. Each oscillator region folds its lanes with the shared level
compensation before ordinary block Add or Multiply. The current
`DomainContext` passthrough ports and processor are transitional deletion
targets, not the stable interface.

The current Cycle V2 Wave source is a placeholder ramp, and `VoiceContext`
currently supplies ordering without an executable oscillator context.
Therefore a post-buffer chorus/delay approximation is forbidden. Until the
shared oscillator/voice-lane substrate consumes `UnisonCore` configuration,
the node may publish, serialize, edit, preview, and validate the authoritative
configuration but audible Cycle V2 parity remains incomplete.

## Parameters

The first Cycle V2 node exposes Cycle 1 group mode:

- `enabled`: bypasses to one centred, zero-phase, zero-detune voice;
- `order`: integer 1 through 10;
- `width`: maximum detune, 0 through 70 cents;
- `panSpread`: Cycle 1 pan scale, 0 through 100%;
- `phase`: Cycle 1 phase scale, 0 through one cycle;
- `jitter`: Cycle 1 detune/phase jitter scale, 0 through 100%.

Serialized IDs are stable. Individual voice mode is a later slice because it
requires structured per-voice node state and add/remove/selection interaction;
it must reuse the same core rather than extend a generic effect adapter with
voice-kind branches.

## Preview Contract

The global preview context owns:

- preview MIDI note, default 60;
- voice duration in seconds, default 1 second.

These values are not serialized as Unison parameters. Compact and expanded
Unison previews receive them from presentation state. Until the application
exposes user controls for that global state, the defaults are explicit and
testable rather than hidden renderer constants.

For voice `i`, the unwrapped relative phase is:

```text
phase_i(t) = initialPhase_i
           + t * (frequency(note, detune_i) - frequency(note, 0))
```

The renderer maps the signed wrapped result to `[-0.5, 0.5)` cycles and splits
the path exactly at wrap crossings. The horizontal axis is
`[0, voiceDuration]`. The vertical axis represents one oscillator cycle, with
zero centred. Pitch and duration change the visible number of wraps; width,
jitter, and order change the generated voice paths; phase changes their
starting offsets.

No per-pixel DSP formula belongs in paint code. The renderer consumes shared
voice values and shared trajectory slope/wrap helpers.

## Threading And Lifecycle

- Group-layout calculation is bounded by ten voices and allocation-free.
- Cycle 1 continues publishing graphic and audio snapshots through its existing
  pending-action boundary.
- Cycle V2 configuration construction occurs during non-realtime graph
  publication/preparation.
- Paint reads an immutable configuration and performs bounded path geometry
  only.
- Future voice-lane audio state is per synth voice and per unison lane.
  Authoring configuration remains immutable and contains no oscillator phase
  history.

## Negative Boundaries

- Do not implement Unison as a chorus over an already mixed time buffer.
- Do not model Unison as a `DomainContext` signal transform or passthrough
  processor; it is a typed Voice Context configuration attachment.
- Do not copy Cycle 1's jitter table or group-layout formulas into Cycle V2.
- Do not make preview note or voice duration node parameters.
- Do not mutate oscillator/audio history from preview traversal.
- Do not add Unison-specific behavior to `NodeCanvas`.
- Do not claim audible parity while Wave source and voice-lane execution remain
  placeholders.
- Do not add individual-mode scaffolding without structured per-voice state and
  complete add/remove/edit/persistence interaction.

## Implementation Slices

1. Extract and characterize `UnisonCore`; migrate Cycle 1 group calculations.
2. Add the Cycle V2 Unison node schema, immutable configuration, palette/icon
   registration, persistence, and transitional graph/runtime contracts.
3. Add compact and expanded phase-path displays and group-mode controls using
   the shared core.
4. Add focused numerical, serialization, editor, raster, and automation tests.
5. Implement the typed Voice Context attachment contract and delete Unison's
   transitional `DomainContext` ports and passthrough runtime step.
6. Implement the time-only and spectral oscillator-region strategies in
   `cycle-v2-oscillator-region-compilation.md`, make voice-aware sources consume
   Unison configuration, and prove Cycle 1/Cycle 2 audible parity.
7. Add structured individual-mode voice state and interaction parity.

## Verification

- Shared-core golden tests cover every order, the exact jittered voice values,
  width/pan/phase/jitter endpoints, bypass, level compensation, positive and
  negative drift, pitch scaling, duration scaling, and exact wrap crossings.
- Cycle 1 adapter tests or existing semantic tests prove its public detune,
  phase, pan, and order behavior is unchanged.
- Node-definition, codec, compiler, configuration, registry, palette, and
  editor tests cover the new kind without broad generic fallbacks.
- Pixel or path-level tests prove zero detune is horizontal, opposite detunes
  slope oppositely, wraps have no vertical bridge, bypass is greyscale, and
  pitch/duration changes are observable.
- Focused automation covers add, connect, open, edit, bypass, save, reopen, and
  preview publication.
- Modified hot paths contain no scalar `std::<math>` work in per-sample,
  per-bin, or per-pixel loops where `Buffer`/`VecOps` applies.
- Standalone Debug and focused Catch2 tests pass; UI capture logs contain no
  assertion, crash, or suspicious runtime failure.

## Implementation Evidence

Slices 1 through 4 are implemented for group mode:

- commit `9278d180` extracted the exact Cycle 1 jitter table, group layout,
  parameter mappings, individual detune mapping, level compensation, and phase
  trajectory into `CycleDsp::UnisonCore`; Cycle 1 consumes that shared core;
- Cycle V2 has a serializable `Unison` node, immutable configuration, explicit
  voice-domain ports and runtime role, palette/icon registration, compact
  preview, and hosted effect-style editor;
- compact and expanded previews render analytically split phase paths from the
  shared configuration at the supplied preview MIDI note and voice duration;
- focused Catch2 coverage exercises shared golden values, wrap boundaries,
  pitch/duration scaling, node compilation, configuration publication, effect
  rendering, registry coverage, and editor registration;
- `cycle-v2-agent-unison-editor.json` covers add, edit, bypass, save, reopen,
  and editor persistence. The OS-level capture
  `/private/tmp/cycle-v2-unison-editor-os.png` verifies the OpenGL presentation.

The focused automation completed successfully. Its filtered launch log
contained only the pre-existing JUCE Settings assertions already recorded in
`docs/TDD/ui-bugs.md`; it contained no Unison-specific assertion, crash, or
suspicious runtime failure.

Slice 5 and slice 7 are implemented:

- Unison is a typed Voice Context configuration attachment with no transitional
  signal ports, signal buffer, or passthrough runtime step;
- individual mode has structured per-voice detune, pan, and phase state with
  add/remove/select/edit, serialization, preview, and one-gesture undo;
- group and individual preview trajectories communicate resolved pan using an
  orange-left, greyscale-centre, purple-right colour mapping;
- the expanded group editor keeps every control, including Jitter, within the
  hosted panel at its registered production size.

Slice 6 remains in progress. Explicit oscillator regions now compile and the
Cycle 1 fractional lane clock plus mature chained-rasterizer transition have
been extracted into shared Cycle DSP primitives consumed by Cycle 1. The first
complete runtime lowering executes a direct time-only Trimesh region through
the mature rasterizer independently for every configured lane, preserves
split-block continuity and low-note cycle capacity, and folds the result with
the shared pan and level contracts. Multi-operation time regions, shared
spectral-frame reconstruction, independent oscillator-region materialization,
and lifecycle/latency parity remain open, so full audible parity is not yet
claimed.

## Completion Criteria

- Cycle 1 and Cycle 2 use one authoritative unison layout/phase core.
- Cycle 2 has a first-class, serializable Unison node with truthful compact and
  expanded phase-path presentation.
- Global preview note and duration drive the visualization.
- Group-mode controls preserve Cycle 1 ranges, defaults, mappings, and output.
- The compiled Cycle 2 voice lane audibly renders and sums the configured
  unison voices with Cycle 1 parity.
- Structured individual-mode voices preserve Cycle 1 edit and persistence
  semantics.
- No approximation or duplicate mature behavior remains.
