# Waveform Sampling Contract

## Status

Proposed.

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

