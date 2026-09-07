# Cycle V1 Factory Preset Library Migration

## Status

In progress.

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

## Lifecycle And Ownership

- Cycle 1 owns `.cyc` loading and canonical export on its GUI/message thread.
- The batch driver owns only command sequencing, artifact paths, and a result
  report; it contains no preset-domain translation.
- The converter owns offline type/value/routing translation and deterministic
  `.cyclegraph` serialization.
- Cycle V2 owns load, validation, compilation, save/reload canonicalization,
  preview, and audio-render verification.

## Migration Slices

1. Add a repeatable Cycle 1 factory-library export sweep and inventory every
   source preset against existing Cycle V2 migrations. Complete: 225 of 229
   decode to canonical preset objects through the direct migration boundary.
2. Add converter diagnostics that enumerate every unsupported authored feature
   without mutating the source or emitting a partial graph.
3. Port and verify the currently representable subset.
4. Add narrow mappings for already-supported Cycle V2 node features where the
   Cycle 1 semantics are unambiguous, with focused converter tests.
5. Repeat conversion and Cycle V2 load/compile verification until remaining
   presets are genuinely feature-blocked or defective.
6. Record every remaining blocker, final counts, verification evidence, and
   deletion/stable-state review here; commit each coherent slice.

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
- Direct canonical migrations: 225.
- Decode failures: `guitar-1`, `punk-2-a`, `punk-2`, and `simple-2`. Their
  payloads are not accepted by the current gzip/XML-or-JSON document reader and
  require format identification before migration.
- Cycle 1 live-document crashes discovered while establishing the direct
  boundary: `calming-keys` and `cluck-2`; tracked in `ui-bugs.md`.
- Layer enablement is durable authored state on time, magnitude, phase, and
  Envelope layers. Cycle V2 currently has effect- and Guide-specific enable
  parameters but no general Trimesh or Envelope enable/bypass contract. Do not
  discard inactive authored layers; define their graph/runtime/UI destination
  before those presets are considered migrated.

## Completion Criteria

- All source presets are accounted for by destination graph or explicit
  blocker.
- No active authored feature is silently discarded.
- All representable presets are checked in and discoverable as factory content.
- Remaining limitations identify the missing Cycle V2 feature or precise
  defect and the authoritative implementation needed to resolve it.
- Batch/export scaffolding is either retained as a narrow reusable tool or
  removed; temporary canonical JSON and reports are not checked in.
