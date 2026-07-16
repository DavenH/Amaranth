# Deterministic Guide-curve Seed Ownership

## Status

Proposed.

## Problem

Rasterizers create guide-curve offsets from wall-clock milliseconds. Identical
documents therefore render different visualization geometry depending on call
timing. Consecutive bongo panel captures differ materially in waveform and
spectrum panels, making the documented PSNR regression gate ineffective.

## Goals

- Give every seed an explicit semantic owner.
- Make document and panel visualization reproducible.
- Preserve intentional per-voice variation where required.
- Remove wall-clock reads from rasterizer domain code.

## Seed Policy

- Visualization seeds derive from stable document/layer/guide identity or
  persisted state.
- Voice seeds are supplied by the voice engine and scoped to the intended note
  or voice lifecycle.
- Tests supply ordinary production seed inputs; no test-only randomness branch
  is permitted.

## Semantic Tests

- Repeated rendering of one document produces identical results and OS panel
  captures.
- Save/reload preserves visualization output.
- Reordering unrelated renders does not change a layer's visualization.
- Two intentionally distinct voices receive the specified distinct seeds.
- No rasterizer source references wall-clock time.

## Completion Criteria

- Dynamic rasterizer panel baselines satisfy the established PSNR threshold
  across consecutive runs.
- Seed derivation and lifecycle are documented at every call site.

