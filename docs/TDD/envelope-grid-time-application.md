# Envelope Grid Time Application

## Status

Implemented.

## Problem

Two different products are currently easy to call an "envelope grid":

- a traversal grid whose columns are successive waveform cycles over time;
- Cycle v1's `E3Rasterizer` surface, whose columns are cross-sections along a
  selected morph/view axis.

Cycle v2 traversal processing already prepares its `EnvRasterizer` once and
samples positions at `1 / columns`. `EnvelopeGridRasterizer`, despite its
generic name, is currently called only by Cycle v1 `E3Rasterizer`; it changes a
morph coordinate and rasterizes once per displayed column. Those E3 renders are
required because each column is a different morph cross-section. Its problem is
hidden context and misleading naming, not asymptotic waste.

An envelope's phase axis represents time. A traversal-grid column represents a
waveform cycle at one successive time position. The envelope is therefore one
control signal `f(t)`, not a different mesh cross-section for each column.
For an `n`-column signal traversal, column `i` must be scaled by `f(i / n)`.

Cycle v1's mature behavior is authoritative: prepare the envelope once, sample
it evenly across the requested time range, then apply one sample to each
column.

## Goals

- Rasterize or prepare a signal-traversal envelope exactly once per grid
  operation.
- Sample one envelope value per grid column at the defined time cadence.
- Apply that scalar to every sample in the corresponding input column.
- Preserve unipolar/bipolar, logarithmic, sustain, loop, release, bypass, and
  inactive-envelope semantics where applicable.
- Perform no panel snapshot publication during signal-grid processing.
- Give time-control grids and morph-surface grids different types and names.
- Preserve E3's required per-morph-position cross-section renders.
- Replace `renderMesh()` with a domain name such as
  `renderCrossSectionAt(const MorphPosition&)`, taking the varying morph
  position explicitly rather than mutating it through ambient rasterizer state.

## Target Design

Separate preparation, control sampling, and grid application:

```cpp
PreparedEnvelope envelope = renderer.prepare(mesh, request);
envelope.sampleEvenly(controlValues, timeRange);
EnvelopeGridApplier().multiplyColumns(input, controlValues, output);
```

The concrete types may reuse `EnvRasterizer` or its extracted prepared-envelope
engine. They must not reproduce envelope interpolation or loop semantics.

For an `n`-column traversal grid:

```text
t(i) = i / n
output[i][j] = input[i][j] * envelope(t(i))
```

Thus the last of `n` columns samples `(n - 1) / n`; the operation does not add
an inclusive `t = 1` column.

## Semantic Tests

- A constant envelope scales every column equally.
- A linear 0-to-1 envelope produces the independently calculated value for
  each column; assertions cover first, middle, penultimate, and last columns.
- Every sample within one column receives the same envelope scalar.
- Changing row count does not change the envelope value for that column.
- Changing column count changes only the time cadence.
- Empty and single-column grids follow an explicit contract.
- Independent sampling of the prepared Cycle v2 envelope proves that traversal
  columns use `i / n`, rather than one cached phase value.
- E3 axis characterization proves each displayed column uses the requested
  morph cross-section and preserves the existing surface.
- An operation-count test expects one cross-section render per E3 morph column;
  unlike signal traversal, that work is semantically required.

## Complexity Contract

Let `R` be envelope preparation cost and `S` the total number of grid samples.
The operation is `O(R + n + S)`, with exactly one `R`. It must not be
`O(nR + S)`. Applying the already-sampled envelope is `O(S)`, which is
inherent because every output sample must be written.

## Completion Criteria

- No signal-traversal column rasterizes or republishes an envelope.
- Envelope time, mesh morph coordinates, and traversal columns have distinct
  names and types at the boundary.
- E3 cross-section rendering receives its varying morph position explicitly;
  it does not depend on a caller mutating hidden request state first.
- Tests fail if preparation is moved back inside the column loop.

## Implementation Evidence

- Cycle v2's envelope signal processor prepares one `EnvRasterizer` when its
  configuration changes, then samples that prepared rasterizer at `i / n`
  while constructing the traversal grid. `TestNodeAudioProcessor.cpp` compares
  every emitted column with an independently prepared envelope sampler and
  verifies that the scalar is repeated down the column.
- Cycle v1's former `EnvelopeGridRasterizer` is now the explicitly named
  `EnvelopeMorphSurfaceRasterizer`. Its `renderSurface()` boundary accepts the
  base morph position, selected morph axis, column count, and cross-section
  resolution. Each required E3 slice is rendered through
  `renderCrossSectionAt(mesh, morphPosition)`, with the varying position passed
  explicitly and the base position restored afterward.
- `TestEnvelopeMorphSurfaceRasterizer.cpp` covers request semantics, empty-mesh
  clearing, distinct per-axis morph slices, and morph-position restoration.
  The distinct-slice assertion fails if a single cross-section is incorrectly
  reused for the whole E3 surface.
- The Cycle v2 traversal test also changes row and column counts independently:
  row count does not alter a column's control value, while column count changes
  the cadence to the independently calculated `i / n` positions.
- Focused proof:
  `AmaranthLib_tests '[envelope-surface]'` (13 assertions) and
  `CycleV2_tests 'Envelope traversal samples phase across grid columns'`
  (47 assertions).
