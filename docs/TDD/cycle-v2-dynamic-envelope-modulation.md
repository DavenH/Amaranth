# Cycle V2 Dynamic Envelope Modulation

## Status

Implemented.

Depends on the implemented preparation slices of
`envelope-renderer-playback-separation.md` and
`envelope-grid-time-application.md`.

## Problem

Cycle v2 envelope red and blue morph positions are persistent node parameters.
Changing either parameter builds and publishes a complete immutable DSP
configuration. The audio processor can adopt that configuration without
resetting its playback state, and `EnvRasterizer::validateState()` reconciles
the active cursor and loop with the replacement envelope.

This is sufficient for authoring edits, but it is not yet a live modulation
contract. Routing a changing modulation source through graph parameter edits
would conflate persistent authoring state with per-voice runtime state, create
graph revisions at control rate, and potentially queue expensive envelope
preparations faster than they can be consumed.

Cycle v1 exposes an opt-in `Dynamic while live` property. With it disabled,
the effective red/blue morph position is latched for the note. With it enabled,
pitch and scratch envelopes are rerasterized when routed modulation changes,
then their existing playback state is validated against the replacement. The
Cycle v1 volume-envelope path omits that rerasterization and must not be copied
as intentional behavior without independent justification.

Audio playback and graphic traversal need the same authoritative envelope
geometry and sampling semantics, but they consume them differently:

- audio has per-voice note, loop, sustain, release, cursor, continuity, and
  realtime-publication constraints;
- a traversal grid has the complete time-axis rollout available, can sample a
  prepared envelope once at `i / n`, and does not need to simulate audio cursor
  transitions or preserve cycle-boundary waveform continuity.

Forcing both products through either consumer's lifecycle would discard useful
domain-specific optimizations. Implementing two evaluators would allow their
curve, morph, loop, and scaling semantics to diverge.

## Goals

- Preserve one authoritative envelope preparation and sampling core.
- Separate persistent base controls from per-voice effective modulation.
- Make `Dynamic while live` an explicit envelope policy, defaulting to off.
- Preserve active note, sustain, loop, and release state when a dynamic
  envelope replacement is adopted.
- Keep envelope preparation, allocation, graph mutation, and snapshot
  publication off the realtime audio thread.
- Let traversal rendering exploit its complete time-axis domain without
  acquiring audio playback machinery.
- Coalesce live modulation requests so preparation work is bounded.
- Define observable update cadence and latency rather than inheriting it from
  UI callbacks or graph recompilation timing.

## Non-Goals

- Per-sample rerasterization of envelope geometry.
- Treating a modulation event as a persistent graph edit.
- Reusing an audio voice cursor to render a traversal grid.
- Reimplementing curve interpolation, morph slicing, loop discovery, or
  logarithmic scaling in a node processor, modulation adapter, or preview.
- Reproducing Cycle v1's inconsistent volume-envelope omission.

## Semantic Model

An envelope has persistent authoring controls and runtime modulation:

```text
baseMorph       = (node.red, node.blue)
routedMorph     = connected per-voice ControlSignal values
effectiveMorph  = connected(routedMorph) ? constrain(routedMorph) : baseMorph
```

Red and blue inputs are absolute effective morph positions in `[0, 1]`, not
offsets applied to the persistent parameters. An unconnected axis falls back
to its persistent node parameter independently of the other axis. Scaling,
offset, polarity, depth, or combination belongs in upstream control-signal
nodes. Cycle v2 deliberately has no separate modulation-matrix UI or hidden
modulation routing subsystem.

Envelope nodes expose top-side red and blue `ControlSignal` inputs. Trilinear
Mesh nodes expose top-side yellow, red, and blue `ControlSignal` inputs under
the same absolute-position contract. Canonical normalization adds these ports
to previously saved Cycle v2 nodes without a separate migration layer.

When dynamic playback is disabled, `effectiveMorph` is latched at note-on and
remains fixed for that note. Changes remain available to the next note.

When dynamic playback is enabled, a meaningful change to `effectiveMorph`
requests a new immutable prepared envelope. Once ready, the voice adopts it at
an explicitly defined audio boundary and validates its existing playback state.
The adoption does not restart the note.

## Target Design

Separate shared domain products from consumer policy:

```text
EnvelopeMeshSnapshot + EnvelopeRenderRequest(effectiveMorph)
        -> EnvelopePreparer
        -> PreparedEnvelope
             |                         |
             v                         v
   EnvelopePlaybackEngine       EnvelopeTraversalSampler
   per-voice lifecycle          full-axis i / n sampling
   cursor/loop/release           grid application/preview
```

`EnvelopePreparer` and `PreparedEnvelope` are the shared processing core. They
own the authoritative morph cross-section, curves, intercepts, waveform, loop
metadata, and sampling representation. They do not own graph revisions, UI
publication, voice cursors, or traversal-grid layout.

`EnvelopePlaybackEngine` owns per-voice lifecycle and continuity policy. It
adopts a complete `PreparedEnvelope`, retaining its current temporal state and
using the existing validation behavior to clamp or wrap state when boundaries
change.

`EnvelopeTraversalSampler` samples the prepared representation directly at
the requested grid positions. For `n` time columns it samples `f(i / n)` once
per column and applies that scalar down the column. It does not render one mesh
cross-section per time column and does not step an audio playback cursor.

## Control And Publication Boundaries

Persistent UI edits continue through the conflict-checked graph mutation and
immutable node-configuration publication path.

Live modulation uses a distinct runtime control path with these properties:

- values are scoped per voice where the source is per voice;
- modulation does not alter serialized node parameters;
- the audio thread may publish or record a small latest-value request, but may
  not build an envelope or allocate;
- a non-realtime preparation owner reads the newest request and builds a
  complete immutable result;
- superseded pending requests may be discarded before preparation;
- at most one prepared replacement and one newest pending request are retained
  per independently modulated envelope/voice;
- the audio side adopts only complete generations at the selected safe
  boundary;
- stale results carry enough request identity to be rejected.

The implementation must state whether adoption occurs at audio-block, waveform-
cycle, or another boundary. Cycle-boundary adoption is preferred where changing
the prepared envelope mid-cycle would create an avoidable discontinuity.

## Dynamic Policy

Add a typed envelope policy equivalent to `Dynamic while live`, default false.
It belongs to the envelope node/domain configuration, not to the modulation
matrix adapter.

The disabled path must not continuously prepare unused envelopes. The enabled
path should apply a documented change threshold or quantization only if needed
to bound work; any such policy must be expressed in domain units and tested.
Silently inheriting GUI slider precision is not acceptable.

Turning dynamic mode on during an active note permits the next effective-morph
change to prepare and adopt a replacement. Turning it off freezes the most
recently adopted envelope for existing notes; it must not implicitly restart
or jump them back to the persistent base morph.

## Audio Semantics

- Note-on latches the initial effective morph and starts one playback state.
- A dynamic replacement preserves elapsed envelope position where valid.
- If the new end precedes the cursor, validation clamps consistently.
- If loop bounds change, an active loop cursor is wrapped into the new loop.
- If the loop becomes invalid, playback leaves looping through the established
  state transition.
- Note-off uses the release section of the most recently adopted generation.
- A replacement prepared before note-off but adopted after it must obey an
  explicit stale-generation rule; it may not restore sustain or restart the
  note.
- Each voice may have a different effective morph and prepared generation.
- Global modulation may share a prepared generation only when effective morph,
  mesh snapshot, and envelope policy are identical.

## Graphic And Traversal Semantics

The graphic consumer must state what it is visualizing:

- the persistent base envelope;
- a selected voice's current effective envelope; or
- an explicitly supplied audition morph.

It must not accidentally display whichever per-voice result was published
last. A selected live generation may reuse the same `PreparedEnvelope` as audio
when ownership and thread boundaries permit. Otherwise it must prepare the same
request through the shared `EnvelopePreparer`.

Traversal grids exploit their full x-axis rollout:

```text
t(i) = i / n
column(i) *= preparedEnvelope.sample(t(i))
```

They do not model note-on, note-off, cursor wrapping, or cycle-boundary adoption
unless the visualization explicitly requests an audio-lifecycle trace. Loop
and release metadata may be drawn from the prepared result without running the
playback state machine.

## Realtime And Complexity Contracts

Let `R` be one envelope preparation, `B` an audio block, `V` the number of
voices whose effective morph actually changes, `n` traversal columns, and `S`
the total traversal samples.

- unchanged audio processing performs no `R`;
- a coalesced dynamic update performs at most one useful `R` per distinct
  effective request eventually adopted, not one `R` per sample;
- realtime adoption is bounded independently of waveform/intercept vector
  length, apart from selecting an already owned immutable result;
- audio sampling remains `O(B)` per active voice;
- traversal application remains `O(n + S)` after one required preparation;
- persistent authoring edits may rebuild graph configuration, while live
  modulation must not compile or copy the graph;
- no preparation, allocation, serialization, UI snapshot publication, mutex
  wait, or unbounded retry occurs on the realtime thread.

## Semantic Tests

### Shared Preparation

- The same mesh snapshot and effective morph produce equivalent sampled values
  for audio and traversal consumers.
- Red and blue changes select the expected mature mesh cross-section; no node-
  local interpolation is introduced.
- Prepared generations are immutable and remain valid while a newer generation
  is prepared and adopted.

### Dynamic Disabled

- Modulation before note-on affects the latched envelope.
- Modulation during an active note does not change its sampled envelope.
- A later note uses the newest effective morph.
- No preparation request is emitted solely because a disabled live value
  changes.

### Dynamic Enabled

- A routed red or blue change during sustain eventually changes the active
  envelope without restarting it.
- Cursor position advances monotonically across adoption when no clamp or wrap
  is required.
- Changed loop bounds wrap the cursor correctly.
- Removing a valid loop follows the established non-looping transition.
- Note-off after adoption uses the adopted generation's release curve while
  beginning from the current output level according to mature semantics.
- Red and blue modulation are isolated per voice.
- Rapid changes are coalesced; a deterministic test proves bounded pending
  work and rejection of stale prepared generations.

### Audio/Graphic Separation

- A traversal grid matches independent `f(i / n)` samples from the shared
  prepared envelope.
- Traversal generation performs one preparation and does not instantiate or
  advance an audio playback cursor.
- Row count does not affect a column's envelope value.
- A graphic base-envelope view does not nondeterministically switch between
  active voices.
- An explicitly selected live-voice view identifies and displays the requested
  prepared generation.

### Realtime Boundary

- Instrumented audio processing reports no envelope preparation, allocation,
  serialization, graph edit, or snapshot publication.
- Adoption work remains bounded as prepared-envelope size changes.
- The producer can run slower than incoming modulation without growing a
  queue; the newest request ultimately wins.

## Migration

1. Characterize Cycle v1 dynamic pitch and scratch behavior, including loop and
   release reconciliation. Record the volume-path omission as non-authoritative.
2. Give the prepared envelope a typed request/generation identity suitable for
   stale-result rejection and optional sharing.
3. Extract or formalize the playback consumer and traversal consumer around the
   common prepared representation.
4. Add the envelope dynamic policy and persistent serialization/UI control.
5. Define absolute per-voice effective-morph `ControlSignal` inputs supplied by
   graph edges, without coupling them to graph mutations.
6. Add bounded non-realtime preparation and safe generation adoption.
7. Add the audio lifecycle, traversal, coalescing, and realtime-boundary tests.
8. Route ordinary node-canvas control sources directly to the red/blue inputs;
   keep scaling and combination in upstream control nodes.

## Completion Criteria

- Red/blue modulation works during playback according to the explicit dynamic
  policy and does not mutate the graph.
- Audio and graphic consumers use one authoritative envelope preparation and
  sampling implementation.
- Their lifecycle and optimization policies remain separate and domain-shaped.
- Dynamic adoption preserves or validly reconciles note, loop, sustain, and
  release state.
- Preparation and publication are absent from the realtime audio thread.
- Modulation bursts cannot create an unbounded work queue.
- Tests prove product semantics, not merely that callbacks or revisions occur.
