# Deterministic Guide-curve Seed Ownership

## Status

Implemented, including the Cycle v2 audio lifecycle boundary.

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
- Cycle v2 note events mix their producer-captured clock timestamp with the
  queue sequence. The allocated voice carries that seed into Trimesh blockwise
  and gridwise rasterization; the audio thread does not read the wall clock.
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

## Implementation Notes

- `GuideCurveSeed` distinguishes stable visualization identity from explicit
  voice-lifecycle entropy. Rasterizers derive offsets from that value with a
  deterministic integer mixer and never read wall-clock time.
- Cycle v1 visualization call sites use stable purpose/layer identities.
  Audio voices supply their own lifecycle seeds when notes and voice caches are
  initialized, preserving intentional variation without hiding ownership in a
  rasterizer.
- Cycle v2 preview call sites retain stable domain identities, while every
  allocated note voice receives event-time lifecycle entropy. Repeated renders
  within one voice are stable, and note retriggers receive distinct offsets.
- Envelope one-sample-per-cycle playback derives per-voice offsets from the
  same explicit lifecycle seed contract.
- Guide-curve noise storage and missing/persisted-seed fallbacks are stable by
  guide index. New documents therefore remain reproducible, and saved guide
  seeds retain their existing authority.
- Repeated bongo preset loads were captured through the macOS OS-capture panel
  workflow. All eight waveform, spectrum, guide, and waveshaper crops were
  pixel-identical (`inf` PSNR), exceeding the 48 dB gate. Artifacts were
  written to `/tmp/cycle-panel-seed-a` and `/tmp/cycle-panel-seed-b`.
- CMake source discovery now uses `CONFIGURE_DEPENDS`, preventing newly added
  rasterizer implementation files from silently appearing only in a freshly
  configured test build while remaining absent from an older standalone
  build tree.
