# Cycle V1 Factory Preset Library Migration

## Status

Complete. All 229 Cycle 1 factory sources have a Cycle V2 destination. Generated
graphs omit structural no-ops, preserve authored spectral range on Trimesh, and
use compact, authorable, non-overlapping layouts.

## Goal

Migrate every Cycle 1 factory `.cyc` preset that Cycle V2 can represent into a
checked-in `.cyclegraph`, excluding presets already migrated. Preserve authored
state through the existing Cycle 1 migration and Cycle V2 graph boundaries,
and record unsupported features or product defects instead of silently dropping
them.

## Authoritative Implementations

- `PresetMigrator` and `Document::exportPresetJSON()` own legacy `.cyc` parsing,
  schema migration, default synthesis, and canonical Cycle 1 preset JSON.
- `scripts/port_cycle_v1_preset.py` owns the narrow translation from canonical
  Cycle 1 state into Cycle V2 graph ownership and routing.
- `scripts/simplify_cycle_v2_presets.py` owns conservative post-migration
  cleanup for both generated and previously protected factory graphs. The
  converter invokes the same cleanup before serialization so regeneration does
  not restore structural no-ops.
- Cycle V2 node definitions, graph validation, serialization, and runtime
  compilation own the target representation and acceptance contract.
- Mature Cycle 1 DSP, mesh, envelope, effect, and modulation implementations
  remain authoritative for behavior. The converter must not approximate or
  duplicate them.

## Design

Run one Cycle 1 automation session over the factory directory. For each source
preset, open the `.cyc` through the product loader and export canonical JSON.
The sweep records open/export failures independently so one malformed preset
does not prevent inventorying the rest of the library.

Classify each canonical preset before conversion:

- already migrated;
- representable by the current converter and Cycle V2 nodes;
- representable after a narrow converter mapping;
- blocked by a missing Cycle V2 feature or an ambiguous semantic mapping; or
- blocked by a Cycle 1 load/export or Cycle V2 validation/runtime defect.

Conversion is all-or-nothing per authored feature. Active layers, envelopes,
effects, modulation mappings, multisamples, or other meaningful state may not
be ignored merely to produce a loadable graph. A blocked preset remains absent
and its blocker is recorded in this TDD, `ui-bugs.md`, `audio-bugs.md`, or
`refactors.md` according to ownership.

The stable end state is a repeatable batch workflow around the existing
product export and graph converter. Canonical JSON is an intermediate build
artifact, not another checked-in preset format or compatibility layer.

### Layer enablement

Cycle 1's `properties.active` is authored layer state for connected or populated
layers. Cycle V2 stores it as an `enabled` parameter on each retained Envelope
and Trilinear Mesh node and exposes that parameter through the enable action in
the expanded editor header. Empty spectral meshes and inactive unconnected
Envelopes are omitted under the no-op rules below.

The source node remains the sole durable owner. Runtime translation happens at
the existing DSP-configuration boundary:

- a disabled time-domain Trilinear Mesh publishes zero;
- a disabled spectral Trilinear Mesh derives its immediately downstream
  Add/Multiply topology, following one optional Pan, and publishes the operation
  identity: zero for phase/additive magnitude and one for multiplicative
  magnitude;
- a disabled Envelope publishes its purpose identity: one for volume, normalized
  `0.5` for pitch, and zero for generic control. A disabled scratch Envelope is
  treated as unavailable by the attached Trimesh so traversal falls back to
  voice time, matching Cycle 1.

The Pan node does not gain a second independently editable enable flag. The
factory already receives the immutable graph and node ID while preparing DSP,
so this is narrow topology translation rather than duplicated layer state.
Changing the source parameter or operation topology rebuilds the immutable
execution configuration; audio processing performs no graph lookup. Once
native Cycle V2 layer stacks own a first-class layer object, this downstream
inspection is a deletion target and `enabled` moves unchanged with that owner.

### Generated graph layout

Generated coordinates are durable preset presentation, not incidental output.
The converter owns a deterministic layout pass after semantic graph assembly.
It uses the production compact-node footprints from `naturalSizeForNode()` as
the acceptance authority and conservative shared cell/gap constants rather
than hand-authored coordinates for individual presets.

The main audio path reads left to right: Voice Context and time sources, FFT,
spectral fan-in, IFFT, effects, and Output. Magnitude meshes sit in aligned rows
above their accumulator operations; phase meshes use corresponding rows below.
Layer stacks remain in monotonic lanes because the operation rotator always
keeps its output on the right; a generated switchback would otherwise persist a
layout the user could not reproduce. Modulation, Unison, pitch, scratch, and
inactive Envelope nodes occupy aligned auxiliary lanes without competing with
the audio spine. The active pitch Envelope is placed immediately left of Voice
with their connected ports aligned.

The layout contract is mechanical: no compact node rectangles overlap after
Cycle V2 resolves their natural sizes, ordinary horizontal gaps are consistent,
the signal spine and accumulator lanes are monotonic, and generated port-side
overrides are reachable through the corresponding compact control. This is a
converter-specific presentation pass; it does not duplicate canvas routing or
become a hidden runtime auto-layout system. If user-authored graphs later need
automatic organization, extract a shared service around `naturalSizeForNode()`
and delete the converter-local footprint policy.

### General inline Pan

Cycle V2 already presents Pan as a cable-inline operation. Its current
`SpectralLayer` name and validator/runtime restriction are an incomplete
implementation, not evidence that time-layer panning has no graph destination.
Generalize that existing operation to accept time signals and apply the mature
`Arithmetic::getPans` channel gains without changing its spectral magnitude and
phase contracts. The three remaining presets then place Pan immediately after
each time Trimesh and before layer summation.

Pan is only stereo placement. It owns no spectral range or operation mode, and
a centred Pan is a graph no-op in every domain. The converter emits the inline
Pan node only when the authored value differs from `0.5`.

Spectral Trimesh owns Cycle 1 `range` (also called width) as a normalized
parameter and exposes it in the expanded editor. The mature
`SpectralLayerCore` remains authoritative for phase scaling and magnitude
shaping. Additive versus multiplicative magnitude behavior is inferred from
the downstream Add/Multiply topology, following one optional inline Pan; it is
not duplicated as node state. Disabled spectral Trimesh output uses that same
topology to select zero or multiplicative identity.

The graph loader provides a narrow compatibility translation for existing V2
graphs that stored `range` and `mode` on Pan: it ignores the obsolete Pan mode,
moves range to the directly upstream Trimesh after edges are loaded, and emits
only canonical Pan state on the next save. The stable end state deletes this
read-only translation when pre-correction V2 graph compatibility is retired.

### Authorable cable geometry

Persisted port sides must be states reachable through the production compact
node controls. Add/Multiply inputs use only the four operation layouts;
Trimesh outputs use only the output-side cycle; single-input/single-output
effects use only `NodePortLayout`; and FFT/IFFT retain definition-owned sides
until they expose a rotator. Envelope gains the same output-side control as
Trimesh, with its purpose icon shifted left so both header symbols remain
separate. Generated volume Envelopes retain the ordinary right output.

The converter aligns the main signal path by actual port centres rather than
node centres. FFT magnitude/phase outputs, their serial operation lanes, IFFT
inputs, the post-IFFT chain, and Output share the corresponding y coordinates.
Layer meshes centre over the operation input they feed. Auxiliary Envelopes
sit near their consumers without occupying the main cable corridor.

The production `NodeCanvasScene` cable path is the acceptance authority for
signal-cable/node intersections. Generated layouts keep every ordinary audio
or control cable out of unrelated compact node bounds. Domain-context fanout,
configuration attachments, and processing attachments are excluded because
their distribution/bundle geometry is distinct; missing obstacle-aware routing
for domain-context fanout is recorded in `ui-bugs.md`.

### Empty spectral-layer identity

An authored spectral layer with no mesh vertices has no spectral content.
Emitting Trimesh, Pan/range, and operation nodes for that empty branch obscures
the effective graph without preserving additional behavior. The converter
omits empty magnitude and phase branches and carries the corresponding FFT
output directly to the next non-empty operation or IFFT.

If both spectral branches contain no non-empty layer, the converter also omits
the FFT and IFFT nodes and connects the time-layer result directly to the
post-oscillator processing chain. Non-empty spectral layers retain their Pan,
range, operation topology, enablement, model, and guide/scratch ownership.

### Volume Envelope cable approach

When volume processing is active, generated volume Envelopes occupy a row below
and to the left of Multiply. The active Envelope is the rightmost sibling, one
standard gap from Multiply, so its normal right-side output approaches the
lower-left operation input without reversing direction or overlapping the node.
Inactive, unconnected volume Envelopes are omitted as structural no-ops.

### Inactive unconnected Envelopes

An inactive Envelope with no connection contributes no state to the rendered
graph. The converter omits these nodes regardless of purpose. Active and
connected Envelopes preserve their model and parameters.

### Post-migration structural cleanup

The checked-in library receives the same conservative structural pass as newly
converted graphs. It removes an isolated non-Output node only when no edge,
Guide assignment, signal probe, or audio binding references it. A Guide resource
is removed only when it has no assignment; heatmaps are retained while any
remaining Guide uses them.

A graph whose only Trimesh nodes are empty time layers and whose remaining node
kinds cannot generate or process nonzero content collapses to a single Output.
An empty time mesh is otherwise retained when populated spectral layers depend
on it as their zero-spectrum seed. A direct FFT-to-IFFT magnitude/phase pair is
bypassed only when neither transform has another branch or external reference.
Legacy range stored on a centred Pan is transferred to its upstream Trimesh
before the Pan is removed.

## Lifecycle And Ownership

- Cycle 1 owns `.cyc` loading and canonical export on its GUI/message thread.
- The batch driver owns only command sequencing, artifact paths, and a result
  report; it contains no preset-domain translation.
- The converter owns offline type/value/routing translation and deterministic
  `.cyclegraph` serialization.
- Cycle V2 owns load, validation, compilation, save/reload canonicalization,
  preview, and audio-render verification.

## Migration Slices

1. Added a repeatable Cycle 1 factory-library export sweep and inventoried every
   source preset against existing Cycle V2 migrations.
2. Added converter diagnostics that enumerate unsupported authored features
   without mutating the source or emitting a partial graph.
3. Added narrow mappings for Cycle V2 layer stacks, Guides, Envelopes,
   Waveshaper, drawn impulse response, group Unison, EQ, Delay, Reverb,
   oversampling, oscillator controls, and fixed modulation routing.
4. Added durable Envelope and Trilinear Mesh enablement in graph, runtime, and
   expanded-editor UI ownership.
5. Repeated conversion and Cycle V2 load/compile/save/reopen verification until
   only the time-domain layer-pan feature gap remained.
6. Recorded final counts, verification evidence, and deletion targets here and
   in `refactors.md`.
7. Codified compact non-overlapping generated layout, generalized inline Pan
   to time signals, and migrated the final three presets.
8. Restricted generated port overrides to authorable states, removed neutral
   time Pan, aligned connected ports, and added whole-library rejection of
   ordinary audio/control cable crossings through unrelated nodes.
9. Elided 79 empty additive phase layers from the regenerated graphs and routed
   volume Envelope cables monotonically into Multiply.
10. Move spectral range from Pan to Trimesh, infer magnitude mode from graph
    topology, omit every centred Pan, empty spectral Trimesh, redundant
    FFT/IFFT pair, and inactive pitch Envelope, expose range in the expanded
    Trimesh editor, and add undoable Stop Panning authoring.
11. Audited all 230 checked-in factory graphs and applied the shared conservative
    cleanup to generated and protected content. Added a whole-library invariant
    covering isolated nodes, unused Guides, empty spectral meshes, centred Pan,
    empty time seeds, direct transform round trips, loading, and compilation.

## Verification

- Every `.cyc` opens and exports through Cycle 1, or has a recorded failure.
- Every emitted `.cyclegraph` parses, validates, compiles, and survives a
  Cycle V2 save/reload cycle.
- Converter tests cover every newly translated feature family and reject
  unsupported active state.
- Representative presets receive focused state and audio assertions where a
  successful compile alone cannot prove the mapping.
- No existing migrated graph is overwritten, including local user changes.
- `git diff --check`, applicable Python tests, Cycle V2 semantic tests, and the
  standalone build pass.

## Current Inventory

- Source `.cyc` files: 229.
- Existing source-derived Cycle V2 graphs protected from regeneration:
  African Horn, Alto Sax, Baroque Flute, Stengah, and the Subbass parity graph.
- Direct canonical migrations: 229. Four early ZIP-container presets
  (`guitar-1`, `punk-2-a`, `punk-2`, and `simple-2`) are unwrapped at the file
  boundary and passed to the existing XML migrator; later header+gzip XML/JSON
  documents continue through the existing path.
- Cycle 1 live-document crashes discovered while establishing the direct
  boundary: `calming-keys` and `cluck-2`; tracked in `ui-bugs.md`.
- New checked-in migrations: 224. Together with the five protected prior
  migrations, all 229 Cycle 1 sources have a Cycle V2 destination. The bundled
  library contains 230 graphs because `spectral-reference.cyclegraph` is a
  native Cycle V2 reference rather than a Cycle 1 migration.
- Every new graph preserves all populated time and spectral meshes and all
  connected volume, pitch, and scratch Envelopes, including meaningful inactive
  layer state. The converter omitted 60 empty magnitude meshes, 79 empty phase
  meshes, 183 inactive unconnected pitch Envelopes, and 622 centred Pan no-ops.
  Fifty-one presets with no populated spectral layer omit the FFT/IFFT pair.
  Modulation blue-axis routing preserves the legacy distinction between
  velocity (input 2) and mod wheel (input 101).
- The post-migration audit removed 133 inactive, unconnected Envelope nodes and
  142 unassigned Guides. It also removed one empty spectral branch, two remaining
  centred Pan nodes, and one direct FFT/IFFT round trip. Nine wholly silent
  presets (`by-myself`, `empty`, `env-test`, `envelope-test`, `layers`, `now`,
  `power`, `sitar-model-1`, and `speed-test`) now contain only Output. Sixty-eight
  empty time meshes remain intentionally because they seed populated spectral
  layer stacks.
- Remaining blockers: none. `crash`, `cymbal`, and `downfall` retain their two
  active time layers and opposite stereo placement through the generalized
  inline Pan operation and the authoritative `Arithmetic::getPans` gain law.

## Final Verification

- Converter unit tests: 28 passed, including empty magnitude and phase meshes,
  all-empty spectral stacks, centred Pan, range ownership, and inactive pitch
  Envelope omission.
- Cycle 1 archive migration tests: 111 assertions across 5 cases passed.
- Cycle V2 layer enablement tests: 14 assertions across 2 cases passed.
- The focused layout coverage passes 308,437 assertions across 9 cases,
  covering compact node overlap and actual production cable paths across all
  224 regenerated graphs.
- Focused graph/preset coverage passes 370,085 assertions across 6 cases;
  Envelope/output-layout authoring and hit routing pass 54 assertions across 4
  cases.
- Cycle V2 Pan tests: 169 assertions across 11 cases passed, including direct
  time processing, chained oscillator rendering, and a two-update cable edit,
  commit, downstream effect, and undo sequence.
- Native Cycle V2 batch verification: all 224 generated destinations loaded,
  compiled, saved, reopened, and compiled again; 1,120 automation operations
  completed with zero failures across the batch sessions.
- The spectral Trimesh range automation fixture passed 12 commands, including
  a real drag, visible-state update, commit, and undo, and captured the expanded
  editor at `/private/tmp/cycle-v2-spectral-trimesh-range.png`.
- Post-migration simplifier and converter tests pass all 38 cases. The
  whole-library structural invariant loads and compiles all 230 presets and
  passes 5,813 assertions.
- Production-size macOS captures include the final authorable Envelope and
  operation layout in `/private/tmp/cycle-v2-layout-thrash-final.png`, and the
  empty-phase bypass plus down-left volume Envelope placement in
  `/private/tmp/cycle-v2-phase-bypass-alto-sax-1.png`.
- Standalone Cycle and Cycle V2 builds passed on macOS.
- The complete Cycle V2 binary passes 612 of 615 cases. The three unrelated
  failures are `african-horn.cyclegraph` lacking newly explicit default
  `enabled` fields and two pre-existing Stengah probe expectations for a probe
  that is no longer present in the graph.

## Completion Criteria

- All source presets are accounted for by destination graph or explicit
  blocker.
- No active authored feature is silently discarded.
- All representable presets are checked in and discoverable as factory content.
- Remaining limitations identify the missing Cycle V2 feature or precise
  defect and the authoritative implementation needed to resolve it.
- Batch/export scaffolding is either retained as a narrow reusable tool or
  removed; temporary canonical JSON and reports are not checked in.
