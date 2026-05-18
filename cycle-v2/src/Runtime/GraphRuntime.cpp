#include "GraphRuntime.h"

namespace CycleV2 {

RuntimeProcessTrace GraphRuntime::process(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan) const {
    RuntimeProcessTrace trace;
    trace.nodes.reserve(plan.nodeOrder.size());

    for (const auto& nodeId : plan.nodeOrder) {
        const Node* node = findNode(graph, nodeId);

        if (node == nullptr) {
            continue;
        }

        trace.nodes.push_back({
                node->id,
                node->kind,
                collectInputs(plan.signalEdges, node->id),
                collectInputs(plan.attachments, node->id)
        });
    }

    return trace;
}

const Node* GraphRuntime::findNode(const NodeGraph& graph, const String& nodeId) const {
    for (const auto& node : graph.getNodes()) {
        if (node.id == nodeId) {
            return &node;
        }
    }

    return nullptr;
}

std::vector<RuntimeInput> GraphRuntime::collectInputs(
        const std::vector<Edge>& edges,
        const String& nodeId) const {
    std::vector<RuntimeInput> inputs;

    for (const auto& edge : edges) {
        if (edge.destNodeId != nodeId) {
            continue;
        }

        inputs.push_back({
                edge.sourceNodeId,
                edge.sourcePortId,
                edge.destPortId,
                edge.domain
        });
    }

    return inputs;
}

}
