# Cycle V2 Dynamic Envelope Modulation

## Status

In Progress.

Absolute red/blue inputs and synchronous, preallocated per-note morph latching
are implemented by `cycle-v2-realtime-note-on-envelope-preparation.md`. The
obsolete worker preparation exchange was removed. The
original Envelope-owned `Dynamic while live`
policy was removed on 2026-07-31 after ownership review: live adoption is a
voice traversal policy and may only be exposed through Voice Context (or a
shared voice-policy object owned by it). Voice Context now owns an opt-in
`envelopeModulation=live` contract; the default
remains note-on latching. The bounded implementation and musical acceptance are
tracked by `cycle-v2-expressive-saxophone-articulation.md`, section A.

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
- Keep live-adoption policy out of Envelope authoring state; Voice Context owns
  its per-voice live traversal policy.
- When Voice Context permits live adoption, preserve active note, sustain,
  loop, and release state when an envelope replacement is adopted.
- Keep allocation, graph mutation, and snapshot publication off the realtime
  audio thread. Both note-on and authorized block-start live materialization
  use the shared fixed-capacity realtime core; general rasterizers remain
  forbidden on the callback.
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

With the default latched policy, `effectiveMorph` is captured and materialized
synchronously at the exact note-on offset, before the first rendered sample. It
remains fixed for that note. The opt-in live policy adopts changed morphs at
subsequent block starts without restarting playback.

The original Guitar 3 G characterization attributed Cycle 1's frame-zero
`0.00553` scratch coordinate to routed key and velocity. A later source trace
showed that the mature renderer never advances those smoothed morph targets;
the value comes from its initialized `0/0` cross-section. Imported factory
graphs now preserve that legacy behavior explicitly. The bounded request for
ordinary V2 Envelope modulation is still materialized synchronously at note-on
so newly authored graphs do not begin from a stale persistent preparation.
General-purpose realtime `EnvRasterizer` use remains forbidden; the contract
uses the extracted shared core with fixed-capacity voice storage.

When Voice Context enables live adoption, a meaningful change to
`effectiveMorph` requests a new immutable prepared envelope. Once ready, the
voice adopts it at an explicitly defined audio boundary and validates its
existing playback state. The adoption does not restart the note.

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

Active-note live modulation uses a distinct runtime control path. Voice Context
serializes `envelopeModulation` as `latched` (default) or `live`; Envelope owns
no competing flag. The compiler resolves that policy for its envelope consumers.

- Each enabled active envelope samples its connected absolute red/blue controls
  at block start and prepares at most one changed morph for that block.
- Changes later in a block become eligible at the next block boundary. Exact
  note-on sample-offset preparation retains its separate existing contract.
- The shared realtime materializer stages into preallocated inactive storage.
  Only a complete successful display/cycle pair is adopted. Failure retains the
  last valid geometry and unchanged controls perform no preparation.
- A successful swap increments a voice-local preparation generation. Pitch and
  scratch consumers reconcile playback when that generation changes, even when
  immutable configuration identity stays the same.
- Consumers retain scalar loop bounds across blocks, not borrowed materializer
  views: oscillator buffering may delay consumption through several swaps.
- Shared playback preserves normalized loop phase when valid loop bounds change;
  ordinary validation handles clamping and removal of invalid loops.
- Note-off freezes the last adopted shape for release. Reset/retrigger retain
  existing lifecycle semantics and cannot resume a released note accidentally.

There is no worker exchange, pending request queue, graph edit or configuration
publication at control rate. Response is bounded by one host block plus existing
consumer sampling; partition-independent cadence is not promised. This explicitly
supersedes the older non-realtime preparation requirement, using the newer shared
fixed-capacity core rather than general-purpose `EnvRasterizer` calls.

## Audio Semantics

- Note-on latches the initial effective morph and starts one playback state.
- A Voice Context-authorized live replacement preserves elapsed envelope
  position where valid.
- If the new end precedes the cursor, validation clamps consistently.
- If valid loop bounds change, active cursors preserve normalized loop phase.
- If the loop becomes invalid, playback leaves looping through the established
  state transition.
- Note-off uses the release section of the most recently adopted generation.
- No live replacement is prepared after note-off; release retains its adopted
  geometry and cannot return to sustain.
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
- a changed live envelope performs at most one `R` per block, never per sample;
- materialization is bounded by the accepted plan capacity and playback
  reconciliation by the prepared voice count;
- audio sampling remains `O(B)` per active voice;
- traversal application remains `O(n + S)` after one required preparation;
- persistent authoring edits may rebuild graph configuration, while live
  modulation must not compile or copy the graph;
- no allocation, serialization, UI publication, mutex wait, unbounded retry or
  general rasterization occurs on the realtime thread. Note-on and authorized
  block-start replacement both use preallocated shared materialization.

## Semantic Tests

### Shared Preparation

- The same mesh snapshot and effective morph produce equivalent sampled values
  for audio and traversal consumers.
- Red and blue changes select the expected mature mesh cross-section; no node-
  local interpolation is introduced.
- Prepared generations are immutable and remain valid while a newer generation
  is prepared and adopted.

### Current Latched Policy

- Modulation before note-on affects the latched envelope.
- Modulation during an active note does not change its sampled envelope.
- A later note uses the newest effective morph.
- No preparation request is emitted solely because a disabled live value
  changes.

### Voice Context Live-Adoption Policy

- A routed red or blue change during sustain eventually changes the active
  envelope without restarting it.
- Cursor position advances monotonically across adoption when no clamp or wrap
  is required.
- Changed loop bounds wrap the cursor correctly.
- Removing a valid loop follows the established non-looping transition.
- Note-off after adoption uses the adopted generation's release curve while
  beginning from the current output level according to mature semantics.
- Red and blue modulation are isolated per voice.
- Within-block changes are deferred; tests prove no early adoption and at most
  one changed-morph preparation per envelope per block.

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

- Instrumented unchanged rendering performs no preparation. Changed live
  rendering performs bounded preparation without allocation, locks,
  serialization, graph edits or snapshot publication.
- Instrumented initial note-on preparation satisfies
  `cycle-v2-realtime-note-on-envelope-preparation.md`.
- Accepted capacities and eight-voice compact/complex fixtures bound and measure
  repeated preparation work; no request queue exists.

## Migration

1. Characterize Cycle v1 dynamic pitch and scratch behavior, including loop and
   release reconciliation. Record the volume-path omission as non-authoritative.
2. Give the prepared envelope a typed request/generation identity suitable for
   stale-result rejection and optional sharing.
3. Extract or formalize the playback consumer and traversal consumer around the
   common prepared representation.
4. Remove the historical Envelope-owned dynamic policy and legacy serialized
   parameter; define live-adoption control on Voice Context.
5. Define absolute per-voice effective-morph `ControlSignal` inputs supplied by
   graph edges, without coupling them to graph mutations.
6. Replace note-start exchange/adoption with the bounded synchronous path in
   `cycle-v2-realtime-note-on-envelope-preparation.md`. Reserve non-realtime
   authoring preparation; live replacement uses the same fixed-capacity core.
7. Add the audio lifecycle, traversal, coalescing, and realtime-boundary tests.
8. Route ordinary node-canvas control sources directly to the red/blue inputs;
   keep scaling and combination in upstream control nodes.

## Completion Criteria

- Red/blue modulation is latched per note without mutating the graph.
- Live adoption policy is owned and serialized by Voice Context rather than
  Envelope; existing documents default to latching.
- Audio and graphic consumers use one authoritative envelope preparation and
  sampling implementation.
- Their lifecycle and optimization policies remain separate and domain-shaped.
- Voice Context-authorized live adoption preserves or validly
  reconciles note, loop, sustain, and release state.
- Initial and Voice Context-authorized live materialization satisfy their
  fixed-capacity realtime contracts without publishing persistent state.
- Modulation bursts cannot create an unbounded work queue.
- Tests prove product semantics, not merely that callbacks or revisions occur.
