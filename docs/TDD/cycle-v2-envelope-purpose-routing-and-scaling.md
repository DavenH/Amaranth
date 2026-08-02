# Cycle V2 Envelope Purpose, Routing, And Scaling

## Status

In Progress.

Purpose persistence and migration, typed connection kinds, purpose-aware
presentation and polarity, logarithmic DSP/preview/spy parity, and compiled
scratch attachments are implemented in `4ad62d36` and `5b5e6b9d`. Scratch
attachments now change every attached Stengah Trimesh in block audio,
traversal previews, and spies without realtime graph lookup or allocation.

Three completion boundaries remain and must not be filled with local
approximations:

- Voice Context still has no typed pitch input or pitch-aware runtime. Pitch
  Envelope routing therefore remains truthfully rejected as specified below.
- Trimesh block rendering still selects one morph coordinate for a whole
  processing block. Sample-accurate scratch traversal depends on the
  pitch-clocked source scheduling owned by
  `cycle-v2-oscillator-region-compilation.md`.
- The expanded Trimesh editor is an authoring view with no published runtime
  preview input. Compact previews and spies consume the compiled scratch
  traversal today; displaying that same traversal in the expanded editor
  requires a shared runtime-preview boundary rather than a second editor-side
  evaluator.

The pitch connection slice depends on the Voice Context pitch port being
merged from its current branch. Purpose state, presentation, logarithmic DSP,
and scratch execution can be implemented and tested before that merge. Until
the port exists, pitch mode must serialize and render truthfully but reject all
connections; it must not temporarily masquerade as a control or scratch
envelope.

This document is the authoritative Cycle V2 contract for Envelope purpose,
output grammar, polarity, logarithmic scaling, and scratch traversal. The
broader Voice Context and layer-routing TDDs refer to this contract rather than
defining competing subsets of Envelope behavior.

## Problem

Cycle V2 currently exposes every Envelope through one generic
`EnvelopeSignal` output. The node has no visible purpose selector, so its
connection grammar cannot distinguish volume, generic control, pitch, and
scratch behavior. Pitch is consequently drawn as unipolar, while scratch
attachments are present in imported graphs but do not affect their target
Trimesh traversal.

The `logarithmic` parameter is serialized and supplied to
`EnvelopePlaybackEngine`, but this is not reflected consistently in all
observable products. In particular, runtime spies and traversal-grid previews
must show the same transformed values as audio. A control that changes only
the editor background is not implemented DSP behavior.

These are one semantic problem: Envelope purpose determines the meaning of its
values, legal connection kind and destination, visual range, and applicable
scaling policy. Node IDs, titles, imported preset structure, and downstream
guessing must not supply that meaning.

## Authoritative Implementations

- Envelope geometry, preparation, loop/sustain/release state, and sample
  playback remain in `EnvelopeMesh`, `EnvRasterizer`, and
  `lib/src/Curve/Rasterization/EnvelopePlaybackEngine.*`, as separated by
  `envelope-renderer-playback-separation.md` and
  `cycle-v2-dynamic-envelope-modulation.md`.
- Cycle 1 logarithmic audio behavior is
  `EnvelopePlaybackEngine::renderToBuffer`: for ordinary sample-rate envelope
  playback it applies `Arithmetic::applyInvLogMapping(buffer, 30)`. Cycle 1's
  volume visualization applies that same transform in
  `cycle/src/UI/VisualDsp.cpp`; `Envelope2D::updateBackground` uses the inverse
  display grid through `Arithmetic::logMapping`.
- Cycle 1 pitch interpretation is
  `NumberUtils::unitPitchToSemis`, consumed by
  `CycleBasedVoice::getAngleDelta`. A normalized value of `0.5` is neutral,
  and `[0, 1]` represents `[-12, +12]` semitones. Cycle 1 constrains live
  tuning samples to `[0.01, 0.99]`; the exact endpoints remain visually
  undefined rather than becoming special tuning values.
- Cycle 1 Envelope vertical framing is `Envelope2D::zoomAndRepaint`, which
  delegates to the shared `Panel2D::contractToRange` and
  `ZoomPanel::contractToRange` implementation. The Envelope controls expose
  both contract-to-range and expand-to-full actions with atlas cells `(6, 0)`
  and `(6, 1)` from `Images::icons_png`. Cycle V2 reuses both those exact
  glyphs and that fitter rather than introducing replacement iconography or
  pitch-specific range arithmetic.
- Cycle 1 scratch lifecycle and per-voice coordinate production are
  `CycleBasedVoice::updateEnvelopes` and `getScratchTime`.
  `cycle/src/Curve/Rasterization/Policies/Graphic/GraphicPolicies.h`,
  `lib/src/Curve/Rasterization/Rasterizer/TimeColumnRasterizer.cpp`, and
  `cycle/src/UI/VisualDsp.cpp` are authoritative for applying the selected
  scratch value as a source layer's traversal coordinate.
- Cycle V2 graph ports and validation remain orchestration in
  `cycle-v2/src/Graph/NodeDefinition.cpp` and `GraphValidator.cpp`; immutable
  preparation and processing remain under `cycle-v2/src/Nodes/Envelope/` and
  the existing Trimesh runtime/preview processors.

The mature interpolation, envelope state machine, logarithmic transform,
pitch mapping, and scratch-coordinate behavior are reused unchanged. If the
scratch application rule cannot be called from both Cycle 1 and Cycle 2
without copying it, extract an application-neutral core below both callers
before implementing Cycle V2 execution.

## Persistent Purpose

Envelope adds one persistent choice named `purpose` with four stable JSON
values:

| Purpose | Output semantic | Connection kind | Legal destinations | Display range |
| --- | --- | --- | --- | --- |
| `control` | Generic `ControlSignal` | Signal | Ordinary compatible control inputs | Unipolar |
| `volume` | Lifecycle-aware `EnvelopeSignal` | Signal | Envelope-aware gain/factor inputs | Unipolar |
| `pitch` | `PitchSignal` | Signal | Voice Context `pitch` only | Bipolar about neutral |
| `scratch` | Scratch traversal source | Processing attachment | Scratch-capable source-layer targets | Unipolar |

`volume` is the compatibility default for an existing Envelope whose output
feeds an envelope-aware Multiply/factor path. Existing scratch attachments
migrate to `scratch`. Unconnected legacy pitch Envelope instances imported by
the preset converter migrate from their Cycle 1 group metadata to `pitch`.
Other unconnected legacy Envelope nodes migrate to `control`. Migration is
canonical and idempotent; it does not infer purpose from an instance name.

Purpose belongs to the typed Envelope node definition/model and is immutable
for one published graph revision. Display name remains the immutable node-type
name `Envelope`; purpose is a selectable semantic property shown as a badge or
subtitle, not a custom node name.

Changing purpose is one undoable graph edit. The edit changes the output
descriptor and atomically removes every now-incompatible edge, reporting the
removed edges to UI automation. It does not reinterpret a surviving cable as
a different connection kind. Undo restores both purpose and removed edges.

## Purpose-Dependent Connection Grammar

Port compatibility is resolved from typed purpose metadata, not port IDs,
labels, node kinds at the validator call site, or cable colour. The stable
connection representation distinguishes at least:

```cpp
enum class ConnectionKind {
    Signal,
    ConfigurationAttachment,
    ProcessingAttachment
};
```

The current `bool attachment` may be decoded for old files, but it is a
transitional representation with an explicit deletion target. Scratch uses a
`ProcessingAttachment`: it binds one prepared, per-voice scratch trajectory to
the traversal input of each explicitly attached Trimesh/source layer. It does
not become an ordinary amplitude/control buffer, and no target discovers it by
graph proximity.

Pitch uses an ordinary typed signal edge because it is evaluated per voice by
Voice Context. Once the other branch supplies `VoiceContext.pitch`, a
pitch-purpose Envelope can connect to that port and no other destination.
Before the merge, validation reports the missing legal consumer. The eventual
merge must add this grammar directly rather than a temporary generic-control
adapter.

Volume and control remain distinct even though both produce sample values.
Volume retains envelope traversal metadata needed by envelope-aware arithmetic
and gain consumers. Control is the generic control domain used by morph and
other compatible parameter inputs. An explicit conversion node is required
where those domains intentionally cross; the compiler must not make every
`ControlSignal` and `EnvelopeSignal` interchangeable.

## Scratch Execution

An attached scratch Envelope changes the attached Trimesh traversal in audio
and preview. For each active synth voice:

1. Prepare and advance the scratch Envelope with the shared Envelope playback
   core and the note's lifecycle events.
2. Clamp its sampled result to `[0, 1]`, matching Cycle 1.
3. Supply that value as the target layer's selected traversal coordinate under
   the authoritative Cycle 1 group/axis rule.
4. Rasterize/sample the Trimesh with that coordinate instead of the fallback
   voice time for the affected traversal dimension.

One scratch Envelope may attach to multiple compatible targets. Its immutable
prepared geometry is shared, and one per-voice playback state produces a
single coordinate observed by every target at a given sample/frame. Attaching
the same edge more than once is rejected or canonicalized; it must not multiply
the effect or create duplicate UI cables.

The compiler resolves processing attachments into immutable target bindings
off the audio thread. Realtime Trimesh processing receives an already-resolved
scratch view or coordinate block. It performs no graph search, node lookup,
allocation, serialization, lock acquisition, or envelope preparation.

Graphic Trimesh rasterization consumes the same prepared scratch trajectory
and coordinate-selection rule as audio. A compact preview, expanded editor,
and spy must not each invent their own scratch evaluation. With the attachment
removed or the scratch Envelope inactive/unavailable, the target returns to
its existing voice-time traversal.

## Polarity And Presentation

The expanded editor exposes a visible, strictly snapped four-stop icon
selector labeled `Mode`: `Control / Volume / Pitch / Scratch`. It reuses the
same dedicated icon renderer as compact Envelope nodes. Each option has a
distinct silhouette and accessible name. The four options form one continuous
segmented control with a single outer boundary and internal separators; the
icons have no individual outlines. The selected option receives a highlighted
cell background, so the selection reads as one viewing lens moving over the
available modes rather than four unrelated buttons. Interior highlights have
square corners; only a selected end cell inherits the corresponding outer
corners of the group. Icon artwork is scaled to 85% of its established in-cell
bounds without shrinking its interaction target.
Compact presentation shows the current purpose as text plus an icon or badge;
colour alone is insufficient. Output
port domain, connection affordance, cable style, subtitle, tooltip, and
accessibility label update from the same purpose state.

Mode selection is an inline button interaction, not a popup menu. Hovering an
option repaints only that lightweight button and must not publish graph state,
refresh the curve rasterizer, request an OpenGL panel repaint, or invalidate
the node canvas. Selecting a different option remains one discrete undoable
graph edit through the existing purpose transaction.

Envelope actions are grouped by ownership and interaction scope. Loop and
Sustain are vertex-marker toggles in one group labeled `Vertex`; they reuse
Cycle v1 atlas cells `(4, 3)` and `(5, 3)` rather than text buttons. Both are
disabled until the panel has exactly one selected Envelope vertex. Their
disabled tooltips explain that a vertex must first be selected, while their
enabled tooltips describe toggling that selected vertex as the loop start or
sustain point. Selection changes refresh this transient UI state through the
Envelope panel/controller contract and do not introduce an editor-side copy of
selection semantics.

Logarithmic scaling is an Envelope-wide property and appears as its own
control, outside the vertex-marker group. Fit-to-curve and full-range actions
form a separate two-icon framing group using their existing Cycle v1 atlas
icons. The three groups have distinct visual boundaries and do not imply that
Log or vertical framing operates on the selected vertex.

Purpose, not the current downstream cable, determines the editor range and
shading:

- control, volume, and scratch show unipolar `[0, 1]` shading;
- pitch shows bipolar meaning with a strong neutral line at normalized `0.5`,
  labels below/above it as negative/positive pitch, and reports values through
  the shared `[-12, +12]` semitone mapping;
- changing purpose changes interpretation and presentation without rewriting
  or renormalizing authored Envelope vertices.

Entering pitch mode vertically fits the rendered curve once with the shared
Cycle 1 range fitter so small deviations around neutral remain comfortable to
edit. The expanded editor's explicit fit/full actions use the Cycle 1
contract/expand glyphs in the action row rather than text controls beside the
purpose selector. They update presentation state without publishing or
serializing a graph edit. The compact Envelope preview uses the same
interactive vertical range as the expanded view so the curve's apparent
vertical framing remains continuous between both presentations.

The Envelope vertex-parameter rows use the shared Trimesh parameter renderer
with an explicit presentation density of `1.15`. The default Trimesh
presentation remains unchanged.

This supersedes the older suggestion in `node-graph-workflow.md` that polarity
be inferred solely from downstream connection role. A disconnected pitch
Envelope must still be visibly bipolar, and reconnecting an Envelope must not
silently change its authored meaning.

## Logarithmic Semantics

`logarithmic` is available for `control` and `volume`. It is inapplicable to
`pitch` and `scratch`, whose normalized values already have authoritative
pitch and traversal-coordinate meanings. Switching to pitch or scratch
normalizes the stored logarithmic policy to false in the same undoable edit;
the UI disables or hides the control rather than displaying an ignored state.

For control and volume, logarithmic mode applies the exact shared Cycle 1
transform with tension `30` to the prepared linear Envelope samples. The
transform occurs once, after curve sampling and before node `level` scaling.
It is not reimplemented with a scalar per-sample loop in Cycle V2.

Every consumer observes the transformed product:

- realtime audio output;
- traversal grids used by arithmetic and downstream previews;
- compact and expanded Envelope previews;
- spy blocks and spy rasterization;
- offline capture and deterministic test execution.

The logarithmic editor background retains all 16 Cycle 1 halving divisions,
but promotes every fourth division to the major-line treatment. The remaining
minor divisions use 120% of the ordinary minor-grid brightness so the dense
logarithmic spacing remains visible without competing with the major lines.

The current Cycle V2 traversal-grid publication samples the rasterizer
directly and therefore risks bypassing playback's logarithmic transform. The
end state publishes or samples one authoritative scaled representation; it
does not patch spies with a visualization-only transform. Toggling logarithmic
mode on a nontrivial Envelope must change observed spy values immediately
after the graph's normal publication boundary, without restarting the note
unless the established Envelope adoption policy requires it.

## Ownership, Lifecycle, And Complexity

- Serializable Envelope purpose and controls live in the Envelope node/model.
- Prepared geometry and sampling remain in the existing shared Envelope core.
- Per-voice playback cursor, loop, sustain, release, and scratch coordinate
  state live in voice/runtime state, never in the authoring model.
- The graph/compiler resolves port semantics and attachment targets; it owns no
  interpolation, scaling, pitch, or scratch traversal algorithms.
- UI adapters translate purpose edits and render immutable presentation state;
  they contain no DSP or connection inference.
- Ordinary Envelope block rendering, log transformation, and scratch
  application remain linear in the already-required sample/frame count.
  Purpose resolution and attachment binding are graph-publication work, not
  per-sample work.

Expected production work is a focused extension across the Envelope typed
configuration/model, node definition/codec, graph connection metadata and
validation, Envelope editor/presentation, and narrow Trimesh audio/preview
input boundaries. A large mixed node-kind adapter, repeated purpose switches
through generic runtime files, or copied Cycle 1 traversal code is evidence to
stop and extract the shared boundary. No generic flat-curve component may gain
Envelope purpose or playback state.

The expected footprint is approximately 300-600 net production lines spread
across those existing domain boundaries, plus a small shared extraction if the
scratch-coordinate policy is not already callable. No single orchestration or
adapter file should grow by more than roughly 150 lines for this feature. A
larger diff, more than one generic `NodeKind` dispatch expansion, or a new
cross-family bridge triggers architectural review before further implementation.

The expected end state deletes:

- the ambiguous one-domain Envelope output contract;
- `bool attachment` as the connection-kind representation;
- ignored scratch attachment execution;
- visualization-only or duplicated logarithmic transforms;
- polarity inference from downstream edges.

## Implementation Slices

1. **Complete:** add typed four-purpose persistence, canonical migration, purpose-aware
   presentation, and atomic incompatible-edge removal.
2. **Complete:** unify logarithmic audio, traversal-grid, preview, and spy output through the
   shared Cycle 1 transform; add value-level parity tests.
3. **Partial:** extract or expose the authoritative scratch-coordinate application rule,
   lower scratch processing attachments to immutable target bindings, and
   implement matching Trimesh audio/preview traversal. Immutable binding,
   block-coordinate audio, traversal preview, spy, and Stengah coverage are
   complete. Sample-accurate source rendering and expanded-editor publication
   remain at the boundaries listed in Status.
4. **Blocked on its declared dependency:** after the Voice Context pitch port merges, enable the typed pitch edge and
   reuse shared pitch sampling/tuning in the compiled voice plan.
5. **Partial:** remove transitional connection/output inference and run the architectural
   review required by `docs/TDD/README.md` before marking this implemented.
   The ambiguous serialized attachment boolean is deleted; the review below
   records the remaining runtime boundaries.

Each slice receives its own implementation, refactor, style, semantic-test,
automation, and commit pass.

## Implementation Review

Review performed 2026-07-31 against commits `4ad62d36` and `5b5e6b9d`:

- Production diff: 653 additions and 71 removals across 31 files (582 net).
- Largest production changes: `TrimeshNodeAudioProcessor.cpp` at 112 additions
  and 5 removals, `EnvelopePurpose.cpp` at 84 additions, and
  `GraphSerializer.cpp` at 75 additions and 3 removals. No production file
  exceeded the design's 150-line review threshold.
- Type/kind branches: Envelope purpose validation is centralized in
  `EnvelopePurpose`; graph validation adds one Envelope-purpose branch and the
  existing Trimesh runtime adds one resolved processing-attachment lookup. No
  generic flat-curve component gained Envelope state.
- Mature reuse: logarithmic scaling calls
  `Arithmetic::applyInvLogMapping(..., 30)`; Cycle 1 and Cycle 2 now call the
  shared `Rasterization::ScratchPositionPolicy`; Envelope playback remains in
  the shared playback engine.
- Deletion status: serialized `bool attachment` has been replaced by
  `ConnectionKind`; legacy JSON decoding remains intentionally at the codec
  boundary. UI renderer booleans are derived presentation flags, not graph
  state. Pitch routing, sample-accurate scratch source scheduling, and expanded
  runtime-preview publication remain incomplete as listed in Status.
- Semantic evidence: focused purpose, migration, undo, logarithmic value,
  scratch traversal, Stengah topology, duplicate-edge, and realtime tests pass.
  The Stengah UI fixture
  `scripts/fixtures/cycle-v2-agent-envelope-purpose.json` verifies the visible
  selector, bipolar/unipolar state, and atomic removal of its three scratch
  cables.
- Complexity: purpose and attachment resolution occur during graph edit,
  validation, compilation, or publication. Realtime processing receives
  resolved attachment payloads and preallocated traversal morph storage; it
  performs linear block/grid work without graph search, serialization, mutex
  acquisition, or allocation.

## Verification

### Purpose and persistence

- All four purposes save/load canonically and remain visually distinguishable
  while disconnected.
- Legacy volume, scratch, pitch, and generic Envelope cases migrate to the
  documented purpose without using node IDs or titles.
- Changing purpose removes incompatible edges atomically; undo restores the
  purpose and exact edge set.
- Duplicate scratch attachments do not survive canonicalization or produce
  overlapping cables.

### Connection grammar

- Control connects only to compatible generic control inputs.
- Volume connects to envelope-aware gain/factor inputs and does not attach as
  scratch or pitch.
- Scratch connects only by processing attachment to scratch-capable targets.
- Pitch is rejected before the Voice Context port exists; after merge, it
  connects only to that typed port.
- Invalid edges fail validation before runtime publication and are never
  silently reinterpreted after a purpose change.

### DSP and visualization

- Golden sample vectors for logarithmic off/on match
  `Arithmetic::applyInvLogMapping(..., 30)` exactly within the declared float
  tolerance, including values near `0`, `0.5`, and `1`.
- Audio, traversal grids, compact/expanded previews, offline capture, and spies
  report the same logarithmically transformed Envelope values.
- The transform is applied exactly once and before level scaling.
- Pitch shows a neutral centre at `0.5`; intermediate labels match
  `NumberUtils::unitPitchToSemis`, live values use Cycle 1's clamp, and exact
  endpoint presentation remains undefined.
- Entering pitch vertically fits a narrow curve through the shared Cycle 1
  range fitter; the Cycle 1 expand glyph restores the complete normalized
  range and its contract glyph fits it again without publishing a graph edit.
- The fit/full actions occupy the action row, not the purpose row, and use the
  exact Cycle 1 atlas cells `(6, 0)` and `(6, 1)` at their native 24-pixel size.
- Compact Envelope previews preserve the expanded panel's current vertical
  zoom, including fitted pitch ranges, while unrelated curve-node preview
  framing remains unchanged.
- Envelope vertex-parameter rows are 15% taller than the shared default.
- Logarithmic editor grids contain 12 minor and 4 major horizontal divisions,
  with minor brightness exactly 20% above the ordinary grid.
- Control, volume, and scratch retain unipolar shading regardless of whether
  they currently have a cable.
- The expanded editor labels purpose as `Mode` and presents all four modes as
  one icon row with a persistent selected highlight. Hovering across the row
  performs no graph publication or curve/canvas invalidation.

### Scratch parity

- A nonlinear scratch Envelope measurably changes samples from each attached
  Trimesh in both audio and preview, while an unattached peer is unchanged.
- Multiple attached targets observe the same per-voice scratch coordinate and
  do not advance the Envelope independently.
- Note-on, sustain/loop, note-off/release, global/per-voice behavior selected
  for the parity slice, disabled Envelope fallback, and attachment removal
  match the characterized Cycle 1 behavior.
- Stengah is the end-to-end fixture: its authored scratch edges survive
  save/load, affect every associated Trimesh, update spies, and render without
  duplicate attachment cables.
- Realtime diagnostics show no allocation, preparation, graph lookup, mutex
  wait, or serialization in the processing callback.

## Completion Criteria

- Envelope has a persistent, visible Control/Volume/Pitch/Scratch purpose that
  determines its output domain, connection kind, legal destinations, polarity,
  and applicable scaling policy.
- Scratch attachments audibly and visibly drive their explicitly attached
  Trimesh traversal using the authoritative Cycle 1 behavior.
- Logarithmic mode changes DSP values and every observable downstream product
  through one shared transform.
- Pitch presentation is bipolar while other modes are unipolar; no polarity is
  inferred from node names or current cables.
- After the Voice Context branch merges, pitch-purpose Envelopes connect only
  to its typed pitch input and drive shared per-voice tuning semantics.
- No copied envelope, logarithmic, pitch, or scratch algorithm; ambiguous
  attachment boolean; ignored semantic control; or purpose-specific logic in
  generic flat-curve infrastructure remains.
