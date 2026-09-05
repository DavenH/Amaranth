#pragma once

#include <JuceHeader.h>

#include "UI/CanvasChromePalette.h"

namespace CycleV2::EffectPlotPalette {

inline const juce::Colour& background = CanvasChromePalette::canvasBackground;
inline const juce::Colour& insetBackground = CanvasChromePalette::insetBackground;
inline const juce::Colour grid { 0xff53616d };
inline const juce::Colour& label = CanvasChromePalette::mutedText;
inline const juce::Colour accent { 0xff43c7d0 };

inline juce::Colour forEnabledState(juce::Colour colour, bool enabled) {
    return enabled ? colour : colour.withSaturation(0.f);
}

}
