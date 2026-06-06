#pragma once

#include "TrimeshNodeModel.h"

#include <JuceHeader.h>

#include <array>
#include <vector>

namespace CycleV2 {

class TrimeshSidePanelRenderer {
public:
    struct AxisState {
        juce::String label;
        juce::String shortLabel;
        juce::Colour colour;
        float value {};
        bool primary {};
        bool linked {};
    };

    static void drawSidePanel(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::array<AxisState, 3>& axes,
            const std::vector<TrimeshCubePreviewVertex>& cubeVertices,
            const std::vector<TrimeshVertexParameter>& parameters);
    static void drawMorphCubePreview(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::array<AxisState, 3>& axes,
            const std::vector<TrimeshCubePreviewVertex>& cubeVertices);
    static void drawVertexParameters(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::vector<TrimeshVertexParameter>& parameters);

    static juce::Rectangle<float> morphCubeBounds(juce::Rectangle<float> sideArea);
    static juce::Rectangle<float> morphRailBounds(juce::Rectangle<float> sideArea, int axisIndex);
    static juce::Rectangle<float> primaryAxisBounds(juce::Rectangle<float> sideArea, int axisIndex);
    static juce::Rectangle<float> linkToggleBounds(juce::Rectangle<float> sideArea, int axisIndex);
    static juce::Rectangle<float> vertexParameterPanelBounds(juce::Rectangle<float> sideArea);
    static juce::Rectangle<float> vertexParameterRowBounds(
            juce::Rectangle<float> parameterArea,
            int parameterIndex);
    static juce::Rectangle<float> vertexParameterRailBounds(juce::Rectangle<float> parameterRow);
};

}
