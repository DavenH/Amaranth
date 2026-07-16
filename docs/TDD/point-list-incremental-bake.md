# Point-list Incremental Bake Coherence

## Status

Proposed.

## Problem

`Rasterizer2D` preserves a valuable O(1)-scope curve edit, but hand-maintains a
second waveform baking implementation. It updates curves, waveform x/y,
differences, and slopes without visibly updating every derived field, including
area and zero/one boundary indices. It also duplicates sampleability, padding,
and resort state already represented by `RenderResult`.

## Goals

- Preserve local edit complexity; do not turn vertex movement into a full sort
  or rebuild.
- Recompute every derived invariant affected by the local curve range.
- Share affected-range baking mechanics with the full waveform builder.
- Remove duplicate result status.

## Complexity Contract

Moving one existing point updates only its adjacent curves and their waveform
ranges. Cost is proportional to affected curve resolution and independent of
total point count, except when the edit explicitly crosses an ordering or
topology boundary.

## Semantic Tests

After each incremental edit, compare against an independently constructed full
rebuild for:

- waveform x/y, difference, slope, and area;
- zero/one indices and sampleability;
- samples and integrated samples;
- edits at first/final interior points;
- zero/one crossings, sharpness extremes, and resolution transitions.

Add operation counters proving no full-vector sort, serialization, or rebuild
occurs for an in-order point movement.

## Completion Criteria

- One shared affected-range baker owns all derived fields.
- `Rasterizer2D` has no parallel status scalars.
- Fault injection leaving area or an index stale fails the semantic test.

