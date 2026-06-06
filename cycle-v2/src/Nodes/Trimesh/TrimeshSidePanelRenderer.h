#pragma once

#include "TrimeshNodeModel.h"

#include <JuceHeader.h>

#include <array>
#include <vector>

namespace CycleV2 {

class TrimeshSidePanelRenderer {
public:
    static void drawMorphCubePreview(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::array<float, 3>& values,
            int selectedVertexIndex);
    static void drawVertexParameters(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::vector<TrimeshVertexParameter>& parameters);

    static juce::Rectangle<float> vertexParameterRowBounds(
            juce::Rectangle<float> parameterArea,
            int parameterIndex);
    static juce::Rectangle<float> vertexParameterRailBounds(juce::Rectangle<float> parameterRow);
};

}
