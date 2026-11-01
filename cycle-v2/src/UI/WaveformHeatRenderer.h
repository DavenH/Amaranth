#pragma once

#include <JuceHeader.h>
#include <UI/Panels/ZoomRect.h>

namespace CycleV2 {

bool paintWaveformHeatOpenGL(juce::Graphics& graphics, const juce::Image& coverage,
        const juce::Image& heat, const ZoomRect& coverageViewport, const ZoomRect& viewport,
        juce::Rectangle<float> bounds, juce::Rectangle<float> deviceBounds);

}
