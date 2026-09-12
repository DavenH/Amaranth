# Cycle V2 Global Audio Graph

## Status

In progress on 2026-09-12. The authored node/capability schema, required
singleton edit boundaries, explicit scope validation/partitioning, and Global
Input runtime binding are implemented. The three selectable-effect editors now
publish processing scope through the command service, retain invalid cables,
support stepwise repair, and undo each semantic edit. The legacy compiler path
remains only until migration lands. Canvas presentation now uses one compact
global-only semantic icon, including authored invalid states, and reserves no
scope space for voice nodes. Migration/layout and preset conversion remain.

Proposed on 2026-09-11. This TDD supersedes the temporary per-node `VOICE` and
`GLOBAL` text badges added during Cycle 1 audio-parity work. It does not change
DSP algorithms. It makes the existing voice-mix/global-processing boundary an
authored graph and UI concept.

## Decision

Cycle V2 presents two disjoint audio graphs on one canvas:

```text
Voice-local graph                         Global graph

[Voice Context] -> ... -> [voice audio]   [Global Input] -> ... -> [Output]
                          terminal               |
                              \__________________/
                               implicit voice mix
                               (no authored cable)
```

The voice-local graph runs independently for every synth voice. Its one
terminal time-signal path is mixed across active voices by the runtime. The
Global Input node exposes that mixed stereo signal as the root of a separate,
persistent global graph. There is no authored edge between the two graphs and
no global signal can re-enter the voice-local graph.

Every canvas contains exactly one Global Input and exactly one Output. The
global graph begins at Global Input and terminates at Output. Reverb and Delay
are always global. Waveshaper, IR Modeller, and EQ expose an authored Voice /
Global processing selector. Existing presets migrate every instance of those
effects to Global, matching Cycle 1.

Voice-local processing is the unmarked default. A compact icon identifies only
nodes executing in the global graph. The current text badges are deleted; no
`VOICE` or `GLOBAL` label remains on node headers or previews.

## User And Product Contract

The canvas should answer three questions without requiring knowledge of the
runtime compiler:

1. Which processing is repeated inside each voice?
2. Which processing happens once after voices are mixed?
3. Which cables are invalid because they cross that lifetime boundary?

Spatial separation supplies the primary answer: the global chain occupies a
distinct lane underneath the voice-local graph. The global icon is a secondary
node-level confirmation, especially while moving nodes or inspecting a partial
graph. Absence of the icon means voice-local; it is not replaced by a `VOICE`
label.

The processing selector changes ownership, not wet/dry state or bypass. It is
a two-choice control labelled `PROCESSING`, with `Voice` and `Global` segments
visible simultaneously. Selected state must have a shape/fill indication and
must not rely on colour alone.

## Authoritative Implementations And Reuse

- `RealtimeGraphRenderer` and `GraphAudioExecutor` already own the mature
  separation between per-voice execution, voice summation, and one persistent
  global execution. Their buffer ownership, silence processing, effect tails,
  and DSP processors remain authoritative and are reused unchanged.
- `GraphCompiler::compileProcessingScopes()` currently promotes every node
  downstream of a fixed-global effect and `compileVoiceMixBuffers()` discovers
  local-to-global boundary buffers. These are the temporary inference rules to
  replace with explicit graph membership and the Global Input boundary.
- `NodeDefinitionRegistry` currently declares all five Cycle 1 effects global.
  It remains authoritative for fixed capabilities, but selectable ownership
  moves to an authored node parameter for Waveshaper, IR Modeller, and EQ.
- `GraphValidator` owns cable/domain grammar. Scope validation belongs there,
  not in the canvas, individual effect editors, or the audio executor.
- `GraphCommandDispatcher` and the existing node-editor command service own the
  selector edit, publication, undo, and invalid-graph result. Editors must not
  mutate `NodeGraph` or manage undo directly.
- The existing property segmented-control presentation is reused for the Voice
  / Global selector.
- `NodeCanvasPresentation` and the established node icon/chrome vocabulary own
  the compact global indicator. No DSP or graph policy belongs in the painter.
- `scripts/port_cycle_v1_preset.py` remains the authoritative Cycle 1-to-V2
  representation translator. A shared migration/layout operation must serve
  both converter output and versioned V2 graph migration; the converter must
  not acquire a separate packing algorithm.

The adapter boundary is narrow: Global Input translates the already-existing
post-voice mix buffer into an ordinary linked-stereo graph output. It does not
mix voices, process effects, traverse the graph, or copy effect DSP.

## Authored Model

### Global Input

Add a first-class `globalInput` node with these semantics:

- singleton and required in every graph;
- non-deletable and not duplicable;
- no authored input ports;
- one linked-stereo time-signal output;
- no user parameters and no independent gain;
- executable only as the boundary source for the global graph; and
- supplied from the runtime's existing post-voice mix buffer.

Global Input does not conceal a model edge. The voice-local side must resolve to
zero or one terminal linked-stereo time-signal output:

- zero terminals means silence at Global Input, allowing an unfinished canvas;
- one terminal is the voice signal mixed across active voices; and
- more than one terminal is an `AmbiguousVoiceOutput` validation/compile error.

Parallel voice-local branches must therefore be combined explicitly with
existing local Add/Multiply/Stereo Join grammar before reaching their terminal.
This avoids a hidden summing topology or an invisible source selection stored
on Global Input.

An output observed only by a Spy remains terminal for audio-graph purposes;
Spies are observational and cannot choose or consume the voice output.

### Processing capability and authored mode

Node definitions declare one of three processing-scope capabilities:

| Capability | Node families | Authored selector |
| --- | --- | --- |
| Voice-only | Voice Context, synthesis, envelopes, spectral transforms, and other voice-domain nodes | No |
| Global-only | Global Input, Delay, Reverb, and Output | No |
| Selectable | Waveshaper, IR Modeller, and EQ | `Voice` / `Global` |

Selectable nodes persist a normalized string parameter named
`processingScope`, with values `voice` and `global`. New selectable effects
default to `voice` when created from the palette. Cycle 1 ports and migrated
Cycle V2 presets explicitly store `global`.

The parameter has ownership/topology impact, not merely DSP-configuration
impact. Changing it must validate and prepare a new execution plan. It must not
be handled as a parameter-only processor refresh or an editor-to-audio side
channel.

Domain-neutral routing/operation nodes do not need a selector. Their scope is
derived from the graph rooted at Global Input or from the voice-local graph in
which they participate. A neutral node reachable from both graphs is invalid;
the compiler never resolves the conflict by promotion.

## Cable Grammar

The model classifies each ordinary audio edge by the graph membership of its
endpoints:

| Source | Destination | Result |
| --- | --- | --- |
| Voice-local | Voice-local | Allowed when existing domain/channel rules pass |
| Global Input/global | Global/Output | Allowed when existing domain/channel rules pass |
| Voice-local | Global Input/global/Output | Invalid cross-scope edge |
| Global Input/global | Voice-local | Invalid global re-entry edge |

Configuration and processing attachments remain voice-local unless a later TDD
defines a meaningful global attachment contract. A global effect cannot retain
a Voice Context, envelope, morph, scratch, or Unison attachment merely because
the port domains happen to match.

Every valid global audio-processing node must be reachable from Global Input
and must reach Output. Output accepts only the global graph. Delay and Reverb
cannot be placed in the voice-local graph. A selectable effect in `voice` mode
cannot be attached to the global graph; the same node in `global` mode cannot
be attached to the voice graph.

### Invalid edits and error cables

A new drag that attempts a cross-scope connection is rejected on drop. During
the drag, the candidate cable uses the established invalid/error styling and
the status/help surface reports the scope mismatch.

Changing an already-connected selectable effect from Voice to Global, or from
Global to Voice, does not silently delete, move, or reconnect its cables. The
semantic parameter command succeeds and retains the authored edges, the now
invalid cables render with error styling, and compilation of the edited graph
reports the precise endpoints and required scope. The last valid prepared audio
generation remains active until the graph becomes valid. Undo restores the
mode and the previously valid generation as one semantic action.

This retained-error state is deliberate authoring feedback, not a runtime
compatibility path. The document may preserve and save that repairable invalid
state, but preset admission and audio-plan publication require a valid graph.

## Compiler And Runtime Design

Compilation separates topology into a voice partition and a global partition
before building executable ownership:

1. Validate required singleton nodes and fixed/selectable scope capability.
2. Walk ordinary audio edges outward from Global Input to identify the global
   partition and require that it terminates at Output.
3. Identify the remaining voice-local partition and resolve its zero or one
   terminal linked-stereo time output.
4. Reject cross-partition edges, global re-entry, conflicting neutral-node
   reachability, fixed-scope violations, and ambiguous voice terminals.
5. Compile the voice partition with existing oscillator-region/Unison/voice
   ownership.
6. Bind the terminal voice buffer to the existing voice mixer and expose that
   post-mix buffer through the Global Input step.
7. Compile the Global Input-to-Output partition into the existing persistent
   global executor.

`RuntimeOwnershipScope` may remain the prepared-plan representation, but it is
derived from validated authored membership. It is no longer inferred by
finding the first global effect and promoting everything downstream.
`voiceMixBufferIndices` becomes the explicit result of the resolved voice
terminal rather than a scan of local-to-global edges.

Global Input is processed on every host block, including blocks with no active
voices. It passes silence into the persistent global graph so Delay and Reverb
tails continue according to their existing DSP state. Switching a selectable
effect's mode causes ordinary non-realtime plan/processor preparation; no
processor changes ownership on the audio thread.

## Presentation Contract

### Scope indication

- Delete the current `VOICE` and `GLOBAL` pill badges and their header-space
  reservations.
- Paint no scope marker on voice-local nodes.
- Paint one compact global-processing icon on every node in the validated
  global partition, including derived global routing nodes. Global Input and
  Output may use the same marker even though their titles also imply scope.
- Use one recognizable 16 px visual footprint at 1x canvas zoom and reserve a
  24 px non-overlapping indicator area. The symbol must remain identifiable in
  monochrome and at production canvas zoom; colour is supplementary.
- Place the icon in the preview/header chrome using the existing right-side
  reservation system so it cannot collide with node action controls, Envelope
  purpose icons, ports, or titles.
- The indicator is informational, not a tiny mode toggle. Hover may expose the
  tooltip `Global processing`; mode changes belong in the effect controls.
- Invalid or uncompiled nodes retain their authored-mode indication, while
  error cables and compile diagnostics communicate why they are not part of a
  valid executable partition.

This is a single semantic icon, not a new icon family. It should be implemented
with the repository's existing code-native vector/icon vocabulary and reviewed
at actual node size before introducing a new asset format.

### Selector geometry and interaction

Waveshaper, IR Modeller, and EQ controls add one `PROCESSING` group containing
a two-segment `Voice | Global` selector:

- both choices are always visible;
- each segment has at least a 28 px height and the combined hit target spans at
  least 112 px at 1x UI scale;
- selected state has a persistent non-colour shape/fill treatment;
- hover, pressed, keyboard focus, and disabled states reuse established
  property-control styling;
- Left/Right changes the focused segment, Return/Space selects, and Escape
  cancels an uncommitted transient interaction where applicable; and
- a selection is one dispatcher-owned semantic command and one undo step.

Delay and Reverb do not show a disabled or redundant selector. Their controls
are always global. Voice-only nodes likewise show no processing control.

## Canvas Layout And Spatial Separation

The canvas remains one pannable/zoomable workspace. Separation is expressed by
placement and topology, not by introducing a second editor, modal page, or
hard clipping region.

For the default graph and preset migration:

- preserve every voice-local node's authored position, size, port side,
  editor state, model state, and relative layout exactly;
- compute the union of actual voice-local presentation bounds;
- place the global lane at least 96 world pixels below that union;
- place Global Input first, global processors in stable topological order, and
  Output last;
- use actual presentation bounds with at least 48 world pixels horizontal and
  vertical clearance; fixed constants may not stand in for effect/editor
  dimensions;
- retain existing relative order for global effects when it does not conflict
  with topology, then resolve overlap deterministically;
- route long chains into additional rows only when a single row would make the
  complete graph smaller than 0.75 canvas zoom in the standard production
  viewport; wrapped rows preserve topological reading order and cable clarity;
- fit the combined voice and global bounds with at least 40 screen pixels of
  visible margin on initial preset open; and
- never overlap the global lane with the keyboard, Spy/Guide dock, minimap, or
  palette when calculating the initial viewport.

The migration intentionally changes positions for Global Input, global
effects, global-only routing nodes, and Output. It does not run a general
auto-layout over the user's voice graph. Reopening an already-migrated graph is
idempotent and never reapplies packing.

## Preset And Graph Migration

The native graph format receives a versioned, one-way representation migration.
The stable stored format contains Global Input and explicit selectable-effect
scope; runtime compilation contains no old-format inference adapter.

For every bundled `.cyclegraph` and every fresh `.cyc` conversion:

1. Add the singleton Global Input if absent.
2. Mark every Waveshaper, IR Modeller, and EQ instance `global`.
3. Keep Delay, Reverb, and Output fixed-global.
4. Classify the old graph with the existing compiler's pre-migration scope
   rules only inside the migration utility.
5. Remove old voice-to-first-global boundary edges.
6. Connect Global Input to the former first global node, preserving the old
   global effect order and all downstream connections to Output.
7. If there are no effects, connect Global Input directly to Output.
8. Reject rather than guess when an old graph has multiple inequivalent
   voice/global boundaries or cannot identify one terminal voice signal.
9. Apply the deterministic global-lane layout while preserving voice-local
   presentation and every non-position property.
10. Serialize only the new format and verify a second migration is byte-stable.

The Cycle 1 converter reuses the same graph-building and layout policy. It must
preserve authored effect parameters, curve/mesh models, enabled state, port
sides, editor dimensions/state, probes, guide resources, and user-adjusted
voice-node positions. Preset migration is semantic; it must not be used as an
excuse to regenerate or normalize unrelated preset content.

## Thread, Lifecycle, And Failure Boundaries

- Scope selection, validation, migration, layout, and compilation run off the
  audio thread.
- The audio thread reads only an accepted immutable plan and preallocated
  voice/global buffers.
- No node editor, canvas painter, serializer, or migration utility decides DSP
  lifetime independently.
- An invalid scope edit cannot partially publish a new generation or destroy
  the previous global effect state on the callback.
- Preset load either produces one complete valid new-format graph or reports a
  migration error without partially rewriting the document.
- Global Input introduces no allocation, lock, graph read, or voice scan on the
  callback beyond the existing preallocated voice summation.

## Semantic And Interaction Tests

### Graph grammar

- A minimal Voice Context-to-terminal voice graph plus Global Input-to-Output
  global graph validates and renders.
- Zero voice terminals produces silence; two terminal time paths report
  `AmbiguousVoiceOutput`.
- Voice-to-global, global-to-voice, voice-to-Output, and global-to-Voice Context
  cables report endpoint-specific scope errors.
- Global Input is unique, required, non-deletable, has no input, and emits
  linked stereo.
- Every global node is reachable from Global Input and reaches Output.
- A neutral operation used by both partitions is rejected rather than promoted.

### Effect scope

- Delay and Reverb are always global and expose no selector.
- New Waveshaper, IR Modeller, and EQ nodes default to Voice.
- Migrated/ported instances of those nodes explicitly store Global.
- Voice-mode instances use isolated per-voice processor state; Global-mode
  instances use one persistent processor after voice summation.
- Switching mode while disconnected produces a valid new plan and changes
  processor ownership without changing DSP parameters or model content.

### Complete edit sequence

- Connect a selectable effect in the voice graph, switch it to Global, retain
  both authored cables as visible errors, observe a failed new compilation and
  unchanged last-valid audio, repair both connections into the global graph,
  observe the new global processing, then undo each semantic edit.
- Repeat Global-to-Voice in the global graph.
- Attempt a new cross-scope cable drag and verify invalid hover/drop feedback,
  no durable edge, stable selection, and a useful diagnostic.

### Audio lifecycle

- Two simultaneous notes through a Voice-mode effect use separate effect state;
  the same graph with Global mode processes their summed stereo signal once.
- Delay and Reverb continue processing silence and emit tails after every voice
  retires.
- Global stereo input remains stereo through switchable and fixed-global
  effects and Output.
- Block partitioning and note order do not change which samples cross the
  voice/global boundary.

### Presentation and layout

- Voice-local nodes reserve no scope-badge space and paint no scope label/icon.
- Every validated global node paints exactly one icon and no text badge.
- Icon bounds do not overlap titles, action controls, ports, or preview content
  at minimum, natural, and expanded node sizes.
- Selector visual footprint, hit target, keyboard interaction, publication,
  downstream refresh, and undo satisfy the full gesture contract.
- Migration preserves all voice-local bounds exactly, creates no node overlap,
  maintains the required lane gaps, and is idempotent.
- Production-size screenshots cover the default graph, a long migrated preset,
  both selectable modes, and the retained error-cable state.

### Preset admission

- Every bundled preset contains one Global Input and one Output.
- Every migrated effect has the required scope and is in the correct partition.
- Every bundled graph validates, compiles, opens inside the production viewport,
  and retains its pre-migration non-presentation semantic hash apart from the
  declared boundary/scope additions.
- Representative Cycle 1/V2 audio fixtures remain within their established
  parity thresholds after migration; layout changes cannot alter audio.

## Negative Boundaries

- Do not keep the current text badges alongside the icon.
- Do not mark every voice-local node merely to create visual symmetry.
- Do not infer a global chain from whichever fixed-global effect happens to
  appear first after migration.
- Do not allow a global signal to become voice-local through a neutral utility
  node, bypassed effect, disabled effect, or mode change.
- Do not silently delete or auto-rewire cables when scope changes.
- Do not make effect bypass change processing ownership.
- Do not copy effect DSP, voice summation, graph traversal, or segmented-control
  behavior into a Global Input adapter or effect editor.
- Do not run a whole-canvas auto-layout or disturb user-authored voice-node
  placement during preset migration.
- Do not accept a hidden multi-terminal voice sum; require explicit local
  combination or report ambiguity.
- Do not mark this TDD implemented while legacy scope inference, text badges,
  old-format presets, or non-idempotent layout migration remain.

## Implementation Slices

1. Add failing graph-contract tests for singleton Global Input, disjoint scope
   grammar, unique voice terminal, and fixed/selectable effect capabilities.
2. Add the `globalInput` node and explicit `processingScope` parameter schema;
   keep runtime behavior unchanged until the compiler partition is complete.
3. Replace downstream global promotion with validated voice/global partitions,
   bind Global Input to the existing post-voice mix, and delete old boundary
   inference.
4. Add the three effect selectors through the node-editor command service,
   including invalid connected-mode changes, last-valid audio, repair, and
   undo sequence tests.
5. Delete `VOICE`/`GLOBAL` badges and add the global-only icon with focused
   geometry, accessibility, and production-size visual evidence.
6. Implement one deterministic migration/layout service and use it from native
   graph-version migration, the Cycle 1 converter, and the default graph.
7. Migrate every bundled preset, audit semantic diffs and graph bounds, and run
   representative Cycle 1/V2 audio comparisons.
8. Delete migration-only old-scope classification when supported stored inputs
   and checked-in presets no longer require it; retain only the explicit
   versioned decode boundary if product compatibility policy requires it.

## Completion Criteria

- The canvas visibly and structurally separates voice-local and global audio.
- Global Input is present by default and is the only root of the global graph.
- The voice/global boundary has no authored cable and no hidden ambiguous mix.
- Delay and Reverb are fixed-global; Waveshaper, IR Modeller, and EQ can be
  authored Voice or Global.
- Cross-scope connections cannot enter an accepted execution plan, and scope
  changes retain invalid cables as actionable errors.
- Only global nodes carry a compact icon; all text scope badges are removed.
- Default and migrated layouts place the global graph below the preserved voice
  graph without overlap and fit the production viewport comfortably.
- Every bundled preset is migrated, valid, idempotent, and semantically audited.
- Existing global tail, stereo, master-output, and audio-parity contracts remain
  intact.
- Focused graph, compiler, runtime, editor gesture, migration, layout, and
  automation tests pass with production-size screenshot evidence.
