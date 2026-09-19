#pragma once

#include <unordered_map>
#include <vector>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class GraphEdgeView;

class GraphEdgeIndex {
public:
    explicit GraphEdgeIndex(const std::vector<Edge>& edges);
    explicit GraphEdgeIndex(const GraphEdgeView& edges);

    const std::vector<size_t>& edgesToInput(
            const String& nodeId,
            const String& portId) const;
    const std::vector<size_t>& incomingEdges(const String& nodeId) const;
    const std::vector<size_t>& outgoingEdges(const String& nodeId) const;

private:
    struct StringHash {
        size_t operator()(const String& value) const {
            return static_cast<size_t>(value.hashCode64());
        }
    };

    struct NodeEdges {
        std::unordered_map<String, std::vector<size_t>, StringHash> inputs;
        std::vector<size_t> incoming;
        std::vector<size_t> outgoing;
    };

    void add(size_t edgeIndex, const Edge& edge);
    const NodeEdges* edgesFor(const String& nodeId) const;

    std::unordered_map<String, NodeEdges, StringHash> nodes;
};

}
