#include <algorithm>

#include "Graph/GraphVoiceContextAssignments.h"

#include "Graph/GraphEdgeView.h"

namespace CycleV2 {

GraphVoiceContextAssignments::GraphVoiceContextAssignments(
        const NodeGraph& graph,
        const GraphEdgeView& edges) {
    for (const auto& node : graph.getNodes()) {
        if (node.kind == NodeKind::VoiceContext) {
            contexts.push_back(node.id);
        }
        const bool acceptsContext = std::any_of(
                node.inputs.begin(),
                node.inputs.end(),
                [](const Port& port) {
                    return port.id == "context"
                            && port.domain == PortDomain::DomainContext;
                });
        if (acceptsContext) {
            acceptingNodes.push_back(node.id);
        }
    }

    for (const auto& edge : edges) {
        if (!edge.isAttachment() && edge.destPortId == "context") {
            explicitAssignments.emplace(edge.destNodeId, edge.sourceNodeId);
        }
    }
}

GraphVoiceContextAssignments::GraphVoiceContextAssignments(
        const GraphVoiceContextAssignments& baseline,
        const GraphEdgeView& edges) :
        contexts(baseline.contexts)
    ,   acceptingNodes(baseline.acceptingNodes)
    ,   explicitAssignments(baseline.explicitAssignments) {
    for (const size_t removedIndex : edges.removedIndices()) {
        const Edge& edge = edges.existingEdge(removedIndex);
        if (!edge.isAttachment() && edge.destPortId == "context") {
            explicitAssignments.erase(edge.destNodeId);
        }
    }
    for (const Edge& edge : edges.addedEdges()) {
        if (!edge.isAttachment() && edge.destPortId == "context") {
            explicitAssignments[edge.destNodeId] = edge.sourceNodeId;
        }
    }
}

const String* GraphVoiceContextAssignments::explicitContextFor(
        const String& nodeId) const {
    const auto found = explicitAssignments.find(nodeId);
    return found != explicitAssignments.end() ? &found->second : nullptr;
}

std::vector<Edge> GraphVoiceContextAssignments::implicitEdges() const {
    if (contexts.size() != 1) {
        return {};
    }

    std::vector<Edge> edges;
    for (const String& nodeId : acceptingNodes) {
        if (explicitContextFor(nodeId) != nullptr) {
            continue;
        }
        edges.push_back({
                contexts.front(),
                "context",
                nodeId,
                "context",
                PortDomain::DomainContext,
                ConnectionKind::Signal
        });
    }
    return edges;
}

}
