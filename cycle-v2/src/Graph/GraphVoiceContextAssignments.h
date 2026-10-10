#pragma once

#include <unordered_map>
#include <vector>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class GraphEdgeView;

class GraphVoiceContextAssignments {
public:
    GraphVoiceContextAssignments(const NodeGraph& graph, const GraphEdgeView& edges);
    GraphVoiceContextAssignments(
            const GraphVoiceContextAssignments& baseline,
            const GraphEdgeView& edges);

    const std::vector<String>& contextNodeIds() const { return contexts; }
    const std::vector<String>& acceptingNodeIds() const { return acceptingNodes; }
    const String* explicitContextFor(const String& nodeId) const;
    std::vector<Edge> implicitEdges() const;

private:
    struct StringHash {
        size_t operator()(const String& value) const {
            return static_cast<size_t>(value.hashCode64());
        }
    };

    std::vector<String> contexts;
    std::vector<String> acceptingNodes;
    std::unordered_map<String, String, StringHash> explicitAssignments;
};

}
