# IR Prefilter DC Removal

Status: Implemented (2026-09-05)

## Objective

Make every strictly positive IR High Pass value remove the DC component from
the convolution kernel and its displayed curve. A zero-Hz setting remains an
exact pass-through, and Post Gain must not reveal or amplify residual DC after
filtering.

## Authoritative Implementation

- `CycleDsp::buildIrPrefilterLevels` owns the established cubic cutoff-to-bin
  mapping for ordinary FFT bins.
- `CycleDsp::applyIrFrequencyPrefilter` owns IR spectral filtering shared by
  Cycle 1 and Cycle V2.
- `Transform` owns the packed real-FFT representation. Its DC endpoint is
  separate from `getMagnitudes()`, and `setRemovesOffset` already provides the
  mature cross-platform DC-removal behavior.
- `ImpulseResponseAnalysis` owns Cycle V2's non-realtime audio/display filter
  preparation; Cycle 1 `IrModeller::filterImpulse` owns its corresponding
  state boundary.

The shared apply operation gains an explicit `removeDc` input. Callers derive
it from the semantic cutoff (`normalizedHighPass > 0`) rather than inferring it
from the first ordinary-bin level. This preserves sub-bin cutoff behavior:
tiny positive values remove DC without incorrectly deleting bin 1.

## Test-First Contract

1. At zero cutoff, a constant impulse remains unchanged within the existing
   FFT identity tolerance.
2. At the smallest positive normalized cutoff, the same constant impulse has
   negligible mean and amplitude even when no ordinary bin is yet removed.
3. A mixed AC/DC impulse retains its AC content but has negligible mean for a
   positive cutoff.
4. Cycle V2 applies the same rule to both its filtered audio impulse and its
   filtered display impulse.
5. Returning from a positive cutoff to zero resets the transform policy and
   restores zero-cutoff identity.

## Negative Boundaries

- Do not treat FFT bin 1 as DC or broaden the audible cutoff to encode state.
- Do not subtract a time-domain mean in the panel or after Post Gain.
- Do not change the cubic cutoff mapping, serialized values, or Post Gain.
- Do not add scalar per-sample math to DSP or visualization hot loops.

## Completion Criteria

- Shared DSP endpoint tests and Cycle V2 analysis tests pass at zero, the
  smallest positive value, and an ordinary positive cutoff.
- Cycle 1 and Cycle V2 call the shared operation with the semantic DC policy.
- Standalone Debug and tests build; the Cycle V2 suite, `git diff --check`,
  hot-loop review, style review, and production-diff review pass before commit.

## Implementation Evidence

- The shared prefilter operation now sets `Transform`'s existing DC-removal
  policy explicitly for every application, so returning to zero cutoff also
  restores exact pass-through behavior.
- Cycle 1 and both Cycle V2 impulse views derive the policy from the semantic
  condition `highPass > 0`.
- Shared tests cover zero cutoff, a sub-bin positive cutoff, mixed AC/DC input,
  an ordinary positive cutoff, and positive-to-zero reset. Cycle V2 tests cover
  both its audible and display impulses.
