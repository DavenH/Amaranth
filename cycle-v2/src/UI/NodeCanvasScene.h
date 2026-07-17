#pragma once

#include "NodeCanvasViewport.h"
#include "../Graph/GraphEditor.h"

#include <optional>
#include <vector>

namespace CycleV2 {

enum class NodeSceneTargetKind {
    Empty,
    Edge,
    Node,
    InputPort,
    OutputPort,
    NodeAction,
    ParameterControl,
    PaletteItem,
    MiniMap,
    ExpandedEditor
};

struct NodeSceneTarget {
    NodeSceneTargetKind kind { NodeSceneTargetKind::Empty };
    juce::String semanticId;
    juce::String nodeId;
    juce::String portId;
    juce::String parameterId;
    juce::Rectangle<float> bounds;
    int edgeIndex { -1 };
    int zOrder {};

    bool isPort() const {
        return kind == NodeSceneTargetKind::InputPort
                || kind == NodeSceneTargetKind::OutputPort;
    }

    PortAddress portAddress() const {
        return { nodeId, portId, kind == NodeSceneTargetKind::InputPort };
    }
};

struct NodeSceneEdge {
    int edgeIndex { -1 };
    juce::Point<float> source;
    juce::Point<float> destination;
    juce::Path cablePath;
    juce::Path hitPath;
    bool destinationPortLike { true };
};

struct NodeCanvasSceneSnapshot {
    uint64_t graphRevision {};
    uint64_t viewportRevision {};
    uint64_t presentationRevision {};
    std::vector<NodeSceneTarget> targets;
    std::vector<NodeSceneEdge> edges;
};

class NodeCanvasScene {
public:
    const NodeCanvasSceneSnapshot& build(
            const NodeGraph& graph,
            const NodeCanvasViewport& viewport,
            uint64_t presentationRevision = 0,
            uint64_t documentRevision = 0);
    const NodeCanvasSceneSnapshot& snapshot() const { return current; }

    static juce::Point<float> portWorldCentre(const Node& node, const Port& port);
    static juce::Path cablePath(
            juce::Point<float> source,
            juce::Point<float> destination,
            PortSide sourceSide,
            PortSide destinationSide,
            float zoom);

private:
    NodeCanvasSceneSnapshot current;
};

class NodeCanvasHitTester {
public:
    std::optional<NodeSceneTarget> hitTest(
            const NodeCanvasSceneSnapshot& scene,
            juce::Point<float> screenPosition) const;
};

}
