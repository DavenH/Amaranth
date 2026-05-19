#include "GraphCompiler.h"

#include <algorithm>

namespace CycleV2 {

namespace {

int indexOfNode(const std::vector<Node>& nodes, const String& nodeId) {
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        if (nodes[static_cast<size_t>(i)].id == nodeId) {
            return i;
        }
    }

    return -1;
}

std::vector<String> buildNodeOrder(
        const NodeGraph& graph,
        std::vector<GraphCompileIssue>& issues) {
    const auto& nodes = graph.getNodes();
    const auto& edges = graph.getEdges();

    std::vector<int> indegrees(nodes.size(), 0);
    std::vector<bool> emitted(nodes.size(), false);
    std::vector<String> order;
    order.reserve(nodes.size());

    for (const auto& edge : edges) {
        const int sourceIndex = indexOfNode(nodes, edge.sourceNodeId);
        const int destIndex = indexOfNode(nodes, edge.destNodeId);

        if (sourceIndex >= 0 && destIndex >= 0 && sourceIndex != destIndex) {
            ++indegrees[static_cast<size_t>(destIndex)];
        }
    }

    while (order.size() < nodes.size()) {
        int readyIndex = -1;

        for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
            if (!emitted[static_cast<size_t>(i)] && indegrees[static_cast<size_t>(i)] == 0) {
                readyIndex = i;
                break;
            }
        }

        if (readyIndex < 0) {
            issues.push_back({
                    GraphCompileCode::CycleDetected,
                    "Graph contains a cycle in processing dependencies"
            });
            break;
        }

        emitted[static_cast<size_t>(readyIndex)] = true;
        order.push_back(nodes[static_cast<size_t>(readyIndex)].id);

        for (const auto& edge : edges) {
            if (edge.sourceNodeId != nodes[static_cast<size_t>(readyIndex)].id) {
                continue;
            }

            const int destIndex = indexOfNode(nodes, edge.destNodeId);
            if (destIndex >= 0 && destIndex != readyIndex) {
                --indegrees[static_cast<size_t>(destIndex)];
            }
        }
    }

    return order;
}

std::vector<GraphExecutionStep> buildExecutionSteps(
        const NodeGraph& graph,
        const std::vector<String>& nodeOrder,
        const NodeModuleRegistry& moduleRegistry) {
    std::vector<GraphExecutionStep> steps;
    steps.reserve(nodeOrder.size());

    for (const auto& nodeId : nodeOrder) {
        const int nodeIndex = indexOfNode(graph.getNodes(), nodeId);

        if (nodeIndex < 0) {
            continue;
        }

        const Node& node = graph.getNodes()[static_cast<size_t>(nodeIndex)];
        const auto descriptor = moduleRegistry.descriptorFor(node.kind);

        steps.push_back({
                node.id,
                node.kind,
                descriptor.audioRole,
                descriptor.previewRole,
                descriptor.previewable,
                descriptor.cycle1AdapterBacked
        });
    }

    return steps;
}

}

bool GraphCompileResult::succeeded() const {
    return validationIssues.empty() && compileIssues.empty();
}

GraphCompileResult GraphCompiler::compile(const NodeGraph& graph) const {
    GraphCompileResult result;
    result.validationIssues = validator.validate(graph);

    if (!result.validationIssues.empty()) {
        return result;
    }

    for (const auto& edge : graph.getEdges()) {
        if (edge.attachment) {
            result.plan.attachments.push_back(edge);
        } else {
            result.plan.signalEdges.push_back(edge);
        }
    }

    result.plan.nodeOrder = buildNodeOrder(graph, result.compileIssues);

    if (result.compileIssues.empty()) {
        result.plan.steps = buildExecutionSteps(graph, result.plan.nodeOrder, moduleRegistry);
    }

    if (!result.succeeded()) {
        result.plan = {};
    }

    return result;
}

}
