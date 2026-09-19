#include "Graph/GraphEdgeIndex.h"

#include "Graph/GraphEdgeView.h"
#include "Graph/InteractionComplexityDiagnostics.h"

namespace CycleV2 {

namespace {

const std::vector<size_t>& emptyEdgeIndices() {
    static const std::vector<size_t> empty;
    return empty;
}

}

GraphEdgeIndex::GraphEdgeIndex(const std::vector<Edge>& edges) {
    InteractionComplexityDiagnostics::recordValidationEdgeVisits(edges.size());
    for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        add(edgeIndex, edges[edgeIndex]);
    }
}

GraphEdgeIndex::GraphEdgeIndex(const GraphEdgeView& edges) {
    InteractionComplexityDiagnostics::recordValidationEdgeVisits(edges.size());
    for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        add(edgeIndex, edges[edgeIndex]);
    }
}

const std::vector<size_t>& GraphEdgeIndex::edgesToInput(
        const String& nodeId,
        const String& portId) const {
    const NodeEdges* nodeEdges = edgesFor(nodeId);
    if (nodeEdges == nullptr) {
        return emptyEdgeIndices();
    }
    const auto found = nodeEdges->inputs.find(portId);
    return found != nodeEdges->inputs.end()
            ? found->second
            : emptyEdgeIndices();
}

const std::vector<size_t>& GraphEdgeIndex::incomingEdges(
        const String& nodeId) const {
    const NodeEdges* nodeEdges = edgesFor(nodeId);
    return nodeEdges != nullptr ? nodeEdges->incoming : emptyEdgeIndices();
}

const std::vector<size_t>& GraphEdgeIndex::outgoingEdges(
        const String& nodeId) const {
    const NodeEdges* nodeEdges = edgesFor(nodeId);
    return nodeEdges != nullptr ? nodeEdges->outgoing : emptyEdgeIndices();
}

void GraphEdgeIndex::add(size_t edgeIndex, const Edge& edge) {
    NodeEdges& source = nodes[edge.sourceNodeId];
    source.outgoing.push_back(edgeIndex);

    NodeEdges& destination = nodes[edge.destNodeId];
    destination.incoming.push_back(edgeIndex);
    destination.inputs[edge.destPortId].push_back(edgeIndex);
}

const GraphEdgeIndex::NodeEdges* GraphEdgeIndex::edgesFor(
        const String& nodeId) const {
    const auto found = nodes.find(nodeId);
    return found != nodes.end() ? &found->second : nullptr;
}

}
