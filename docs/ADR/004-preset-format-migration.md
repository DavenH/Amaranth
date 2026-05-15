# ADR 004: Centralized Preset Migration And Current JSON Presets

## Status

Proposed

## Context

Cycle preset loading currently mixes three concerns:

- document container parsing,
- preset schema evolution across versions,
- module-specific state restore in `Savable::readXML(...)`.

Recent debugging showed that the current preset readers no longer align cleanly
with the old preset schema captured in
[preset-schema-v1.json](../../cycle/content/presets/preset-schema-v1.json).
Examples include:

- the old `AllMeshes` schema versus the current `MeshLibrary` group/layer model,
- legacy envelope sections versus generic mesh-library owned layers,
- old modulation mapping sections versus the current `ModMatrix`,
- guide/deformer property naming drift,
- modules such as `MorphPanel` that currently have little or no persistence logic.

At the same time, XML has become harder to inspect and evolve than necessary for
human-edited or debug-inspected presets. A more legible current format is
desirable.

The main design goal is:

- each module should only need to understand the current preset schema.

Legacy format translation should not be spread across all `Savable`
implementations.

## Decision

We will introduce a centralized preset migration layer and move the current
preset format toward JSON.

The migration boundary will live before module-level state restore. The loading
flow will become:

1. detect preset container format,
2. parse legacy XML or current JSON,
3. migrate legacy data into the current preset schema,
4. pass only current-schema data to module serializers.

The key policy decisions are:

- old presets remain loadable through a centralized `PresetMigrator`,
- new presets are written only in the current JSON format,
- module serializers target only the current schema,
- legacy XML knowledge is removed from module readers over time.

This means `Savable` implementations should eventually stop owning
version-branching logic for old preset formats. They should read and write only
the current schema for their subsystem.

## Consequences

### Positive

- Schema migration logic is centralized instead of duplicated across modules.
- Current serializers stay focused on current state shape.
- JSON presets become easier to inspect, diff, and debug than XML.
- Future schema changes can be implemented as explicit migrator steps instead of
  adding more legacy branches to every reader.
- Preset debugging becomes clearer because migration can be instrumented and
  tested independently from module restore logic.

### Negative

- `Document` loading becomes more complex because it must detect formats and run
  migration before restore.
- The project must maintain both legacy XML parsing and current JSON parsing for
  a transition period.
- Existing `Savable` XML interfaces will need a staged replacement or adapter.
- Migration correctness becomes a critical responsibility of one subsystem.

## Alternatives Considered

### Keep legacy version branches inside each `readXML(...)`

Rejected.

This makes every module responsible for old schema knowledge. It increases drift
and makes future redesign harder, especially for subsystems such as
`MeshLibrary`, modulation routing, and envelope layers where structural changes
have already occurred.

### Keep XML as the current format and migrate legacy XML to current XML

Rejected as the primary direction.

This would improve separation of concerns, but it keeps the project on a less
legible format when there is already motivation to improve preset readability.

### Convert modules directly from old XML to current in-memory state without an
intermediate migration layer

Rejected.

This recreates the same coupling problem under a different shape and prevents
clear format-version testing.

## Implementation Plan

### Phase 1: Introduce a preset migrator

- Add a `PresetMigrator` near document loading.
- Detect whether an input preset is legacy XML or current JSON.
- Add an explicit migration path from legacy XML V1 to the current schema.

### Phase 2: Define the current JSON schema

- Define the canonical current preset structure for:
  - mesh/library data,
  - panel layout state,
  - effects,
  - modulation routing,
  - morph state,
  - settings and document details.
- Keep this schema authoritative for current writers.

### Phase 3: Move module serializers to current-only schema

- Replace or adapt `Savable::readXML/writeXML` so modules consume only current
  schema data.
- Remove legacy-format branches from module readers as they become redundant.

### Phase 4: Preserve legacy import, not legacy write

- Keep the XML migrator for old preset compatibility.
- Do not emit legacy XML from current builds.
- Treat XML import as compatibility code, not as the active preset format.

## Notes For This Repository

The highest-risk migration areas are:

- `MeshLibrary`, because old named layer buckets have diverged from the current
  group/layer model,
- `Envelope2D`, because its persistence logic still reflects older envelope
  section ownership,
- `GuideCurvePanel`, because guide/deformer property names have changed,
- `ModMatrixPanel`, because modulation routing has outgrown the old compact
  schema,
- `MorphPanel`, because current persistence is incomplete and should be defined
  in the new format rather than extended in the old XML model.

The old schema in
[preset-schema-v1.json](../../cycle/content/presets/preset-schema-v1.json)
should be treated as a legacy migration reference, not as the target for new
serializers.

## Follow-Up Work

- Define the current JSON preset schema document.
- Add preset format detection in `Document`.
- Implement `PresetMigrator::migrateV1XmlToCurrentJson(...)`.
- Decide whether module serializers should move to JUCE `var`/`DynamicObject`,
  typed DTOs, or another current-schema representation.
- Add migration tests using representative old presets and current expected
  output.
