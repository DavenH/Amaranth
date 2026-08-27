#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

struct OutputMeterLayout {
    juce::Rectangle<float> left;
    juce::Rectangle<float> right;
};

class OutputMeterPresentation {
public:
    static OutputMeterLayout layout(juce::Rectangle<float> area);
    static juce::Rectangle<float> fillBounds(
            juce::Rectangle<float> channelBounds,
            float level);
    static void paint(
            juce::Graphics& graphics,
            juce::Rectangle<float> area,
            float leftLevel,
            float rightLevel,
            juce::Colour colour);
};

}
