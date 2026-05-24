#include "GraphInvalidation.h"

namespace CycleV2 {

GraphInvalidationResult GraphInvalidation::invalidateFrom(
        const GraphExecutionPlan& plan,
        const String& nodeId,
        GraphChangeKind changeKind) const {
    GraphInvalidationResult result;
    result.requiresRecompile = changeKind == GraphChangeKind::PortSchema;

    appendDependents(plan, nodeId, result.audioNodes);
    result.previewNodes = result.audioNodes;

    return result;
}

void GraphInvalidation::appendDependents(
        const GraphExecutionPlan& plan,
        const String& nodeId,
        std::vector<String>& nodes) const {
    if (contains(nodes, nodeId)) {
        return;
    }

    nodes.push_back(nodeId);

    for (const auto& edge : plan.signalEdges) {
        if (edge.sourceNodeId == nodeId) {
            appendDependents(plan, edge.destNodeId, nodes);
        }
    }

    for (const auto& attachment : plan.attachments) {
        if (attachment.sourceNodeId == nodeId) {
            appendDependents(plan, attachment.destNodeId, nodes);
        }
    }
}

bool GraphInvalidation::contains(const std::vector<String>& nodes, const String& nodeId) const {
    for (const auto& node : nodes) {
        if (node == nodeId) {
            return true;
        }
    }

    return false;
}

}
