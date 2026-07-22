# Cycle V2 Incremental Preview Performance

## Status

Complete.

## Goal

Make live downstream feedback proportional to the affected graph slice. An
incremental edit must not copy every cached signal buffer, scan the dirty set
for every node, or force obsolete work to finish before the newest edit can
begin publication.

## Design

- Represent dirty membership as a plan-indexed byte mask built once per pass.
- Compile node-ID lookup plus both dependency directions once with the graph.
  Reuse generation-stamped closure storage and observation workspaces across
  movement edits.
- Keep cached node audio results owned by the executor and return non-owning
  immutable views to preview rendering.
- Check the source/product generation between node executions. Cancellation
  leaves the previous published cache intact and never publishes a partial
  result.
- Replace movement-time tree planning with plan-indexed reusable product slots;
  fingerprints and exactly-once edit stamps use `[node][product]` arrays.
  Strings remain at API/trace boundaries rather than as internal planning keys.
- Preserve clean node previews and render only the dirty preview step indices.
  Compile probe source step/output addresses, then publish probes with indexed
  reads rather than per-refresh node hashes and output-name searches.
- Bound preview traversal to nodes that are both downstream of the edit and
  upstream of an active probe. A probe attached before a branch terminates the
  traversal slice at that attachment.

## Verification

- A one-node dirty edit processes one node and copies zero unaffected payloads.
- Dirty membership work is linear in plan size plus dirty roots.
- Root lookup is average O(1), while dependency traversal is proportional to
  the affected slice and does not rebuild reverse edges.
- A cancellation requested after one node prevents later dirty nodes from
  executing and produces no published causal result.
- Diamond and active-probe exactly-once contracts remain green.
- One dirty preview node renders once while an unrelated cached preview remains
  byte-for-byte unchanged.

## Completion Criteria

The executor, scheduler, unit suite, and native live-edit smoke satisfy these
contracts without changing audio or preview semantics.
