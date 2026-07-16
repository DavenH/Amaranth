# Cycle v2 Trimesh Traversal Workspace

## Status

Implemented.

## Problem

`TrimeshAudioProcessor` constructs a temporary `TrimeshGridwiseDsp`, returns a
new vector of columns, and lets every column allocate its own `SignalPayload`
buffer while processing an audio block with traversal capture enabled. The
mesh slice and waveform bake are required once per morph column, but temporary
collection and block allocation are not. The preview path then independently
renders the same grid again.

## Cost Contract

For `C` columns, `R` rows, and mesh slicing cost `S`, traversal production is
`O(C * (S + R))` because the morph position genuinely varies per column. It
performs zero allocations after `prepareExecution`, zero snapshot publications,
and no second `C * S` render for a preview consuming the same traversal result.

## Design

- Add a reusable gridwise workspace sized from `AudioExecutionSpec`.
- Render columns directly into caller-owned traversal storage; do not return a
  temporary vector of owning columns from the realtime path.
- Split stable grid preparation from the varying morph-position command.
- Let source previews consume a compatible captured audio traversal grid.
  Retain the preview rasterizer only when no audio traversal is available or a
  distinct preview resolution is explicitly requested.
- Keep E3 behavior intact: each column is a different morph cross-section and
  therefore requires its own slice/bake.

## Semantic Tests

- Allocation instrumentation reports zero prepared-path allocations at `C`,
  `2C`, and `4C`.
- Slice and bake counters equal `C`, never `1` and never `2C`.
- Audio traversal and fallback preview rendering are numerically equivalent.
- Supplying captured traversal data makes the preview perform zero mesh slices.
- Different morph columns retain their independently expected cross-sections.

## Completion Criteria

- Realtime traversal owns no temporary column collection.
- Prepared storage is reused across blocks up to the declared maximum.
- Preview reuse is explicit and tested rather than inferred from shared data.

## Implementation Notes

- `TrimeshGridwiseDsp` now prepares its rasterizer and scratch storage at the
  declared maximum, then renders each morph cross-section directly into a
  caller-owned contiguous traversal buffer.
- `TrimeshAudioProcessor` owns and reuses the gridwise DSP. It no longer builds
  a temporary vector of owning column payloads or copies those payloads into a
  second grid allocation.
- `TrimeshBlockwiseDsp` accepts `Buffer<float>` destinations, keeping the
  column loop to morph selection, slice/bake, and direct sampling.
- Captured audio traversal data is indexed by `GraphPreviewExecutor` and reused
  by the Trimesh preview when its row resolution matches the explicit preview
  request. Distinct resolutions retain the fallback renderer.
- Allocation guards cover prepared `8/16/32`-column rendering. Slice and bake
  counters remain exactly one of each per morph column, and direct output is
  compared numerically with the former owning-column API.
