# Cycle 2 Curve State Publication

## Status

Proposed.

Depends on `cycle-v2-curve-identity-and-edit-commands.md` and
`cycle-v2-node-definition-and-graph-model.md`.

## Problem

Curve editors currently publish named controls through several individual
parameter commands and then publish the model snapshot separately. Compound
undo groups those mutations, but graph observers and compilation scheduling
can still see intermediate combinations. A different snapshot can also reuse
the current model revision because only lower revisions are rejected.

Typed snapshot decoding falls back to legacy data when a typed field is
present but malformed, which can conceal document corruption.

## Goals

- Publish the complete state of one curve node as one graph mutation.
- Give every accepted content revision an unambiguous payload.
- Make retries idempotent and stale/conflicting writes detectable.
- Schedule compilation, preview refresh, and repaint once per committed edit.
- Distinguish absent legacy state from invalid typed state.

## Non-Goals

- Define curve identity or interaction behavior.
- Replace graph-wide immutable DSP configuration publication.
- Add collaborative multi-user editing.

## State Contract

Use a node-kind-specific state value containing the curve snapshot and named
controls. A representative command is:

```cpp
struct CurveNodeStatePublication {
    juce::String nodeId;
    uint64_t expectedRevision {};
    uint64_t revision {};
    juce::String modelSnapshot;
    std::vector<NodeParameterValue> controls;
};
```

The dispatcher validates node kind, parameter schema, snapshot type, embedded
revision, and the expected current revision before mutating the graph. It then
applies controls, snapshot, and revision in one `GraphEditor` operation and
emits one consolidated `GraphChangeSet`.

## Revision Rules

- A content-changing publication must use a revision greater than the current
  revision.
- An equal revision is accepted only when the complete canonical state is
  identical; this is an idempotent retry and creates no undo entry.
- An equal revision with different content is a conflict.
- A lower or unexpected base revision is stale.
- Loading and migration establish a revision without simulating user edits.

Use explicit result codes for stale revision, conflicting revision, invalid
typed snapshot, wrong node kind, and invalid control value.

## Typed And Legacy Decoding

- If typed state is absent, migrate a supported legacy payload.
- If typed state is present but invalid, reject it and report a diagnostic.
- Do not silently substitute legacy state for malformed typed state.
- Canonical serialization is deterministic so idempotence comparisons are
  meaningful.

## Migration Plan

### Slice 1: Publication Value And Validation

- Introduce typed publication values and result codes.
- Add canonical state validation per curve node kind.
- Add conflict and idempotence tests.

### Slice 2: Atomic Graph Command

- Add one `GraphEditor`/dispatcher mutation for complete curve-node state.
- Consolidate change impacts and command history.
- Remove sequential parameter-plus-model publication from editors.

### Slice 3: Scheduling And Migration

- Schedule one compile/preview/repaint action per successful publication.
- Tighten typed-versus-legacy decoding.
- Preserve supported saved graph migration.

## Verification

- Observers never receive a graph revision containing mixed old/new curve
  state.
- One drag gesture creates one undo entry and one compiled-state refresh.
- Undo and redo restore controls and curve geometry together.
- Same-revision/same-state retry is a no-op.
- Same-revision/different-state is rejected.
- Stale base revisions are rejected without mutation.
- Invalid typed state is diagnosed rather than hidden by legacy fallback.
- Legacy-only saved graphs migrate to equivalent typed state.

## Completion Criteria

- Editors invoke one semantic publication command per committed gesture.
- Curve controls, snapshot, and revision change atomically.
- Revision equality cannot identify different content.
- Typed corruption is observable and cannot silently activate legacy data.

