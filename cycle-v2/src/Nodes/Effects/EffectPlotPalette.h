#pragma once

#include <JuceHeader.h>

namespace CycleV2::EffectPlotPalette {

inline const juce::Colour background { 0xff101318 };
inline const juce::Colour insetBackground { 0xff0b0f14 };
inline const juce::Colour grid { 0xff53616d };
inline const juce::Colour label { 0xff8793a1 };
inline const juce::Colour accent { 0xff43c7d0 };

inline juce::Colour forEnabledState(juce::Colour colour, bool enabled) {
    return enabled ? colour : colour.withSaturation(0.f);
}

}
