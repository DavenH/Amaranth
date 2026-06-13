# Envelope Loop And Sustain Semantics

Cycle-v2 envelope editing must preserve the loop/sustain behavior from the
Cycle-v1 `Envelope2D` + `EnvRasterizer` path. The important rule is that loop
and sustain are semantic markers on envelope geometry, not UI-only flags and not
stable numeric indices.

## Marker Ownership

- Loop and sustain markers are anchored to envelope line/cube identity.
- They must not be stored as "the Nth intercept" or "the Nth visible vertex" as
  the durable source of truth.
- Adding, deleting, sorting, or regenerating intercepts must not shift which
  logical point is loop or sustain.
- A selected vertex marker toggle should resolve the selected vertex to its
  owning line/cube, then mark or unmark that geometry.

Cycle-v1 stores this through `EnvelopeMesh::loopLines` and
`EnvelopeMesh::sustainLines`. The v2 model does not need to expose the same raw
types, but it needs an equivalent stable identity.

## Index Resolution

Current loop/sustain indices are derived state.

- After envelope geometry changes, regenerate intercepts.
- Then resolve the marked geometry identity back to current intercept indices.
- The resolver must run as part of the rasterizer/update path, not only when the
  user clicks the marker buttons.
- UI state, preview rendering, and audio/render sampling should all read from
  this resolved state.

In Cycle-v1 this is `EnvRasterizer::evaluateLoopSustainIndices()`, called from
`EnvRasterizer::processIntercepts()`.

## Sustain And Release Policy

- If no sustain marker resolves, sustain defaults to the final intercept.
- A sustain marker before the final intercept implies there is a release curve.
- For unipolar envelopes, a tiny post-sustain intercept is inserted for the
  release boundary:
  - copied from the sustain intercept
  - `x += 0.0001`
  - `y` clamped to at least `0.5`
  - `shp = 1`
  - no user-owned cube/line identity
- This synthetic intercept must not become a selectable user marker.

## Loop Validity

- A loop marker is valid only if it resolves before sustain with enough spacing.
- If the resolved loop/sustain indices are too close, loop playback should be
  disabled while preserving the marker identity for later edits where possible.
- The v1 minimum is `EnvRasterizer::loopMinSizeIcpts`.

## Curve Continuity

Loop behavior is a spline/curve continuity issue, not only a playback-position
wrap.

When an envelope can loop and has no release curve, normal end padding is
replaced with loop-aware padding:

- copy points immediately after the loop start
- shift those copied points right by the loop length
- use them as post-sustain curve neighbors

This lets the curve at the sustain boundary continue as if it wrapped back into
the loop section.

When playback first crosses sustain and enters looping, the render-side curve
set should be rebuilt around the loop interval:

- include points before sustain shifted left by the loop length
- include points from loop start through sustain
- include loop-back points after sustain

This gives the sampler valid neighboring points on both sides of the wrap.

## Playback State

Envelope playback has three relevant states:

- Normal: advance until sustain/release boundary.
- Looping: wrap sample position by loop length when crossing sustain.
- Releasing: on note-off, restore the non-loop waveform/curve state if needed
  and enter the release region.

On note-off, release starts from the release boundary and scales from the current
sustain level. Loop-specific render buffers should not leak into release
sampling.

## UI Expectations

- Marker buttons show whether the selected geometry is currently loop/sustain
  after resolved indices have been recomputed.
- Adding vertices inside the release region must not clamp their x coordinate to
  `1.0`; envelope x bounds can extend beyond `[0, 1]`.
- Adding vertices must not bump loop or sustain markers to neighboring points.
  If this happens, the implementation is likely using unstable index state or
  failing to re-resolve markers after edits.
- The rendered envelope preview should use the same resolved marker and curve
  state as the expanded editor.

## V2 Implementation Checklist

- Store loop/sustain anchors by stable envelope geometry identity.
- Re-resolve anchors to current intercept indices after every edit that changes
  geometry, sorting, morph projection, or rasterized intercepts.
- Keep synthetic release-boundary intercepts separate from user geometry.
- Apply loop-aware curve padding when entering loop rendering.
- Restore non-loop curve/waveform state before release rendering.
- Ensure UI marker toggles, preview images, and audio/render sampling all use
  the same resolved envelope state.

