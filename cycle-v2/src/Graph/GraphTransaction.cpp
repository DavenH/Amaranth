#include "GraphTransaction.h"

#include <algorithm>

namespace CycleV2 {

GraphTransaction::GraphTransaction(NodeGraph& targetGraph) :
        target    (targetGraph)
    ,   candidate (targetGraph) {}

GraphEditResult GraphTransaction::addNode(NodeKind kind, Point<float> position) {
    auto result = GraphEditor().addNode(candidate, kind, position);
    if (result.succeeded()) {
        result.changes.nodeIds.push_back(result.nodeId);
        result.changes.topologyChanged = true;
    }
    collect(result);
    return result;
}

GraphEditResult GraphTransaction::connect(const PortAddress& first, const PortAddress& second) {
    auto result = GraphEditor().connect(candidate, first, second);
    if (result.succeeded()) {
        result.changes.nodeIds = { first.nodeId, second.nodeId };
        result.changes.topologyChanged = true;
    }
    collect(result);
    return result;
}

GraphEditResult GraphTransaction::removeNode(const String& nodeId) {
    auto result = GraphEditor().removeNode(candidate, nodeId);
    if (result.succeeded()) {
        result.changes.nodeIds.push_back(nodeId);
        result.changes.topologyChanged = true;
    }
    collect(result);
    return result;
}

GraphEditResult GraphTransaction::removeEdgeAt(size_t edgeIndex) {
    auto result = GraphEditor().removeEdgeAt(candidate, edgeIndex);
    if (result.succeeded()) {
        result.changes.topologyChanged = true;
    }
    collect(result);
    return result;
}

GraphEditResult GraphTransaction::setNodeParameter(
        const String& nodeId,
        const String& parameterId,
        const String& label,
        const String& value) {
    auto result = GraphEditor().setNodeParameter(candidate, nodeId, parameterId, label, value);
    collect(result);
    return result;
}

GraphEditResult GraphTransaction::commit() {
    if (finished || failed) {
        return { GraphEditCode::ValidationRejected, {}, validationIssues, changes };
    }

    const auto issues = GraphValidator().validate(candidate);
    if (!issues.empty()) {
        failed = true;
        validationIssues = issues;
        return { GraphEditCode::ValidationRejected, {}, issues, changes };
    }

    target = std::move(candidate);
    finished = true;
    GraphEditResult result;
    result.changes = changes;
    return result;
}

void GraphTransaction::cancel() {
    finished = true;
}

void GraphTransaction::collect(const GraphEditResult& result) {
    if (!result.succeeded()) {
        failed = true;
        validationIssues.insert(
                validationIssues.end(),
                result.validationIssues.begin(),
                result.validationIssues.end());
        return;
    }

    for (const auto& nodeId : result.changes.nodeIds) {
        if (std::find(changes.nodeIds.begin(), changes.nodeIds.end(), nodeId) == changes.nodeIds.end()) {
            changes.nodeIds.push_back(nodeId);
        }
    }
    changes.topologyChanged = changes.topologyChanged || result.changes.topologyChanged;
    changes.layoutChanged = changes.layoutChanged || result.changes.layoutChanged;
    changes.parameterImpacts = changes.parameterImpacts | result.changes.parameterImpacts;
}

}
