#pragma once

#include <cstdint>

#include "JuceHeader.h"

class PanelRenderContext {
public:
    juce::Rectangle<int> bounds;
    juce::Rectangle<int> clip;

    uint32_t dirtyMask = 0;
    int panelId = -1;
    float scaleFactor = 1.f;
    bool usesCachedSurface = false;
};
