#pragma once

#include "Graph/NodeGraph.h"

namespace CycleV2 {

class NodeGraphTestAccess {
public:
    static Node* findNodeForEditing(NodeGraph& graph, const String& nodeId) {
        return graph.findNodeForEditing(nodeId);
    }

    static void markChanged(NodeGraph& graph) {
        graph.markChanged();
    }
};

}
