# Cycle V1 Factory Preset Library Migration

## Status

Complete. All 229 Cycle 1 factory sources have a Cycle V2 destination. Generated
layouts use authorable port rotations, align connected ports, avoid unrelated
nodes on ordinary audio/control cable paths, and omit neutral time Pan
operations.

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

Cycle 1's `properties.active` is authored layer state, not an instruction to
delete the layer during conversion. Cycle V2 stores it as an `enabled`
parameter on each Envelope and Trilinear Mesh node and exposes that parameter
through the enable action in the expanded editor header.

The source node remains the sole durable owner. Runtime translation happens at
the existing DSP-configuration boundary:

- a disabled time-domain Trilinear Mesh publishes zero;
- a disabled spectral Trilinear Mesh remains present, while its immediately
  downstream Pan configuration derives the upstream source state and publishes
  the operation identity: zero for phase/additive magnitude and one for
  multiplicative magnitude;
- a disabled Envelope publishes its purpose identity: one for volume, normalized
  `0.5` for pitch, and zero for generic control. A disabled scratch Envelope is
  treated as unavailable by the attached Trimesh so traversal falls back to
  voice time, matching Cycle 1.

The Pan node does not gain a second independently editable enable flag. The
factory already receives the immutable graph and node ID while preparing DSP,
so this is narrow source-state translation rather than duplicated layer state.
Changing the source parameter rebuilds the immutable execution configuration;
audio processing performs no graph lookup. Once native Cycle V2 layer stacks
own a first-class layer object, this upstream inspection is a deletion target
and `enabled` moves unchanged with that owner.

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

Cycle 1 spectral `range` is already copied one-for-one to the operation and is
consumed by the shared `SpectralLayerCore`; magnitude operation mode is also
owned there. A centered spectral Pan therefore remains necessary when it owns
range shaping or additive/multiplicative semantics. A centered time Pan has no
such second responsibility and must be omitted as a graph no-op.

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
- Every new graph preserves all time, magnitude, phase, volume, pitch, and
  scratch layers, including inactive layer state. Modulation blue-axis routing
  preserves the legacy distinction between velocity (input 2) and mod wheel
  (input 101).
- Remaining blockers: none. `crash`, `cymbal`, and `downfall` retain their two
  active time layers and opposite stereo placement through the generalized
  inline Pan operation and the authoritative `Arithmetic::getPans` gain law.

## Final Verification

- Converter unit tests: 23 passed.
- Cycle 1 archive migration tests: 111 assertions across 5 cases passed.
- Cycle V2 layer enablement tests: 14 assertions across 2 cases passed.
- The focused generated-layout case passes 462,070 assertions, covering compact
  node overlap and actual production cable paths across all 224 regenerated
  graphs.
- Focused graph/preset coverage passes 462,245 assertions across 6 cases;
  Envelope/output-layout authoring and hit routing pass 54 assertions across 4
  cases.
- Cycle V2 Pan tests: 169 assertions across 11 cases passed, including direct
  time processing, chained oscillator rendering, and a two-update cable edit,
  commit, downstream effect, and undo sequence.
- Native Cycle V2 batch verification: all 224 generated destinations loaded,
  compiled, saved, reopened, and compiled again; 1,120 automation operations
  completed with zero failures across the batch sessions.
- Production-size macOS captures include the final authorable Envelope and
  operation layout in `/private/tmp/cycle-v2-layout-thrash-final.png`.
- Standalone Cycle and Cycle V2 builds passed on macOS.
- The complete Cycle V2 binary passes 602 of 604 cases. The two unrelated
  worktree failures are the protected `african-horn.cyclegraph` lacking newly
  explicit default `enabled` fields, and the pre-existing locally modified
  `stengah.cyclegraph` no longer satisfying its scratch-probe fixture. Neither
  protected graph was rewritten by this layout pass.

## Completion Criteria

- All source presets are accounted for by destination graph or explicit
  blocker.
- No active authored feature is silently discarded.
- All representable presets are checked in and discoverable as factory content.
- Remaining limitations identify the missing Cycle V2 feature or precise
  defect and the authoritative implementation needed to resolve it.
- Batch/export scaffolding is either retained as a narrow reusable tool or
  removed; temporary canonical JSON and reports are not checked in.
