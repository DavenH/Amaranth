#pragma once

#include "JuceHeader.h"

class PanelRenderContext {
public:
    juce::Rectangle<int> bounds;
    juce::Rectangle<int> clip;

    float scaleFactor = 1.f;
    bool usesCachedSurface = false;
};
