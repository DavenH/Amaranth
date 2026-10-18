#pragma once

#include <optional>
#include <vector>

#include "Graph/GraphEditTypes.h"
#include "Graph/NodeGraph.h"

namespace CycleV2 {

class GraphEdgeIndex;

struct NodeInputQuickCreateSource {
    NodeKind kind;
    String outputPortId;
    String envelopePurpose;
    String label;
};

struct NodeInputQuickCreateMove {
    String nodeId;
    Point<float> position;
};

struct NodeInputQuickCreateLayout {
    Point<float> position;
    std::vector<NodeInputQuickCreateMove> moves;
};

class NodeInputQuickCreate {
public:
    static std::optional<NodeInputQuickCreateSource> sourceFor(
            const Node& node,
            const PortAddress& destination);
    static bool canCreate(
            const Node* node,
            const PortAddress& destination,
            const GraphEdgeIndex& edgeIndex);
    static NodeInputQuickCreateLayout layoutFor(
            const NodeGraph& graph,
            const Node& destinationNode,
            const PortAddress& destination,
            const NodeInputQuickCreateSource& source);
    static Point<float> socketWorldCentre(
            const Node& node,
            const PortAddress& destination);
    static Rectangle<float> badgeBounds(Point<float> socket, float zoom);
};

}
