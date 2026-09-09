# Cycle V2 Voice Context Default Scratch Attachment

## Status

Feature implementation is complete on `cycle2/scratch-default`. Default
resolution now uses compiled oscillator-region ownership, so Trimesh side
branches that feed a region inherit the same scratch source as its waveform
spine. Effective inherited attachments participate in execution ordering, and
topology recompilation resets retained preview processor state. The
`scratch-test` inherited, explicit, and undo-restored forms consequently
produce identical settled node and Spy outputs.

The offline simplifier and preset rewrites remain explicitly deferred until
this work is merged to master and then into the preset-cleanup branch.

## Goal

Let one scratch-purpose Envelope attach to Voice Context and become the default
scratch trajectory for every scratch-capable Trimesh in that context. Preserve
per-Trimesh authoring for explicit overrides and provide a visible way for a
target to reject the inherited default and use ordinary voice-time traversal.

Use this representation to simplify migrated factory presets when one scratch
Envelope currently fans out to at least 75 percent of the eligible Trimesh nodes
in one Voice Context.

## Problem

Cycle V2 currently persists one processing-attachment edge from a scratch
Envelope to every Trimesh that consumes it. Most migrated Cycle 1 presets have
one global scratch Envelope connected to every time, magnitude, and phase mesh.
Those repeated cables are semantically correct but obscure the audio graph and
repeat a voice-wide default at each consumer.

Voice Context already establishes the scope in which downstream nodes inherit
default modulation, pitch, and Unison configuration. Scratch differs in its
runtime product, but its common factory-preset topology has the same ownership
shape: one per-voice source, many context-scoped consumers, and occasional
target-local exceptions.

This cannot be implemented as graph-proximity inference. The durable graph must
show the default attachment, the compiler must resolve one unambiguous Voice
Context for each consumer, and exclusions must remain explicit and authorable.

## Current Factory Inventory

A structural audit of the 230 checked-in presets found 152 graphs where one
scratch Envelope attaches to every Trimesh in the graph. Those graphs contain
513 repeated Envelope-to-Trimesh scratch edges. Replacing each complete fanout
with one Envelope-to-Voice Context edge would retain 152 edges and remove 361,
with no `Use Voice Time` exclusions required.

This inventory is evidence for the feature, not sufficient proof for an
automatic rewrite. The implementation-time audit must use the compiler's real
Voice Context assignments rather than assuming that every Trimesh in a file
belongs to the same context.

## Authoritative Implementations

- Voice Context attachment ownership and default semantics:
  `cycle-v2/src/Graph/NodeDefinition.cpp`,
  `compileVoiceContexts()`, `assignVoiceContexts()`, and
  `voiceContextForNode()` in `cycle-v2/src/Graph/GraphCompiler.cpp`.
- Typed attachment validation: `cycle-v2/src/Graph/GraphValidator.cpp` and the
  `ConnectionKind`, `AttachmentType`, and `Port` metadata in
  `cycle-v2/src/Graph/NodeGraph.h`.
- Scratch Envelope preparation and lifecycle:
  `EnvelopeSignalProcessor`, `EnvelopePlaybackEngine`, and the completed
  `cycle-v2-envelope-purpose-routing-and-scaling.md` contract.
- Target-local scratch consumption:
  `TrimeshNodeAudioProcessor::scratchAttachment()` and the existing prepared
  `GraphStepAttachment` path.
- Trimesh enablement and scratch availability:
  `buildTrimeshConfiguration()` in
  `cycle-v2/src/Runtime/NodeDspConfiguration.cpp`.
- Existing graph commands, transient gesture rules, undo, serialization, port
  presentation, and canvas cable rendering remain authoritative for authoring.

The implementation must reuse the current Envelope playback and Trimesh scratch
application unchanged. It may translate context-level authoring edges into
effective target bindings during compilation; it must not add another scratch
evaluator, traversal algorithm, or realtime graph lookup.

## Decision

### Voice Context default

Add a typed `scratch` attachment input to Voice Context with cardinality zero or
one. A scratch-purpose Envelope attached there defines the context's default
scratch source. Voice Context owns only the resolved immutable reference; the
Envelope continues to own its mesh, parameters, enablement, and prepared
playback behavior.

The `context` output propagates the default by compiled context ownership, not
by copying an attachment edge through each signal node. Only scratch-capable
Trimesh nodes assigned to exactly one Voice Context can inherit it. A target
with no unique context does not inherit by traversal order and produces a
compile diagnostic when a default would otherwise be ambiguous.

### Target precedence

Resolve each Trimesh scratch input in this order:

```text
effective scratch = explicit target-local scratch source
                 ?? explicit target-local Use Voice Time override
                 ?? Voice Context default scratch source
                 ?? ordinary voice-time traversal
```

An explicit scratch Envelope wins over the context default. If that explicit
Envelope is disabled or unavailable, the target uses voice time; it must not
fall through to the inherited default. This preserves the present meaning of a
disabled direct scratch attachment.

### Explicit exclusion

Add a small configuration-only node whose product means `Use Voice Time`. It
connects to a Trimesh scratch port using the same attachment affordance but
publishes no samples and owns no DSP state. One instance may fan out to several
excluded meshes.

The compact label should be `Use Voice Time`; an internal type such as
`scratchDefaultOverride` is preferable to calling it a generic no-op. It is not
an audio no-op: its durable semantic purpose is to block inherited scratch and
select the existing voice-time fallback. It must have no executable runtime
step, signal buffer, preview rasterizer, or hidden parameters.

Do not represent exclusion with an unconnected flag, a magic Trimesh parameter,
a disabled dummy Envelope, or an edge with no source. Those forms either hide
topology, duplicate Envelope state, or make the graph impossible to understand
from its cables.

### Typed attachment grammar

Keep `ProcessingAttachment` as the scratch connection kind. Generalize its
validation around registered source/destination port metadata so the scratch
Envelope and `Use Voice Time` product are both explicit compatible sources.
Envelope sources retain the additional requirement that their purpose is
`scratch`.

The validator must not grow an open-ended `NodeKind` switch for every processing
attachment producer. If the current single `AttachmentType` field cannot
truthfully express both products, introduce a narrow scratch-binding product or
accepted-product set in port metadata rather than weakening type checks.

## Compilation And Runtime

Extend `CompiledVoiceContext` with the immutable default scratch reference. Use
the existing Voice Context assignment result to resolve effective scratch for
each Trimesh execution step after explicit graph attachments are known.

Compiler lowering may materialize derived, target-local
`GraphStepAttachment` entries for inherited sources. This is the preferred
boundary because it lets audio and preview continue consuming the exact current
target-local attachment representation:

```text
durable graph: Envelope -> Voice Context.scratch
compiled plan: Envelope -> each inheriting Trimesh.scratch
```

The derived bindings are immutable plan state, not durable graph edges, and do
not appear on the canvas or in serialization. They must participate in the same
dependency scheduling and per-voice Envelope state ownership as today's direct
attachments. One prepared Envelope geometry may be shared, while mutable
playback state remains correctly scoped per synth voice.

`Use Voice Time` resolves to no effective runtime attachment for its target.
The compiler records the explicit override while resolving defaults, then omits
it from executable steps. `TrimeshNodeAudioProcessor` consequently follows its
existing no-attachment voice-time path without a new branch in its inner loops.

`NodeDspConfiguration` must derive scratch availability from the resolved
binding rather than searching only for a direct durable edge. Move that
resolution into compiler/configuration preparation if necessary; do not add
runtime graph ancestry searches.

Expected complexity:

- context assignment and default resolution remain linear in nodes plus edges;
- compilation adds at most one effective binding per scratch-capable Trimesh;
- realtime work and memory remain unchanged from equivalent explicit edges;
- `Use Voice Time` adds no runtime processor, buffer, or per-sample branch.

## Serialization And Compatibility

Persist the Voice Context default and target-local override as ordinary typed
edges and registered node definitions. Existing graphs containing direct
Envelope-to-Trimesh scratch edges remain valid and retain identical precedence
and sound. Loading must not silently rewrite them.

If attachment representation or names change, GraphSerializer owns only the
narrow version/type translation. It must not decide which presets satisfy the
75-percent migration policy. Canonical save/reload must preserve default,
explicit override, and exclusion topology without derived target bindings.

The stable end state has no compatibility adapter beyond decoding old direct
scratch edges, which remain a supported authoring form.

## Preset Simplification Policy

Extend the offline preset simplifier, not the graph loader, with this policy for
each uniquely resolved Voice Context:

1. Enumerate scratch-capable Trimesh nodes owned by the context.
2. Group their direct scratch attachments by source Envelope.
3. Select a source only when it covers at least 75 percent of eligible targets,
   is the unique highest-coverage source, and converting it preserves every
   target's effective behavior.
4. Replace that source's repeated target edges with one attachment to Voice
   Context.
5. Retain direct edges from other scratch sources as target-local overrides.
6. Attach one shared `Use Voice Time` node to targets that previously had no
   scratch attachment and would otherwise inherit the new default.
7. Apply the rewrite only when it reduces attachment-edge count and does not
   introduce an ambiguous context, probe anchor, or unsupported reference.

The 75-percent threshold is a presentation/migration heuristic, not runtime
semantics or automatic canonicalization. Below it, explicit target edges are
usually clearer. Emit an audit diagnostic instead of rewriting ties, multiple
candidate defaults, cross-context Envelope ownership, or graphs whose context
cannot be proven.

The simplifier must be idempotent. It must never convert an already-authored
context default back into repeated edges.

## UI And Authoring

- Voice Context exposes one additional, clearly identified scratch attachment
  port using the established scratch icon and attachment cable style.
- Connecting a scratch Envelope to Voice Context authors one undoable semantic
  graph command and replaces any previous context default according to normal
  input cardinality rules.
- Direct attachment to a Trimesh remains available and visually communicates an
  override.
- `Use Voice Time` is available from an appropriate routing/utility palette
  category and is visually distinct from an Envelope.
- Connection previews and tooltips state whether the destination will inherit,
  override, or suppress the Voice Context default.
- Removing a local override immediately restores inheritance; removing the
  context default immediately restores voice time except where direct sources
  remain.

All graph edits must use `GraphCommandDispatcher`. The UI must not mutate
`NodeGraph`, publish revisions, or manage undo directly.

## Implementation Slices

1. Add the typed Voice Context scratch input and compile its immutable default
   reference without changing existing direct attachment behavior.
2. Resolve inherited bindings per uniquely assigned Trimesh and reuse the
   current runtime/preview scratch attachment path.
3. Add the configuration-only `Use Voice Time` product and explicit precedence,
   validation, serialization, and graph-command authoring.
4. Add compact port/node presentation, connection feedback, and complete undo
   interaction fixtures.
5. Extend the preset simplifier with the 75-percent policy, audit the factory
   library, and commit rewritten presets as a separate coherent slice.
6. Refactor compiler mechanics so Voice Context orchestration reads as default
   resolution rather than a collection of scratch-specific graph scans.
7. Correct default resolution for Trimesh side branches by resolving each
   target through its compiled oscillator region, then prove `scratch-test`-like
   inherited and direct topologies produce identical node and probe outputs.
8. Complete a production-size Voice Context review: keep all inputs on the
   left, contain every socket with the established bottom inset, identify the
   scratch attachment locally, and verify connected graph appearance and the
   complete replace/remove/undo interaction.

## Implementation Evidence

- Slice 1 adds the typed Voice Context scratch input, records the default source
  in `CompiledVoiceContext`, and lowers it to the existing target-local prepared
  attachments for uniquely owned Trimesh steps.
- Direct target attachments take precedence. Configuration preparation follows
  the effective source so disabled inherited Envelopes select voice time while
  an active direct source still overrides a disabled default.
- Focused compiler coverage passes 28 assertions across two cases. The existing
  scratch runtime case now additionally proves that one context attachment
  produces the same blocks and traversal grids as repeated direct attachments.
- `Use Voice Time` is a registered configuration-only scratch-binding source.
  Validation permits it only on Trimesh scratch inputs; compilation consumes it
  as an exclusion marker and emits no attachment, execution step, or buffer.
- One exclusion node may fan out to several targets. Runtime coverage proves an
  excluded target matches the pre-existing voice-time path while its peers keep
  the inherited Envelope trajectory.
- Canonical serialization retains only authored default and exclusion edges.
  Focused `GraphCommandDispatcher` coverage connects and removes an exclusion,
  observes effective compiled binding changes, and undoes back through both
  states.
- The compact 174 by 76 pixel utility uses one aligned right-side attachment
  socket and a low-emphasis voice-time glyph. Hover help distinguishes context
  defaults, inherited targets, direct overrides, and suppression. The focused
  automation fixture is
  `scripts/fixtures/cycle-v2-agent-voice-time-override.json`; its semantic run
  passed and a production canvas review was captured at
  `/private/tmp/cycle-v2-voice-time-override-os.png`.
- Compiler resolution now indexes local scratch bindings once before lowering
  inherited bindings. Envelope playback, Trimesh traversal, and realtime
  processing remain unchanged.
- Side-branch defaults resolve through the oscillator region's Voice Context,
  and a second dependency ordering includes effective processing attachments.
  This schedules a scratch Envelope before every inheriting Trimesh without
  persisting derived graph edges.
- Fresh-executor runtime coverage proves inherited and direct waveform,
  magnitude, and phase branches produce exact matching blocks, traversal grids,
  and Spy arrays. Presentation-model coverage also proves equivalent topology
  recompiles produce identical live preview arrays instead of retaining prior
  processor state.
- Voice Context is 182 pixels tall so all four established left-side sockets
  retain the standard bottom inset. The scratch row uses the existing scratch
  purpose icon and a local `Scratch` label below the summary; geometry tests
  prove containment, separation, and port alignment.
- `cycle-v2-agent-voice-context-scratch-default.json` exercises the standard
  demo graph through inherited scratch, equivalent direct cables, undo, default
  replacement, default removal, and restoration. A separate local run against
  the saved `scratch-test` graph proves its settled inherited, explicit, and
  undo-restored node and Spy sums match exactly.
- The complete Cycle V2 test binary passes all 634 cases and 336,116
  assertions. All 230 shipped graphs remain canonical and compile.
- Preset simplifier implementation, the 75-percent audit, and preset rewrites
  remain deferred by explicit branch sequencing; no `.cyclegraph` contents are
  changed on this branch.

## Expected Production Scope

Expected changes should remain concentrated in:

- graph type/port metadata, node registration, validation, and serialization;
- Voice Context compilation and prepared attachment lowering;
- one small domain-owned `Use Voice Time` node/presentation implementation;
- graph-command connection handling and focused canvas presentation;
- preset simplification tooling.

The runtime Trimesh and Envelope implementations should receive little or no
production logic. A large adapter, copied scratch evaluator, new realtime graph
lookup, or repeated generic `NodeKind` branching is evidence that the design
boundary has been missed.

## Tests

### Semantic graph and compiler tests

- One context-level scratch Envelope changes every context-owned Trimesh exactly
  as equivalent direct attachments do in audio and preview.
- Two Voice Contexts with different defaults remain independent.
- A direct target Envelope overrides the context default.
- A disabled direct target Envelope selects voice time instead of falling
  through to the default.
- `Use Voice Time` suppresses inheritance for one and several targets.
- Removing a local override restores the default; removing the default restores
  voice time.
- Context-free and multiply-context targets produce deterministic diagnostics.
- The compiled plan contains effective target bindings but canonical JSON does
  not contain derived edges.
- No default or override adds an executable utility-node step or buffer.

### Migration tests

- 100-percent and 75-percent uniform fanout simplify to one context edge plus
  only required exclusions.
- Coverage below 75 percent is unchanged.
- Tied candidate sources, cross-context sources, probe-anchored edges, and
  ambiguous ownership are reported and unchanged.
- Other direct Envelope sources remain explicit overrides.
- Conversion is behavior-preserving, reduces edge count, and is idempotent.
- Every rewritten factory preset loads, validates, compiles, saves, reloads,
  and produces the same focused audio/preview result as its explicit-edge form.

### Interaction tests

- Connect a default, connect and remove a direct override, connect and remove
  `Use Voice Time`, commit, observe downstream preview/audio effects, and undo
  each complete gesture.
- Production-size screenshots show default fanout, a local Envelope override,
  and a local voice-time exclusion without cable ambiguity.

## Negative Boundaries

- No implicit discovery by visual proximity, node order, or unrestricted graph
  traversal.
- No copied Envelope playback, scratch-coordinate, mesh traversal, preview, or
  DSP behavior.
- No realtime graph lookup, allocation, serialization, or context resolution.
- No hidden per-Trimesh inheritance flag and no disabled dummy Envelope used as
  an exclusion marker.
- No loader-time 75-percent heuristic or automatic rewrite of authored graphs.
- No change to direct scratch attachment semantics or Cycle 1 scratch parity.
- No UI mutation outside `GraphCommandDispatcher`.

## Completion Criteria

- Voice Context truthfully owns one optional context-wide scratch default.
- Each Trimesh resolves explicit source, explicit voice-time override, inherited
  source, or ordinary voice time with the documented precedence.
- Audio and preview reuse the existing prepared target-local scratch path and
  match equivalent explicit-edge graphs.
- Existing direct-edge graphs remain valid and behaviorally unchanged.
- The exclusion node is configuration-only and creates no runtime work.
- Factory conversion uses the conservative 75-percent policy, produces fewer
  edges, preserves behavior, and is idempotent.
- Complete edit sequences prove transient publication, commit, downstream
  effect, and undo.
- Production review confirms bounded diff size, no duplicated domain logic, no
  broad kind switching, and no unresolved deletion targets.
