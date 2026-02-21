# ADR-002 V2 Parity Checkpoint

Date: 2026-02-21

## Non-Negotiable Rule

- V2 must preserve legacy behavior unless explicitly approved otherwise.
- No heuristic substitutions for legacy logic in parity paths.

## Current Status

- V2 is integrated as default path in key rasterizers and builds/tests pass.
- Full `Cycle` target builds successfully.
- V2 test suite currently passes.

## Critical Parity Gap (Open)

### Voice chaining parity is not complete

- `V2VoiceRasterizer` does **not** yet fully reproduce `cycle/src/Curve/VoiceMeshRasterizer.cpp::calcCrossPointsChaining()`.
- Existing V2 voice chaining implementation is still an approximation relative to legacy call-count/front-back carry semantics.

## What Must Be Ported for Voice Parity

1. Legacy chaining state machine behavior:
   - first-call behavior vs subsequent-call behavior,
   - advancement semantics,
   - carry of prior intercept sets.
2. Legacy front/back chained padding construction:
   - equivalent front/back intercept derivation policy,
   - equivalent curve triplet assembly order.
3. Legacy boundary continuity characteristics:
   - block-to-block continuity under evolving mesh/morph states.

## Mandatory Validation for Voice

1. Add parity tests directly comparing legacy voice chaining outputs vs V2 for same fixtures:
   - same mesh,
   - same morph evolution sequence,
   - same phase progression.
2. Validate:
   - phase wrap behavior,
   - cycle boundary continuity,
   - block concatenation equivalence where legacy guarantees it.

## Guardrail for Future Changes

- Any V2 voice chaining change must reference this checkpoint and state:
  - which legacy behavior is being matched,
  - which test proves parity for that behavior.
