#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

struct CanvasUtilityDockLayout {
    juce::Rectangle<float> minimap;
    juce::Rectangle<float> legend;
    juce::Rectangle<float> keyboard;
    juce::Rectangle<float> status;
};

class CanvasUtilityDock {
public:
    static constexpr float margin = 18.f;
    static constexpr float gap = 8.f;
    static constexpr float cornerRadius = 7.f;

    static CanvasUtilityDockLayout layout(juce::Rectangle<float> contentBounds);
    static void paintSurface(
            juce::Graphics& graphics,
            juce::Rectangle<float> bounds);
};

}
