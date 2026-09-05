#pragma once

#include <JuceHeader.h>

#include "UI/CanvasChromeMetrics.h"

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
    static constexpr float preferredKeyboardWidth = 276.f;
    static constexpr float preferredKeyboardHeight = 112.5f;
    static constexpr float preferredLegendHeight =
            98.f * CanvasChromeMetrics::legendScale;
    static constexpr float minimumCompactLegendHeight =
            30.f * CanvasChromeMetrics::legendScale;

    static CanvasUtilityDockLayout layout(juce::Rectangle<float> contentBounds);
    static void paintSurface(
            juce::Graphics& graphics,
            juce::Rectangle<float> bounds);
};

}
