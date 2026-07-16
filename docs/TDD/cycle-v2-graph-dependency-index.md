# Cycle v2 Graph Dependency Index

## Status

Proposed.

## Problem

Graph invalidation recursively scans every signal edge and attachment for each
visited node, while duplicate detection linearly scans the growing output.
Invalidating a chain can therefore cost `O(V * E + V^2)` even though the
compiled plan already defines a stable dependency graph.

## Cost Contract

Invalidating from one or more roots visits each reachable node and outgoing
dependency at most once: `O(Vr + Er)`. Output order remains deterministic and
compatible with execution order.

## Design

- Compile signal and attachment adjacency into the execution plan.
- Give compiled nodes stable indices.
- Traverse with an indexed visited bitmap or generation marks.
- Produce audio and preview invalidation sets from the same traversal while
  respecting the requested parameter impacts.
- Rebuild the index only when graph semantics change, not for ordinary value
  edits.

## Semantic Tests

- Signal, attachment, fan-out, converging, and disconnected graphs retain
  current invalidation semantics.
- Every reachable edge is inspected once despite converging paths.
- Scaling counters are linear for chains and dense fan-out fixtures.
- Parameter-only edits do not rebuild the dependency index.

## Completion Criteria

- Invalidation contains no recursive full-edge scans or linear duplicate checks.
- The compiler owns dependency indexing and its revision lifecycle.

