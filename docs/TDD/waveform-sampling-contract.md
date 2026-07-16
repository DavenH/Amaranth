# Waveform Sampling Contract

## Status

Implemented.

## Problem

`WaveformSampler` entry points disagree on invalid input: they return phase,
fill `0.5`, fill zero, or rely on debug assertions. Several index-advance loops
can reach `currentIndex + 1` without a release-build bound protecting the read.
Existing adapter-parity tests often compare two paths through the same sampler
and therefore cannot establish mathematical correctness.

## Goals

- Define one waveform domain and endpoint convention.
- Define explicit results for empty, unsampleable, and out-of-domain inputs.
- Make cursor invariants and ownership explicit.
- Guarantee bounds safety in debug, release, and sanitizer builds.
- Preserve valid-input interpolation and integral sampling behavior.

## Contract

Document for every operation:

- required waveform shape and monotonicity;
- whether endpoints are inclusive;
- permitted interval ordering;
- cursor range before and after the call;
- invalid-input result and whether failure is observable;
- phase wrapping behavior.

Prefer one validated sampler object over repeated boolean `unsampleable`
parameters and unrelated static fallbacks.

## Independent Tests

Use analytically defined constant and linear waveforms, not a rasterizer adapter.
Cover:

- empty and one-point inputs;
- exact first/last endpoints and one-ULP neighbors;
- below/above-domain requests;
- zero-length destinations and zero/negative deltas;
- last valid cursor and deliberately invalid cursors;
- ordered and non-monotonic interval arrays;
- scalar, interval, evenly spaced, and area-integrated agreement.

Run the focused suite under AddressSanitizer and UndefinedBehaviorSanitizer.

## Completion Criteria

- No sampling loop depends on assertions for memory safety.
- Equivalent invalid inputs have one documented policy.
- Tests detect an off-by-one mutation at either waveform boundary.

## Implemented Contract

A sampleable waveform has at least two x values, the same number of y values,
and at least one slope per segment. Preparation is responsible for producing
strictly increasing x values; the `sampleable` value carried by `SamplerView`
is the prepared-data certificate. Sampling operations still validate buffer
shape and every requested bound needed for memory safety.

Scalar sampling uses the half-open domain `[waveX.front(), waveX.back())`.
The first endpoint is sampleable and the last endpoint is not, because the
last endpoint has no following interpolation segment. A caller-supplied cursor
may be any integer; it is normalized before access and remains in the valid
upper-endpoint range after a successful sample. Invalid scalar requests return
zero and reset the cursor to zero.

`sampleAtIntervals()` accepts a nondecreasing interval array with at least as
many values as its destination. Every consumed interval must be in the scalar
domain. Invalid input zeros the complete destination. Its cursor advancement
is bounded before every waveform access.

`sampleWithInterval()` accepts a positive delta and a sample sequence whose
first and last requested positions are in the scalar domain. An empty
destination is a no-op. Invalid input zeros the destination and returns the
unchanged phase. A successful call advances the phase by the destination size
and wraps repeatedly into the legacy upper-half-cycle convention rather than
subtracting at most one cycle.

`samplePerfectly()` accepts a positive integration width and requires every
sample's complete centered integration interval to lie inside the waveform.
It additionally requires one precomputed area per segment. `diffX` is not a
dependency of this algorithm. Invalid input zeros the destination and returns
the unchanged phase. Every `currentIndex + 1` access now has a release-build
bound.

`SamplerView` applies the same silence/unchanged-phase behavior when its
prepared waveform is marked unsampleable; callers no longer get the previous
mixture of `0.5`, input phase, zero, and assertion-only behavior.

## Implementation Evidence

- `TestWaveformSampler.cpp` uses an analytical linear waveform to cover exact
  endpoints, one-ULP neighbors, invalid cursors, ordered and non-monotonic
  intervals, zero and negative deltas, empty destinations, empty and one-point
  waveforms, scalar/bulk agreement, area-integrated values, and phase wrapping.
- The Cycle v1 IR curve-to-kernel golden samples protect valid area-integrated
  behavior independently of the sampler implementation.
- Focused debug proof: `AmaranthLib_tests '[rasterization][sampling]'` passes 37
  assertions, the envelope playback suite passes 26 assertions, and the IR
  golden test passes 10 assertions.
- The same three suites pass under AppleClang AddressSanitizer and
  UndefinedBehaviorSanitizer in `build/tests-sanitize`, with leak detection
  disabled for the JUCE test host.
- A 424-test run excluding the already documented exact-float reverb test
  initially found only the false `diffX` precondition in the IR golden test.
  After removing it, the IR golden test passes; all other 423 tests in that run
  passed.
