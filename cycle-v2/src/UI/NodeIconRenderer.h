#pragma once

#include <JuceHeader.h>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class NodeIconRenderer {
public:
    static bool hasIcon(NodeKind kind);
    static bool hasIcon(const juce::String& semanticId);
    static void paint(
            juce::Graphics& graphics,
            NodeKind kind,
            juce::Rectangle<float> area,
            float opacity = 1.f);
    static void paint(
            juce::Graphics& graphics,
            const juce::String& semanticId,
            juce::Rectangle<float> area,
            float opacity = 1.f);
};

}
