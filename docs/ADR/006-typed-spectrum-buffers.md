# ADR 006: Typed Spectrum Buffers For FFT Layout Safety

## Status

Proposed

## Context

Cycle uses `Transform` as a platform abstraction over real FFT operations. On
macOS, `Transform` is backed by Accelerate/vDSP. On Linux, it is backed by IPP.

Recent convolution debugging exposed that the current API hides an important
semantic difference:

- ordinary complex spectra can be multiplied bin-by-bin as normal complex
  values,
- real FFT spectra are packed, with slot 0 storing DC in the real component and
  Nyquist in the imaginary component.

Both forms are currently represented as `Buffer<Complex32>`. That makes it easy
to accidentally pass packed real FFT data into helpers that assume ordinary
complex numbers. The failure mode is subtle: gain may look roughly plausible
while phase, period, or the Nyquist/DC terms are corrupted.

The immediate regression was fixed by:

- making Accelerate `getComplex()` and `inverse(...)` use matching packed
  strides,
- keeping `NoDivByAny` unscaled and applying `1 / N` only for `DivInvByN`,
- treating the packed DC/Nyquist slot specially in convolution,
- restoring tests that verify FFT round trips and Dirac impulse-response
  convolution identity.

Those fixes are necessary, but they still rely on callers remembering the
packing convention.

## Decision

We will introduce typed spectrum buffer wrappers so FFT layout is represented in
the type system instead of as informal knowledge attached to
`Buffer<Complex32>`.

The target model is:

- `RealFftSpectrum` represents the packed real FFT layout:
  - slot `0.real`: DC,
  - slot `0.imag`: Nyquist,
  - slots `1..N/2-1`: ordinary complex bins.
- `ComplexSpectrum` represents an ordinary complex spectrum.
- `Transform::forwardReal(...)` returns or writes a `RealFftSpectrum`.
- `Transform::inverseReal(RealFftSpectrum, ...)` accepts only the packed real
  FFT type.
- arithmetic helpers are defined only for compatible spectrum types.

Raw `Buffer<Complex32>` may remain the storage primitive underneath these
wrappers, but layout-sensitive operations should move onto the typed wrappers.

## Consequences

### Positive

- Packed real FFT layout becomes explicit at compile time.
- Ordinary complex arithmetic cannot accidentally consume a packed real FFT
  spectrum.
- DC/Nyquist handling becomes centralized instead of scattered across callers.
- Platform-specific packing differences can be hidden behind wrapper methods.
- Tests can target named spectrum semantics instead of inferred buffer layout.

### Negative

- Existing APIs and call sites must be migrated gradually.
- Some low-level vector helpers may need overloads for wrapper types.
- The wrapper must stay allocation-free and cheap enough for DSP hot paths.
- During migration, both raw and typed paths may coexist, so naming and review
  discipline still matter.

## Alternatives Considered

### Continue using `Buffer<Complex32>` everywhere

Rejected.

This is the current model and it allowed packed real FFT data to be treated as
ordinary complex data. Comments and naming are not strong enough protection for
this class of bug.

### Add more comments and assertions around existing helpers

Rejected as the primary fix.

Assertions can catch some size mismatches, but they cannot distinguish
`RealFftSpectrum` from `ComplexSpectrum` when both are represented as the same
type. The layout is semantic, not just dimensional.

### Convert all FFT data to ordinary full complex spectra

Rejected for hot paths.

That would simplify arithmetic semantics, but it adds unnecessary storage and
conversion work where packed real FFT layout is the natural representation used
by the platform libraries.

## Implementation Plan

### Phase 1: Add wrapper types

- Add lightweight wrappers around `Buffer<Complex32>`:
  - `RealFftSpectrum`
  - `ComplexSpectrum`
- Keep wrappers non-owning and allocation-free.
- Expose explicit accessors for:
  - DC,
  - Nyquist,
  - ordinary complex bins,
  - underlying storage when a platform primitive requires it.

### Phase 2: Move `Transform` to typed APIs

- Add `Transform::forwardReal(...)`.
- Add `Transform::inverseReal(...)`.
- Keep the old APIs temporarily as adapters for staged migration.
- Make the old layout-sensitive APIs clearly named as legacy or internal.

### Phase 3: Migrate convolution first

- Convert `BlockConvolver` kernel and input blocks to `RealFftSpectrum`.
- Move packed real FFT multiply-add into a typed helper.
- Keep vectorized complex multiply-add for bins `1..N/2-1`.
- Keep scalar handling only for DC/Nyquist endpoint values.

### Phase 4: Migrate analysis and visualization call sites

- Audit `Spectrogram`, `VisualDsp`, pitch tracking, and any caller of
  `Transform::getComplex()`.
- Use `RealFftSpectrum` where data came from a real FFT.
- Use `ComplexSpectrum` only where bins are genuinely ordinary complex values.

### Phase 5: Remove ambiguous APIs

- Remove or restrict public APIs that return raw `Buffer<Complex32>` for
  layout-sensitive FFT data.
- Keep raw buffer access only at platform-boundary implementation sites.

## Notes For This Repository

Relevant files:

- `lib/src/Algo/FFT.*`
- `lib/src/Algo/ConvReverb.*`
- `lib/src/Algo/Spectrogram.*`
- `lib/src/Array/Buffer.*`
- `lib/src/Array/BufferMac.cpp`
- `lib/src/Array/BufferIPP.cpp`
- `cycle/src/UI/VisualDsp.cpp`

Regression tests to preserve and expand:

- `lib/tests/TestFFT.cpp`
- `lib/tests/TestConvReverb.cpp`

The Dirac impulse-response test is especially important because a Dirac kernel
should preserve both amplitude and waveform shape. It catches accidental gain
changes, packed-bin multiplication errors, and inverse layout mismatches quickly.

## Follow-Up Work

- Decide wrapper file placement, likely near `lib/src/Algo/FFT.*` unless it
  becomes broadly useful enough for `lib/src/Array`.
- Add typed helper names for packed real FFT multiply-add.
- Audit every `Transform::getComplex()` call site before removing legacy APIs.
