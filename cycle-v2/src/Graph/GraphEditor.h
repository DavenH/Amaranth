#pragma once

#include "GraphNodeFactory.h"
#include "GraphValidator.h"

namespace CycleV2 {

struct PortAddress {
    String nodeId;
    String portId;
    bool input {};
};

enum class GraphEditCode {
    Connected,
    MissingNode,
    MissingPort,
    MissingEdge,
    DirectionMismatch,
    ValidationRejected
};

struct GraphEditResult {
    GraphEditCode code { GraphEditCode::Connected };
    String nodeId;
    std::vector<GraphValidationIssue> validationIssues;

    bool succeeded() const { return code == GraphEditCode::Connected; }
};

class GraphEditor {
public:
    GraphEditResult addNode(NodeGraph& graph, NodeKind kind, Point<float> position) const;
    GraphEditResult connect(NodeGraph& graph, const PortAddress& first, const PortAddress& second) const;
    GraphEditResult removeEdgeAt(NodeGraph& graph, size_t index) const;
    GraphEditResult removeNode(NodeGraph& graph, const String& nodeId) const;
    GraphEditResult setNodeParameter(
            NodeGraph& graph,
            const String& nodeId,
            const String& parameterId,
            const String& label,
            const String& value) const;

private:
    const Node* findNode(const NodeGraph& graph, const String& nodeId) const;
    Node* findMutableNode(NodeGraph& graph, const String& nodeId) const;
    const Port* findPort(const Node& node, const String& portId, bool input) const;
    String createUniqueNodeId(const NodeGraph& graph, NodeKind kind) const;
    String baseIdForKind(NodeKind kind) const;
};

}
