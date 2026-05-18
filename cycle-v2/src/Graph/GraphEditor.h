#pragma once

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
    DirectionMismatch,
    ValidationRejected
};

struct GraphEditResult {
    GraphEditCode code { GraphEditCode::Connected };
    std::vector<GraphValidationIssue> validationIssues;

    bool succeeded() const { return code == GraphEditCode::Connected; }
};

class GraphEditor {
public:
    GraphEditResult connect(NodeGraph& graph, const PortAddress& first, const PortAddress& second) const;
    GraphEditResult removeNode(NodeGraph& graph, const String& nodeId) const;

private:
    const Node* findNode(const NodeGraph& graph, const String& nodeId) const;
    const Port* findPort(const Node& node, const String& portId, bool input) const;
};

}
