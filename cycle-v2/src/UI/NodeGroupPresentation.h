#pragma once

#include <JuceHeader.h>

#include "Graph/NodeGraph.h"
#include "UI/NodeCanvasViewport.h"

namespace CycleV2 {

class GraphEdgeIndex;

class NodeGroupPresentation {
public:
    static juce::Rectangle<float> memberWorldBounds(
            const NodeGraph& graph,
            const NodeGroup& group);
    static juce::Rectangle<float> worldBounds(
            const NodeGraph& graph,
            const NodeGroup& group);
    static juce::Rectangle<float> toggleBounds(
            const NodeGraph& graph,
            const NodeGroup& group,
            const NodeCanvasViewport& viewport);
    static juce::Point<float> boundaryPoint(
            const NodeGraph& graph,
            const NodeGroup& group,
            juce::Point<float> other);
    static void paint(
            juce::Graphics& graphics,
            const NodeGraph& graph,
            const NodeGroup& group,
            const NodeCanvasViewport& viewport,
            const GraphEdgeIndex& edgeIndex);
};

}
