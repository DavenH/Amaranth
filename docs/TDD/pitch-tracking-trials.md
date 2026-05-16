# Pitch Tracking Trials

Scratch pad for reference-envelope validation experiments. Keep this concise and update it when a trial is accepted or rejected.

## Current Metric

- Validation compares tracked `PitchFrame` frequency to edited `.env` pitch envelopes.
- Error is reported as `rmsCents`, `meanSquaredCents`, `maxAbsCents`, and `normalizedMse`.
- Thresholds are RMS cents per reference in `lib/tests/TestPitchTracker.cpp`.

## Accepted Changes

- Auto detector selection: raised the YIN-to-SWIPE switch threshold from `2.f` average atonality to `10.f`.
  - Hypothesis: moderate YIN atonality can still be a correct fundamental estimate for bright/high material; SWIPE may octave-drop.
  - Result: `deepforest.wav` improved from about `2319.49` RMS cents to about `6.28` RMS cents.
  - Fatbass still switches to SWIPE because its YIN atonality is extreme, around `106`.

- Harmonic post-processing: preserve non-harmonic period candidates only when they are longer than the dominant period.
  - Hypothesis: downward attack bends should not be flattened to the dominant period, but too-short high-frequency blips should still be clamped.
  - Result: `fxbass.wav` improved from about `150.76` RMS cents to about `144.86` RMS cents without reference regressions.

- Onset repair after harmonic post-processing: when an initial run of high-atonality frames has been clamped to the dominant period, and the first accepted following frame has a longer period, extrapolate that longer-period attack bend backward into the clamped onset.
  - Hypothesis: dynamic downward pitch attacks can cause YIN to lock to a short high-harmonic period at the beginning; flattening those frames to the dominant period hides the onset bend.
  - Result: `fxbass.wav` improved from about `144.86` RMS cents to about `21.53` RMS cents without reference regressions.

## Rejected Trials

- Force all references through YIN.
  - Result: `deepforest.wav` improved to about `6.28` RMS cents, but `fatbass.wav` failed badly at about `4327` RMS cents.
  - Conclusion: YIN is not a universal replacement for auto-selection.

- Force all references through SWIPE.
  - Result: many references failed; `deepforest.wav` stayed about two octaves low and `fxbass.wav` worsened to about `1243` RMS cents.
  - Conclusion: SWIPE is only useful as a fallback for specific YIN failure cases.

- Preserve all non-harmonic frame periods in `fillFrequencyBins()`.
  - Result: exposed many too-short period outliers and caused large regressions in references such as `analogue2`, `deffbass`, `dist3`, `dist4`, `dist6`, `mesa1`, and `powerchord3_2`.
  - Conclusion: full preservation is too permissive; keep clamping false high-frequency blips.

## Notes

- `fxbass.wav` is no longer the largest RMS-cent error after onset repair; `dist3.wav` is now higher at about `26.71` RMS cents. `fxbass.wav` still has its max error concentrated in the attack, where the edited envelope has strong downward pitch motion.
- The direct `"PitchTracker basic functionality"` Catch2 case currently fails in several sections even with the rejected post-processing tweak backed out. It appears orthogonal to the reference-envelope trials and is not part of the `ctest -R PitchTracker` pass set.
