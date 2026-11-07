#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

struct CanvasUtilityDockLayout {
    juce::Rectangle<float> footer;
    juce::Rectangle<float> legend;
    juce::Rectangle<float> keyboard;
    juce::Rectangle<float> status;
};

class CanvasUtilityDock {
public:
    static constexpr float margin = 18.f;
    static constexpr float gap = 8.f;
    static constexpr float preferredKeyboardHeight = 56.f;
    static constexpr float preferredLegendWidth = 390.f;
    static constexpr float footerHeight = 30.f;
    static constexpr float preferredLegendHeight = 28.f;
    static constexpr float preferredStatusHeight = 28.f;

    static CanvasUtilityDockLayout layout(
            juce::Rectangle<float> contentBounds);
};

}
