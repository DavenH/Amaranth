# Cycle V1 Factory Preset Library Migration

## Status

Complete for every preset representable by the current Cycle V2 graph. Three
presets remain explicitly blocked on time-domain layer panning.

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
- New checked-in migrations: 221. Together with the five protected prior
  migrations, 226 of 229 Cycle 1 sources now have a Cycle V2 destination.
- Every new graph preserves all time, magnitude, phase, volume, pitch, and
  scratch layers, including inactive layer state. Modulation blue-axis routing
  preserves the legacy distinction between velocity (input 2) and mod wheel
  (input 101).
- Remaining blockers: `crash`, `cymbal`, and `downfall`. Each uses two active
  time layers hard-panned to opposite channels. Cycle V2 has spectral-layer Pan
  but no time-domain layer-pan node or parameter. Routing through the spectral
  implementation would be a domain error, so these presets are intentionally
  not emitted. The authoritative behavior is the per-layer
  `Arithmetic::getPans` mix in `SynthFilterVoice`.

## Final Verification

- Converter unit tests: 20 passed.
- Cycle 1 archive migration tests: 111 assertions across 5 cases passed.
- Cycle V2 layer enablement tests: 14 assertions across 2 cases passed.
- Cycle V2 bundled-preset tests: 420 assertions across 13 cases passed.
- Native Cycle V2 batch verification: all 221 new graphs loaded, compiled,
  saved, reopened, and compiled again; 1,105 automation operations completed
  with zero failures across seven sessions.
- Standalone Cycle and Cycle V2 builds passed on macOS.

## Completion Criteria

- All source presets are accounted for by destination graph or explicit
  blocker.
- No active authored feature is silently discarded.
- All representable presets are checked in and discoverable as factory content.
- Remaining limitations identify the missing Cycle V2 feature or precise
  defect and the authoritative implementation needed to resolve it.
- Batch/export scaffolding is either retained as a narrow reusable tool or
  removed; temporary canonical JSON and reports are not checked in.
