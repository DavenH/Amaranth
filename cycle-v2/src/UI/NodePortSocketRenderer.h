#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class NodePortSocketRenderer {
public:
    static void paint(
            juce::Graphics& graphics,
            juce::Rectangle<float> bounds,
            juce::Colour colour,
            bool input);
};

}
