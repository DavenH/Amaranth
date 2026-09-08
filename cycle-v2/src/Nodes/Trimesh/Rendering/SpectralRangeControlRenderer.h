#pragma once

#include "Graph/NodeGraph.h"

#include <JuceHeader.h>

namespace CycleV2 {

class SpectralRangeControlRenderer {
public:
    static void draw(
            juce::Graphics& g,
            juce::Rectangle<float> row,
            juce::Rectangle<float> rail,
            PortDomain domain,
            float value);
};

}
