# Property Slider Landmark Alignment

Status: Implemented (2026-09-05)

## Objective

Make property-slider landmarks coincide exactly with the value positions they
describe. Delay Time and Pan Cycle ticks must align with their snapped values,
including both endpoints, without stray marks outside the visible track.

## Authoritative Geometry

- JUCE `Slider::SliderLayout::sliderBounds` owns absolute pointer-to-value and
  value-to-indicator travel.
- `propertySliderTrackBounds` owns the visible Amaranth track inset.
- `PropertyControlLookAndFeel` owns property-slider painting shared by Delay,
  Reverb, Voice Context, Equalizer, IR, Waveshaper, and related inspectors.

The custom look and feel will make JUCE's slider region equal the visible
track's horizontal extent. Landmark painting will use the same
value-to-proportion mapping and slider region as the indicator. This removes
the current second inset rather than compensating for it in Delay alone.

## Test-First Contract

1. The JUCE slider region and visible property track have identical horizontal
   endpoints.
2. Values at 0, representative interior fractions, and 1 map to the same
   positions for interaction, indicator painting, and landmarks.
3. Nonlinear slider proportions are respected rather than linearly mapping raw
   values.
4. Delay Time's 2-beat landmark equals the indicator position at 2 beats.
5. All twelve Pan Cycle landmarks are monotonically and evenly distributed
   over the exact usable track, with the first and last on its endpoints.

## Negative Boundaries

- Do not add Delay-specific pixel offsets.
- Do not move labels, value fields, rows, or panel bounds.
- Do not change Delay parameter mappings, snap values, or keyboard steps.
- Do not restore a large thumb merely to satisfy JUCE's default inset.

## Completion Criteria

- Shared geometry and Delay editor regressions pass.
- A production-size Delay screenshot shows aligned ticks with no marks beyond
  the visible track.
- Standalone Debug and relevant tests build; `git diff --check`, style review,
  and production-diff review pass before commit.

## Implementation Evidence

- The shared property look and feel now gives JUCE the visible track's exact
  horizontal endpoints as its interactive slider region.
- Landmark positions use JUCE's `valueToProportionOfLength`, preserving skewed
  mappings while matching the indicator and fill boundary.
- Shared geometry tests cover endpoints, interior values, and nonlinear skew;
  Delay tests cover the 2-beat stop and all twelve Pan Cycle positions.
- The focused fixture passed. Its production-size capture is
  `/private/tmp/cycle-v2-delay-landmarks.png`.
