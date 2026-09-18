#pragma once

#include "Graph/GraphEditTypes.h"
#include "Graph/GraphNodeFactory.h"

namespace CycleV2 {

class GraphEditor {
public:
    GraphEditResult addNode(NodeGraph& graph, NodeKind kind, Point<float> position) const;
    GraphEditResult connect(NodeGraph& graph, const PortAddress& first, const PortAddress& second) const;
    GraphEditResult toggleSignalProbe(NodeGraph& graph, size_t edgeIndex, float tapPosition) const;
    GraphEditResult removeSignalProbe(NodeGraph& graph, const String& probeId) const;
    GraphEditResult reattachSignalProbe(
            NodeGraph& graph,
            const String& probeId,
            size_t edgeIndex,
            float tapPosition) const;
    GraphEditResult spliceNodeIntoEdge(NodeGraph& graph, size_t edgeIndex, const String& nodeId) const;
    GraphEditResult removeEdgeAt(NodeGraph& graph, size_t index) const;
    GraphEditResult removeNode(NodeGraph& graph, const String& nodeId) const;

private:
    GraphEditCode proposeConnectionEdge(
            const NodeGraph& graph,
            const PortAddress& first,
            const PortAddress& second,
            Edge& edge) const;
    const Node* findNode(const NodeGraph& graph, const String& nodeId) const;
    const Port* findPort(const Node& node, const String& portId, bool input) const;
    String createUniqueNodeId(const NodeGraph& graph, NodeKind kind) const;
    String createUniqueProbeId(const NodeGraph& graph) const;
    String baseIdForKind(NodeKind kind) const;
};

}
