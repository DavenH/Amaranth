#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

enum class NodeKind;

class GraphEndpointNodeRenderer {
public:
    static bool isConnector(NodeKind kind);
    static juce::Rectangle<float> visualBounds(
            NodeKind kind,
            juce::Rectangle<float> nodeBounds,
            juce::Rectangle<float> viewportBounds,
            float zoom);
    static void paintConnector(
            juce::Graphics& graphics,
            NodeKind kind,
            juce::Rectangle<float> nodeBounds,
            juce::Rectangle<float> viewportBounds,
            float zoom,
            bool selected);
};

}
