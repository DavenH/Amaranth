# Cycle v2 Preview Execution Index

## Status

Implemented.

## Problem

`GraphPreviewExecutor` linearly searches all prior summaries for every input and
linearly searches audio nodes and their outputs again for every preview step.
It also copies or moves complete preview vectors through intermediate summaries.
A linear execution plan can therefore acquire quadratic lookup work and avoidable
buffer traffic even though source addresses and step order are already known.

## Cost Contract

Executing `V` preview steps over `E` inputs performs `O(V + E)` address lookup
work plus the inherent cost of writing preview samples. Lookup count is
independent of summary buffer length. Each produced preview buffer has one
owner and is not copied merely to propagate its address downstream.

## Design

- Compile stable node/port addresses to indices alongside the audio plan.
- Store preview results in an indexed execution workspace.
- Pass non-owning views to downstream preview processors and move ownership
  only into the final result.
- Index captured audio outputs once at the executor boundary.
- Preserve handling for previewless passthrough steps without cloning vectors.

## Semantic Tests

- Chain, fan-out, fan-in, and previewless-passthrough graphs preserve results.
- Lookup counters scale linearly at `V`, `2V`, and `4V`.
- Buffer-copy instrumentation reports no propagation copies.
- Missing optional audio traversal inputs remain safe and explicit.

## Completion Criteria

- No `findSummary` or `findAudioOutput` linear scans remain in the execution loop.
- Preview result ownership and view lifetimes are represented by types.

## Implementation Notes

- `GraphCompiler` resolves every input to stable source-step and source-output
  indices alongside its buffer address.
- `GraphPreviewExecutor` indexes captured audio results once, then resolves
  preview and audio inputs by compiled indices without node or port scans.
- An indexed workspace propagates `PreviewResultView` aliases through
  previewless nodes. Only `GraphPreviewResult` owns produced preview vectors.
- `PreviewProcessContext` accepts an explicit non-owning `PreviewInputView`, so
  downstream processors cannot accidentally take ownership of upstream data.
- Address and alias counters enforce linear scaling through `8/16/32`-node
  previewless chains; existing spy, fan-out, and graph fixtures retain their
  semantic output checks.
