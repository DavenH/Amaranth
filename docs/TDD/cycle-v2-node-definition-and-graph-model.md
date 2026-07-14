# Cycle 2 Node Definitions And Graph Model

## Status

Implemented.

The authoritative definition registry, schema-backed parameter validation,
semantic change sets, atomic graph transactions, encapsulated graph containers,
and versioned graph loading/migration are implemented in `cycle-v2/src/Graph`.
Cycle 2's existing bundled version-1 graphs remain supported.

## Implemented Decisions

- Stable node type IDs remain textual because they are persisted compatibility
  identifiers. `NodeKind` remains as an in-memory compatibility enum until the
  module-runtime and canvas TDDs migrate their closed dispatch surfaces.
- `NodeDefinitionRegistry` is the source of truth for type IDs, display
  metadata, ID prefixes, static ports, parameter schemas, natural-size policy,
  audio/preview roles, and Cycle 1 adapter references.
- Parameter instance values remain text-compatible at the document boundary,
  but all declared values are parsed, constrained, normalized, and assigned
  semantic impacts through `ParameterDefinition`. Runtime readers use the
  shared typed access layer rather than node-local parsers.
- Trimesh, Envelope, Guide, IR, and Waveshaper temporarily allow dynamic model
  parameters required by their current snapshot/edit adapters. Other node
  types reject undeclared edits. The curve-model TDD removes this escape hatch
  as typed model snapshots replace dynamic parameter keys.
- `NodeGraph` no longer exposes mutable node or edge containers. Compatibility
  editing access is by stable node identity; structural and parameter editing
  uses `GraphEditor` or `GraphTransaction`.
- `GraphTransaction` uses a validated candidate graph and commits atomically.
  Failed commands or validation leave the target graph and revision unchanged.
- Graph format version 2 stores graph and node-definition versions. Loading
  runs ordered migrations before construction, normalizes from current
  definitions, validates the completed graph, and reports structured failures.
- Static port records remain serialized during the compatibility window because
  current graphs use per-instance port domains, sides, and operation layouts.
  Definitions supply defaults and repair missing static state. Removing static
  port persistence waits for those variants to become explicit dynamic state.

This TDD is the foundation for:

- `cycle-v2-node-module-runtime.md`
- `cycle-v2-node-canvas-architecture.md`
- `cycle-v2-curve-node-models-and-editors.md`

It refines the processing-graph direction in `node-graph-workflow.md`. The
existing graph behavior remains product intent; this document changes how node
contracts and graph mutations are represented and owned.

## Problem

A Cycle 2 node's contract is distributed across `NodeKind` switches in the
graph factory, module registry, serializer, compiler, validator, runtime,
preview system, canvas, and node-specific editors. Parameters are identified by
string literals and store all values as strings. Static presentation metadata,
ports, defaults, parsing rules, and execution roles therefore have multiple
sources of truth.

`NodeGraph` also exposes mutable node and edge vectors. Callers can bypass
`GraphEditor`, validation, invalidation, undo, and compilation when changing
graph state. The graph is consequently a data container rather than an
aggregate that protects graph invariants.

These properties make ordinary node changes cross-cutting and failure-prone:

- adding a node requires edits in many unrelated switches
- parameter defaults and parsing can disagree between UI, DSP, and loading
- invalid parameter values silently coerce to zero or another fallback
- serialized labels, ports, and titles can disagree with current definitions
- graph mutations can create states that supported editor commands would reject
- change impact is inferred independently by several consumers

## Goals

- Establish one authoritative definition for every node type.
- Give parameters stable identifiers, declared types, defaults, constraints,
  and change-impact metadata.
- Keep graph node instances lightweight and independent of UI and DSP objects.
- Make `NodeGraph` the aggregate that protects structural invariants.
- Route graph changes through semantic commands or transactions.
- Produce an explicit `GraphChangeSet` for invalidation, compilation, preview,
  repaint, persistence, and undo.
- Derive static ports and presentation metadata from node definitions.
- Version graph serialization and migrate stored data before domain objects are
  constructed.
- Allow a new node definition to participate in generic graph infrastructure
  without adding another `NodeKind` switch to each consumer.

## Non-Goals

- Introduce a deep inheritance hierarchy for graph node instances.
- Make node definitions own live processor, editor, or document state.
- Make every port dynamic or every parameter polymorphic.
- Replace domain-specific configuration objects with a universal property bag.
- Change Cycle's signal-domain grammar, attachment semantics, or current graph
  workflow.
- Remove textual compatibility from the graph file format.

## Design Principles

### Composition Over A Polymorphic Node Hierarchy

`NodeInstance` is document state. A `NodeDefinition` describes the stable
contract for its type and holds factories or strategies for optional behavior.
Audio processors, preview adapters, editors, and configuration builders remain
separate objects with focused lifetimes.

Do not create a base class that combines serialization, DSP, rendering, graph
validation, and editing merely to remove switches. The registry is a
composition root, not a new god-object.

### Definitions Own Static Contract; Instances Own User State

Definitions own:

- stable type identifier and current definition version
- display name, palette category, and default instance-ID prefix
- static input and output definitions
- parameter schema and defaults
- natural-size or presentation policy
- declared capabilities and factories

Instances own:

- stable node identity
- type identity
- canvas layout
- parameter values that differ from, or explicitly override, defaults
- node-owned model snapshots such as meshes and envelopes
- genuinely dynamic port or sub-target state

Titles, labels, static ports, and default values must not have independent
copies in the serializer and graph factory.

## Core Types

Use small value types for identities so node IDs, type IDs, port IDs, edge IDs,
and parameter IDs cannot be accidentally interchanged. Their stored form may
remain a `String`.

```cpp
struct NodeDefinition {
    NodeTypeId typeId;
    int version {};
    String displayName;
    String defaultInstanceIdPrefix;
    NodePresentationDefinition presentation;
    std::vector<PortDefinition> inputs;
    std::vector<PortDefinition> outputs;
    ParameterSchema parameters;
    NodeCapabilities capabilities;
    NodeModuleFactories factories;
};

class NodeDefinitionRegistry {
public:
    const NodeDefinition* find(NodeTypeId typeId) const;
    Span<const NodeDefinition> definitions() const;
};
```

`NodeKind` may remain as a compatibility enum during migration, but generic
consumers must move to `NodeTypeId` and definitions. Once all stored graphs and
callers use stable type IDs, remove `NodeKind` or confine it to a legacy adapter.

### Typed Parameters

```cpp
using ParameterValue = std::variant<bool, int, float, ChoiceId, String,
                                    ModelSnapshot>;

enum class ParameterImpact {
    Presentation,
    Preview,
    GraphSemantics,
    DspConfiguration,
    ProcessorReset
};

struct ParameterDefinition {
    ParameterId id;
    String label;
    ParameterValue defaultValue;
    ParameterConstraint constraint;
    EnumSet<ParameterImpact> impacts;
};
```

The exact variant and constraint representation may be narrower than this
sketch. Mesh and envelope data may use typed node-model snapshots instead of a
generic `ModelSnapshot` alternative. The important contract is that parsing and
validation happen at a boundary, not repeatedly in audio, preview, and UI code.

Unknown parameters loaded from a newer format must be handled explicitly. They
must not silently become active runtime parameters. A compatibility payload may
retain them for round-tripping when required.

### Graph Aggregate

Remove mutable vector access from `NodeGraph`. Expose stable lookup and
read-only iteration, and perform mutation through commands or a transaction:

```cpp
auto edit = graph.beginTransaction(definitions);
edit.setParameter(nodeId, parameterId, value);
edit.moveNode(nodeId, bounds);
GraphEditResult result = edit.commit();
```

The transaction validates its complete candidate and either commits atomically
or leaves the graph unchanged. Compound operations such as edge splicing and
creating then attaching a guide curve are single transactions.

The aggregate enforces at least:

- unique node and edge identities
- existing node and port references
- port direction and connection multiplicity
- attachment target validity
- parameter membership, type, and constraints
- static versus dynamic port ownership
- removal of incident edges with a removed node

Graph-wide domain and cycle validation may remain in focused validator
services, but unsupported mutation paths must not bypass their required checks.

### Change Sets And Undo

Successful commits return a semantic `GraphChangeSet` containing affected node,
edge, parameter, layout, topology, and model identities plus aggregated impact
flags. Consumers use this instead of reinterpreting raw parameter strings.

Undo history stores semantic commands or before/after deltas. Full serialized
snapshots may remain as an initial adapter, but snapshot creation and restoration
belong to a graph document or command service rather than `NodeCanvas`.

## Serialization And Migration

Add an explicit root graph-format version. Deserialize into storage DTOs, run
ordered migrations, then construct and validate the domain graph through the
registry.

Persist:

- graph-format version
- node identity and stable type ID
- node-definition version when node-local migration requires it
- layout and user-authored parameter/model state
- dynamic ports or sub-targets
- edges and attachments

Derive static titles, labels, ports, palette metadata, and defaults from the
current definition. Legacy graphs that persisted these fields are accepted by a
migration but do not make them authoritative.

Loading returns structured errors for unknown types, invalid parameters,
missing ports, and failed migrations. A failed load must not partially replace
the active graph document.

## Dependency Boundaries

- `Graph/` may depend on definition interfaces and value types.
- Node definitions may describe factories through narrow abstract factory
  types, but graph model code must not include node UI or DSP implementations.
- `Runtime/` consumes compiled definitions and typed configurations.
- `UI/` consumes presentation definitions and semantic graph commands.
- `GraphSerializer` consumes storage DTOs, migrations, and the registry; it
  does not repair live presentation state itself.

## Migration Plan

### Slice 1: IDs And Parameter Access

- Add typed identity wrappers and a shared validated parameter-access layer.
- Replace local parameter parsing helpers in new code.
- Characterize existing defaults and accepted legacy textual values.

### Slice 2: Definition Registry

- Extend or replace `NodeModuleRegistry` with `NodeDefinitionRegistry`.
- Move module roles, type IDs, default ID prefixes, static ports, parameters,
  and presentation metadata into definitions.
- Make `GraphNodeFactory` construct instances from definitions.
- Add registry completeness tests before removing existing switches.

### Slice 3: Typed Parameter Values

- Convert UI and compiler reads to the schema-backed access layer.
- Build node-specific immutable DSP configurations from typed values.
- Remove parsing from audio processing and preview rendering.

### Slice 4: Aggregate Mutation

- Add graph lookup indexes and transaction support.
- Route `GraphEditor`, automation, node editors, and demo graph construction
  through transactions.
- Remove `getNodesForEditing()` and `getEdgesForEditing()`.
- Publish `GraphChangeSet` and adapt invalidation consumers.

### Slice 5: Versioned Persistence

- Introduce DTOs, graph-format versioning, and ordered migrations.
- Stop persisting definition-owned metadata in newly saved graphs.
- Preserve loading and round-trip coverage for bundled legacy graphs.

### Slice 6: Remove Compatibility Dispatch

- Remove redundant `NodeKind` and parameter switches from generic graph,
  serializer, compiler, and presentation code.
- Retain explicit switches only where the alternatives form a closed domain
  operation rather than an extensible node-type dispatch.

## Verification

### Definition Tests

- every registered type ID and default instance prefix is valid
- port and parameter IDs are unique within a definition
- every default parameter satisfies its constraint
- required capabilities have the corresponding factory
- all definitions construct a valid default node
- a test-only node definition requires no changes to generic graph services

### Graph Tests

- unsupported mutations cannot leave dangling edges or invalid parameters
- compound edits commit atomically
- failed transactions leave the graph and revision unchanged
- change sets identify topology, DSP, preview, and presentation impacts
- indexed lookup remains correct after add, remove, undo, redo, and load

### Serialization Tests

- current graphs round-trip semantically
- each supported legacy version migrates to a valid current graph
- unknown types and invalid values produce structured failures
- definition-owned metadata cannot override current definitions
- failed loading does not replace the active document

### Regression Tests

- bundled graphs compile to equivalent execution roles, domains, and routing
- current automation commands retain their observable behavior
- node defaults and natural sizes remain unchanged unless separately approved

## Completion Criteria

- There is one authoritative definition for each node type.
- Generic graph, compiler, serializer, runtime, and canvas infrastructure do
  not require per-node `NodeKind` branches.
- Runtime and UI consumers do not parse live parameter strings.
- `NodeGraph` exposes no unrestricted mutable node or edge container.
- All supported graph files pass through explicit versioned migration and
  validation.
