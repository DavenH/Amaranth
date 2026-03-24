# Rasterization V2 Status and Migration Plan

## Status
Active

## Date
2026-03-12

## Supersedes
- 2026-02-15 proposed pipeline plan in this file

## Decision Summary
The V2 rasterization pipeline is no longer a proposed architecture project. The staged pipeline, workspace model, and concrete V2 rasterizers already exist in the codebase and have meaningful test coverage.

The remaining work is narrower and more concrete:
- close parity gaps against legacy rasterizer behavior,
- remove render-path allocation hazards,
- migrate production call sites that still depend on legacy rasterizers,
- delete legacy code only after parity is enforced by tests.

## Non-Negotiable Rules
1. V2 must preserve legacy behavior unless an explicit product or DSP decision says otherwise.
2. No heuristic substitutions are allowed on parity paths.
3. Any change to V2 behavior must identify the legacy behavior it matches or intentionally diverges from.
4. Every rasterizer must first satisfy its own behavioral contract before it can be used as a migration oracle.
5. Legacy-equivalence tests are the primary migration gate only when the V2 and legacy rasterizers are implementing the same product contract.
6. A V2 path is not complete because it is architecturally cleaner; it is complete when it is behaviorally equivalent where required and realtime-safe.

## Current State
The following parts of the original refactor plan are already implemented in substance:
- shared V2 rasterizer pipeline and artifact API,
- preallocated workspace for intercepts, curves, wave buffers, and deform regions,
- concrete V2 graphic, FX, env, and voice rasterizers,
- dedicated env state machine,
- stage-level and rasterizer-level tests for determinism, capacity stability, and selected parity properties.

This means the main question has changed from "what should the architecture be" to "what evidence is still required before legacy paths can be removed".

## Rasterizer Behavioral Contracts
Different rasterizers do not share the same boundary policy. Tests must reflect the actual product role of each rasterizer rather than assuming that every rasterizer is a direct oracle for every other one.

### Voice
Product role:
- audio-rate runtime waveform generator,
- stateful across consecutive cycles,
- expected to support morphing and mesh evolution while audio is running.

Boundary policy:
- successive cycles must chain continuously,
- the first call may be a priming step,
- steady-state chained behavior is the important contract.

Hard invariants:
- no NaNs or infinities,
- no invalid phase values,
- no discontinuous step introduced at the chained cycle boundary,
- stable output under small state progression.

### Graphic
Product role:
- visual cross-section of a cycle-like signal for editors and displays,
- intended to resemble the voice shape without duplicating the audio runtime policy exactly.

Boundary policy:
- may enforce a self-cyclic closure on the current cycle instead of chaining to prior and next cycles,
- seam behavior is acceptable if the rendered display remains internally coherent.

Hard invariants:
- no NaNs or infinities,
- visually continuous within the rendered cycle,
- cyclic closure is internally consistent,
- sample extraction and curve assembly stay deterministic.

### Env
Product role:
- non-periodic control-signal renderer with attack, loop, sustain, and release semantics,
- stateful across note-on and note-off transitions.

Boundary policy:
- not cyclic,
- continuity depends on envelope state and mesh shape rather than phase wrapping,
- corners are allowed when the envelope shape implies them.

Hard invariants:
- no NaNs or infinities,
- valid state transitions,
- correct sustain and loop semantics,
- release starts from the carried state that the product expects.

### FX
Product role:
- general curve renderer for non-voice uses where padding is the main domain extension policy.

Boundary policy:
- no audio-style chaining requirement,
- no self-cyclic closure requirement unless a specific caller opts into it.

Hard invariants:
- no NaNs or infinities,
- sorted and sampleable output,
- padding does not create invalid geometry.

## What Is Already Working
### Implemented V2 structure
The codebase already has the staged pipeline described by the original ADR:
- `V2RasterizerPipeline`
- `V2RasterizerWorkspace`
- interpolator, positioner, curve builder, wave builder, and sampling stages
- `V2GraphicRasterizer`
- `V2FxRasterizer`
- `V2EnvRasterizer`
- `V2VoiceRasterizer`

### Existing coverage that should be preserved
Current tests already cover important pieces of the migration:
- stage behavior and deterministic sampling,
- workspace-capacity stability across repeated renders,
- voice chaining intercept sequencing parity against a legacy oracle,
- env state transitions and rendered behavior,
- FX and graphic artifact extraction behavior.

These tests are valuable, but they are not yet the full acceptance bar for migration.

## Remaining Risks
### 1) Voice waveform parity is not fully proven
V2 currently proves intercept sequencing parity for voice chaining and basic continuity properties. That is necessary but not sufficient.

What is still missing:
- rendered-block parity against the actual legacy `VoiceMeshRasterizer`,
- parity for first-call versus subsequent-call padding behavior,
- parity for non-zero `MinLineLength` advancement across chained renders,
- parity under evolving morph and mesh states across consecutive blocks.

Reason this remains open:
- legacy voice chaining builds front and back padding with more stateful logic than the current V2 curve-padding path,
- current tests do not yet assert end-to-end rendered output equivalence against the legacy voice renderer.

### 2) Env and waveform deformer parity is only partially enforced
V2 env and deformer behavior is substantially implemented, and there is a useful parity debug harness. But the strongest env parity test still behaves like an inspection aid rather than a hard gate.

What is still missing:
- numeric assertions for legacy-vs-V2 output deltas in attack, loop, and release phases,
- explicit parity tests for component deformers in both coupled and decoupled modes,
- parity coverage for `calcWaveform`-derived behavior and `sampleAtDecoupled` behavior using legacy rasterizers as the oracle.

### 3) Realtime safety is not yet fully closed
The workspace is preallocated, but that does not mean the full render path is allocation-free.

Known remaining concerns:
- lazy transfer-table initialization,
- temporary `std::vector` allocations inside stage `run()` functions,
- lack of a hard allocation detector around full render paths.

The bar is stricter than "workspace capacities do not grow". The bar is "rendering performs no heap allocation after prepare".

### 4) Production migration is incomplete
Graphic and FX paths already route through V2-backed facades. Voice audio rendering still depends on legacy `VoiceMeshRasterizer`. Env logic has V2 implementation and tests, but migration is not yet complete at all call sites.

## Acceptance Criteria
A rasterizer path is ready to replace its legacy counterpart only when all of the following are true:
1. The V2 path passes contract tests for finiteness, domain validity, determinism, and boundary policy.
2. The V2 path passes deterministic and capacity-stability tests.
3. The V2 path passes legacy-equivalence tests for rendered output when the legacy rasterizer implements the same contract.
4. The V2 path performs no heap allocation after `prepare(...)`.
5. The production call site can switch to V2 without requiring fallback for normal operation.

## Required Test Strategy
### Priority Rule
For the rest of this migration, start by asserting each rasterizer's own behavioral contract. Then add direct V2-versus-legacy tests where the legacy rasterizer is implementing the same contract.

Contract tests keep invalid implementations from being treated as oracles. Legacy-equivalence tests are still the main migration proof, but only after the oracle has passed its own contract checks.

### Required test categories
1. Contract tests
   - Assert no NaNs or infinities.
   - Assert valid phase, ordering, and state invariants.
   - Assert boundary continuity where the rasterizer contract requires it.
   - Assert that priming behavior and steady-state behavior remain internally coherent.

2. Legacy-equivalence render tests
   - Compare full output buffers from legacy and V2 for the same mesh, controls, morph progression, and phase progression.
   - Use legacy rasterizer classes as the oracle only when they pass the corresponding contract tests.

3. Legacy-equivalence artifact tests
   - Compare intercept sets, curve assembly behavior, zero and one index behavior, and deform-region behavior where those artifacts are part of the legacy contract.

4. Realtime guard tests
   - Assert no allocations during `renderIntercepts`, `renderWaveform`, and `renderBlock` after preparation.

5. Determinism and state-progression tests
   - Repeated runs with identical state produce identical output.
   - Stateful transitions produce the same sequence of outputs as legacy across consecutive calls.

### Same-role parity rule
Do not use cross-role parity at policy boundaries.

Examples:
- `V2VoiceRasterizer` should be compared against `VoiceMeshRasterizer` for chained runtime behavior.
- `V2EnvRasterizer` should be compared against `EnvRasterizer` for attack, loop, sustain, and release behavior.
- graphic renderers should not be required to match voice chaining at the phase seam, because they intentionally apply a different closure policy.

### Required legacy-oracle coverage by subsystem
#### Voice
Add or strengthen tests that compare V2 against `VoiceMeshRasterizer` for:
- legacy contract sanity: finite rendered output and valid chained phases before parity deltas are asserted,
- first chained render versus second and later renders,
- non-zero `MinLineLength` advancement,
- cycle-boundary continuity,
- evolving morph position across consecutive blocks,
- phase wrap behavior with chained rendering,
- padding edge cases near domain boundaries.

#### Env
Add or strengthen tests that compare V2 against `EnvRasterizer` for:
- legacy contract sanity: finite attack, loop, and release buffers before parity deltas are asserted,
- attack progression,
- loop entry and repeated loop traversal,
- note-off transition,
- release traversal,
- no-release envelopes,
- one-sample-per-cycle and decoupled sampling behavior if that mode remains part of the product surface.

#### Graphic and FX
Keep parity tests against legacy rasterizers for:
- intercept extraction,
- curve padding policy,
- sample buffer output where V2 is now the primary path.

Also keep contract tests that assert the product-facing policies those parity tests do not cover:
- graphic self-cyclic closure behavior,
- visual continuity within the rendered cycle,
- FX finite padded output and sorted sampling domain.

#### Deformers
Add direct legacy-oracle tests around `MeshRasterizer::calcWaveform` behavior for:
- component deformers in coupled mode,
- component deformers in decoupled mode,
- sampler-time deform-region application,
- path-aware amplitude, phase, and curve deformation cases that legacy guarantees.

## Revised Implementation Plan
### Stage A: Replace proposal-era ADR text with a status-driven plan
- Treat pipeline architecture as established.
- Record what is complete, what remains open, and what evidence is required.
- Stop framing the main work as contract invention.

Exit criteria:
- ADR accurately describes the current codebase.
- Open items are testable migration tasks rather than abstract architecture tasks.

### Stage B: Install hard legacy-equivalence gates
- Convert current parity debug coverage into assertive tests with tolerances.
- Add voice rendered-output parity tests against `VoiceMeshRasterizer`.
- Add explicit deformer parity tests using legacy waveform generation and sampling as the oracle.

Exit criteria:
- Each remaining parity claim is backed by a failing-or-passing test rather than a comment or manual inspection step.

### Stage C: Remove render-path allocations
- Move any per-render temporary storage into `V2RasterizerWorkspace` or equivalent prepared state.
- Eliminate lazy initialization from render code paths.
- Add debug allocation instrumentation for end-to-end render functions.

Exit criteria:
- After `prepare(...)`, no heap allocation occurs during V2 rendering in debug instrumentation.

### Stage D: Migrate production voice and env call sites
- Introduce V2-backed facades or shadow-mode comparison where useful.
- Switch audio voice rendering away from legacy `VoiceMeshRasterizer` only after parity and allocation gates are green.
- Switch remaining env call sites only after equivalent proof exists.

Exit criteria:
- Normal production rendering no longer depends on legacy voice or env rasterizer implementations.

### Stage E: Delete legacy rasterization paths
- Remove compatibility glue and dead code only after migration gates pass.
- Keep characterization fixtures that continue to provide regression protection.

Exit criteria:
- Legacy rasterizer classes are removed from active production use.
- Remaining tests are V2-native, with preserved fixtures or oracle snapshots where appropriate.

## Rules For Future Changes
Any future change to V2 rasterization must state:
- which legacy behavior is being matched, preserved, or intentionally changed,
- which test proves that claim,
- whether the change affects render-path allocation behavior,
- whether the change affects state progression across consecutive renders.

If a change touches DSP, waveform generation, or sampling behavior, the pull request summary should lead with parity evidence, not architecture discussion.

## Immediate Next Steps
1. Add legacy-only contract tests that prove `VoiceMeshRasterizer` and `EnvRasterizer` produce finite, domain-valid output before treating them as parity oracles.
2. Keep the env parity test as a hard assertion-based legacy-equivalence test once the legacy contract failures are made explicit.
3. Add rendered-buffer parity tests comparing `V2VoiceRasterizer` directly against `VoiceMeshRasterizer`.
4. Add allocation tripwires for all V2 render entry points.
5. Remove stage-local render allocations by moving scratch storage into prepared workspace.
6. Only then begin swapping remaining production voice and env call sites to V2.

## Explicit Non-Goals
The next phase is not:
- introducing another abstract rasterizer architecture,
- replacing legacy behavior with approximations for convenience,
- deleting legacy code before equivalent behavior is proven,
- treating intermediate-artifact parity as a substitute for rendered-output parity.

## Outcome
The refactor direction was broadly right, but the remaining work is not a greenfield redesign. It is a migration and verification program.

From this point forward, the project should optimize for proof:
- proof of behavioral equivalence,
- proof of realtime safety,
- proof that production call sites can leave legacy rasterizers behind.
