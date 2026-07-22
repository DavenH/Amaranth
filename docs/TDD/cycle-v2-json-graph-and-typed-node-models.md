# Cycle V2 JSON Graph and Typed Node Models

## Status

Implemented (2026-07-22).

Cycle V2 graph persistence is now canonical JSON. Nodes own immutable typed
Trimesh, Envelope, and flat-curve models; scalar controls and editor selection
remain separate state. The compiler, DSP preparation, previews, compact and
expanded editors consume those typed snapshots without reparsing graph JSON.
The XML graph APIs and model-shaped parameter codecs were deleted, all bundled
graphs were converted, and native save/reload editing is covered by the Cycle
V2 native edit smoke.

## Problem

Cycle V2 stores `.cyclegraph` files as XML while mature Amaranth `Savable`,
`Mesh`, and `EnvelopeMesh` persistence has moved to JSON. Aggregate node models
are then serialized to JSON strings and placed in generic XML parameter
attributes. A Trimesh therefore appears as:

```xml
<parameter id="mesh.topology" value="{&quot;vertices&quot;: ... }"/>
```

This has several architectural and practical failures:

- a complete model is presented as one scalar parameter;
- JSON is encoded inside XML and escaped a second time;
- one vertex edit replaces an enormous attribute and obscures useful diffs;
- model schema, validation, revision, and impact use generic parameter paths;
- `mesh.topology` is accepted through the dynamic-parameter fallback rather
  than a declared Trimesh model contract;
- graph loading produces strings that downstream code must parse again;
- labels, static ports, and other definition-owned data are duplicated in
  every graph file.

The current `with-spies.cyclegraph` demonstrates the cost. Saving materializes
two approximately 16 KB decoded mesh snapshots into XML attributes, increasing
the file from approximately 67 KB to 124 KB before accounting for whether the
session edits themselves were intended.

The completed Trimesh topology work correctly established that vertices, cube
membership, guide assignments, and gains require one authoritative durable
model. This TDD changes the ownership and representation of that model; it does
not return to sparse indexed vertex parameters.

## Goals

- Make the complete `.cyclegraph` document canonical, human-readable JSON.
- Embed mesh and curve JSON as structured values, never serialized strings.
- Separate scalar parameters, aggregate authored models, and editor intent.
- Reuse mature `writeJSON()` and `readJSON()` implementations.
- Give graph commands and update policy typed model-change information.
- Produce deterministic files with localized, reviewable diffs.
- Delete the Cycle V2 XML graph format and escaped model parameters rather
  than carrying migration or compatibility layers.

## Non-Goals

- Change `Mesh` vertex/cube semantics or invent a replacement mesh schema.
- Store rasterized grids, previews, prepared DSP buffers, or other derived
  products in graph files.
- Add compatibility for Cycle v1 preset documents through the Cycle V2 graph
  loader. Existing product-level preset migration remains a separate concern.
- Create a generic property bag that merely renames the current parameter
  string.

## Canonical Graph Document

Keep the `.cyclegraph` extension and replace its contents with one JSON object:

```json
{
  "format": "cycle-v2-graph",
  "formatVersion": 1,
  "nodes": [
    {
      "id": "waveMesh",
      "kind": "trilinearMesh",
      "definitionVersion": 1,
      "title": "Trilinear Mesh",
      "position": { "x": 144.0, "y": 215.0 },
      "parameters": {
        "yellow": 0.5,
        "red": 0.108,
        "blue": 0.0,
        "primaryAxis": "yellow"
      },
      "model": {
        "schema": "trimesh",
        "version": 2,
        "revision": 1,
        "mesh": {
          "name": "Cycle2TrimeshNode",
          "version": 2,
          "vertices": [],
          "cubes": []
        }
      },
      "editor": {
        "selectedVertexId": 0
      }
    }
  ],
  "edges": [],
  "probes": []
}
```

The exact property names may be refined during implementation, but the three
state categories and structured JSON requirement are fixed.

### Scalar parameters

`parameters` contains small typed values described by `NodeDefinition`:
booleans, integers, finite numbers, choices, and short text. JSON numbers and
booleans remain numbers and booleans; they are not converted to strings.
Labels, constraints, defaults, and impact metadata come from the definition
registry and are not persisted with each value.

Parameters are appropriate for morph positions, gains, enable states, FFT
mode, and similar independently addressable controls. They are not containers
for vertices, curves, snapshots, or nested JSON.

### Authored model

`model` is optional structured aggregate state owned by the node's domain
module. Initial model schemas include:

- `trimesh`: authoritative `Mesh::writeJSON()` state;
- `envelope`: authoritative `EnvelopeMesh::writeJSON()` state plus durable
  envelope model semantics not represented as scalar controls;
- `flatCurve`: ordered stable vertex identity and curve-domain state for
  Guide, Waveshaper, and IR nodes.

The graph writer installs the returned `var` object directly. It must not call
`JSON::toString()` on a domain model before adding it to the graph document.

### Editor state

`editor` contains durable authoring intent that is neither DSP model content
nor an automatable parameter, such as selected stable vertex identity and
domain-specific view choice. Ephemeral hover, drag, cursor, popup, and cached
render state are never saved.

Whether a field belongs in `model`, `parameters`, or `editor` is a semantic
decision made by the domain contract. It must not be chosen merely because one
existing command API is convenient.

## In-Memory Ownership

Replace model-shaped `NodeParameter` strings with an immutable domain model
interface:

```cpp
class NodeModelState {
public:
    virtual ~NodeModelState() = default;

    virtual String schemaId() const = 0;
    virtual int schemaVersion() const = 0;
    virtual uint64_t revision() const = 0;
    virtual var writeJSON() const = 0;
    virtual bool equals(const NodeModelState& other) const = 0;
};
```

`Node` owns a nullable immutable model snapshot, for example through
`std::shared_ptr<const NodeModelState>`. Concrete model state remains in the
Trimesh and curve modules; the generic graph must not gain a node-kind switch
or depend directly on live `Mesh*` ownership.

`NodeDefinition` registers a narrow model codec/factory when its node kind has
aggregate state. The codec validates JSON into a complete concrete model before
the graph accepts it. Invalid or incomplete input cannot partially mutate a
node and cannot silently fall back to a default model.

Avoid replacing one opaque string with an opaque `var` property bag. JSON is a
storage representation at the serializer boundary; domain code consumes typed
model state after load.

## Graph Commands and Change Meaning

Retain distinct command surfaces:

```cpp
setNodeParameter(nodeId, parameterId, value);
replaceNodeModel(nodeId, expectedRevision, modelSnapshot);
setNodeEditorState(nodeId, editorDelta);
```

`replaceNodeModel` is conflict-checked and undoable as one semantic command. It
emits typed model impact without pretending an entire mesh is one parameter.
The command accepts an already validated immutable model snapshot and does not
parse JSON during a live edit.

The causal update graph described by
`cycle-v2-causal-update-graph.md` consumes these distinct changes:

- scalar parameter changes use definition and contextual impact;
- authored model changes dirty model-dependent local, preview, and audio
  products;
- editor-state changes dirty only the required presentation products.

A gesture updates transient typed model state in memory and publishes one
durable model replacement at its semantic boundary. Model JSON is generated
for save/export, not for each mouse movement.

## Reuse of Mature JSON Contracts

Use `PresetJson` helpers and the existing `Savable::writeJSON()` /
`Savable::readJSON()` convention throughout.

- Trimesh embeds `Mesh::writeJSON()` directly and restores through
  `Mesh::readJSON()` during typed model construction.
- Envelope embeds `EnvelopeMesh::writeJSON()` directly and restores through
  `EnvelopeMesh::readJSON()`.
- Existing curve model codecs must expose structured `var` values rather than
  `String` snapshots.
- Graph serialization owns graph structure only; it delegates domain model
  encoding and validation to registered domain codecs.

Do not invent alternative vertex, cube, guide, gain, envelope-marker, or loop
schemas in `GraphSerializer`.

## Definition-Owned Structure

Static input/output ports, port labels, parameter labels, defaults,
constraints, subtitles, and natural node dimensions come from
`NodeDefinition`. They are reconstructed and validated on load rather than
duplicated in each preset.

Persist only authored exceptions:

- node identity, kind, definition version, title override, and position;
- scalar parameter values;
- aggregate model and editor state;
- edges and signal probes.

Dynamic ports are out of scope unless a node kind first defines a genuine
authoring contract for them. Generic dynamic parameter acceptance must not be
used for model payloads.

## Runtime Boundary

Graph loading performs JSON parsing and typed model validation once off the
audio thread. `GraphCompiler` and node configuration publishers receive typed
model snapshots and prepare immutable runtime state without reparsing JSON.

The realtime thread receives only prepared topology/configuration through the
existing immutable publication boundary. Neither graph JSON nor `var` objects
enter audio processing.

Preview and expanded-editor models observe the same immutable authored model
identity. They may build separate derived caches, but they may not reconstruct
topology from scalar parameters.

## Canonical Output and Reviewability

The writer produces deterministic pretty JSON:

- fixed top-level and node property order;
- stable graph node, edge, probe, vertex, and cube ordering;
- finite JSON numbers with one canonical numeric representation;
- a trailing newline;
- no CRLF-dependent output;
- no serialized JSON strings, XML entities, or escaped line-break entities.

Save-load-save without edits must be byte-identical. A single vertex-value
edit should change the corresponding numeric line plus the explicit model
revision, not replace an encoded document-sized string.

Compact binary or external model references are not part of this TDD. Bundled
presets may later reference immutable factory models only if portability,
identity, and override semantics receive their own design. Ordinary authored
graphs remain self-contained.

## Format Replacement

Cycle V2 is net new. Replace its XML graph format rather than adding a loader
migration layer:

- delete `toValueTree`, `loadValueTree`, `toXmlString`, and `loadXmlString`
  graph persistence paths;
- add `writeJSON`, `readJSON`, `toJsonString`, and `loadJsonString` boundaries;
- remove XML-specific load errors and introduce precise JSON/schema errors;
- update the file chooser to load `.cyclegraph` JSON rather than accepting
  `.xml` as a Cycle V2 graph;
- convert every bundled `.cyclegraph`, test fixture, and automation fixture
  directly to the new format;
- reject old Cycle V2 XML clearly rather than guessing or silently loading
  defaults.

Do not resave an arbitrary live application session as the canonical bundled
preset. Convert each reviewed committed preset state, then apply only intended
fixture changes.

## Deletion Targets

- `mesh.topology` dynamic parameter and its string codec.
- `curve.modelSnapshot` and `curve.modelRevision` parameters.
- model JSON parsing from DSP configuration and presentation refresh paths.
- XML/`ValueTree` graph serialization and graph-specific format migration.
- persisted static ports, labels, subtitles, and natural dimensions.
- tests that bless model-shaped string parameters or XML entity encoding.

The mature `Mesh` and `EnvelopeMesh` JSON implementations remain and become
more directly used.

## Verification

### Graph format

- Every bundled `.cyclegraph` parses as JSON and has the expected format ID and
  version.
- No bundled graph begins with XML or contains `&quot;`, `&#10;`, `&#13;`,
  `mesh.topology`, or `curve.modelSnapshot`.
- Typed scalar parameters round-trip as JSON booleans, numbers, and strings.
- Save-load-save is byte-identical.
- Unknown node kinds, model schemas, schema versions, malformed arrays,
  non-finite values, invalid edge addresses, and duplicate identities produce
  precise load issues without partial graph adoption.

### Typed model contracts

- Trimesh vertex identity, cube membership, guide channels, guide gains, and
  model revision round-trip exactly through graph JSON.
- Envelope mesh, loop/sustain markers, vertex identity, morph/link intent, and
  logarithmic semantics round-trip in their proper state categories.
- Guide, Waveshaper, and IR curve identity and interpolation state round-trip
  without a serialized snapshot parameter.
- One model edit produces one conflict-checked graph command and one undo
  entry.
- Invalid model JSON leaves the previously accepted model untouched.

### Runtime contracts

- DSP, preview, compact node, and expanded editor consume the same typed model
  revision after load and after undo/redo.
- Instrumentation proves zero JSON parsing during pointer movement, graph
  preview traversal, audio preparation from an already loaded graph, and
  realtime processing.
- A model edit does not force topology compilation unless graph connectivity or
  node schema changes.

### Diff and preset contracts

- A golden single-vertex edit changes only the intended structured model value
  and revision in the textual diff.
- Converting `with-spies.cyclegraph` preserves the reviewed graph nodes, edges,
  probes, scalar values, and exact mesh/curve behavior without importing
  unrelated live-session edits.
- Native macOS save/reload smoke edits Trimesh, Envelope, and a flat curve,
  then proves exact model identity and visual behavior through application
  state plus OS capture for OpenGL panels.

## Implementation Sequence

1. Introduce typed immutable node-model state and registered domain codecs
   while retaining current runtime behavior.
2. Move Trimesh and curve graph commands from model-shaped parameters to
   `replaceNodeModel` and separate editor state.
3. Add the canonical JSON graph reader/writer using `PresetJson` and direct
   domain `var` values.
4. Move compiler, DSP preparation, preview, and editor synchronization to typed
   model snapshots; remove repeated JSON parsing.
5. Convert reviewed bundled graphs and fixtures directly, then delete XML and
   escaped-snapshot paths.
6. Run persistence, semantic, causal-update, full Cycle V2, standalone, and
   native macOS smoke gates.
7. Perform the required refactor and style review before committing the
   completed TDD.

## Completion Criteria

- `.cyclegraph` is one canonical JSON format with no XML graph path.
- Aggregate node models are structured typed state, never scalar parameters or
  serialized JSON strings.
- Mature mesh/envelope JSON contracts are reused directly.
- Scalar parameters, authored models, and editor intent have distinct command,
  persistence, and invalidation boundaries.
- Presets and fixtures are converted without a Cycle V2 compatibility layer.
- Save/reload, undo/redo, runtime preparation, deterministic diffs, and native
  behavior are all proven semantically.
