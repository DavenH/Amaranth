#pragma once

#include "GraphEditor.h"

namespace CycleV2 {

class GraphTransaction {
public:
    explicit GraphTransaction(NodeGraph& targetGraph);

    GraphEditResult addNode(NodeKind kind, Point<float> position);
    GraphEditResult connect(const PortAddress& first, const PortAddress& second);
    GraphEditResult removeNode(const String& nodeId);
    GraphEditResult removeEdgeAt(size_t edgeIndex);
    GraphEditResult setNodeParameter(
            const String& nodeId,
            const String& parameterId,
            const String& label,
            const String& value);

    GraphEditResult commit();
    void cancel();

private:
    void collect(const GraphEditResult& result);

    NodeGraph& target;
    NodeGraph candidate;
    GraphChangeSet changes;
    bool failed {};
    bool finished {};
    std::vector<GraphValidationIssue> validationIssues;
};

}
