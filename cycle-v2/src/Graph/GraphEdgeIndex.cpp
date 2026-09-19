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

GraphEdgeIndexOverlay::GraphEdgeIndexOverlay(
        const GraphEdgeIndex& baseIndex,
        const GraphEdgeView& proposedEdges) :
        base(baseIndex)
    ,   proposed(proposedEdges) {}

std::vector<size_t> GraphEdgeIndexOverlay::edgesToInput(
        const String& nodeId,
        const String& portId) const {
    auto result = translatedBaseEdges(base.edgesToInput(nodeId, portId));
    for (size_t addedIndex = 0; addedIndex < proposed.addedEdges().size(); ++addedIndex) {
        const Edge& edge = proposed.addedEdges()[addedIndex];
        if (edge.destNodeId == nodeId && edge.destPortId == portId) {
            result.push_back(proposed.retainedSize() + addedIndex);
        }
    }
    return result;
}

std::vector<size_t> GraphEdgeIndexOverlay::incomingEdges(
        const String& nodeId) const {
    return nodeEdges(nodeId, Direction::Incoming);
}

std::vector<size_t> GraphEdgeIndexOverlay::outgoingEdges(
        const String& nodeId) const {
    return nodeEdges(nodeId, Direction::Outgoing);
}

std::vector<size_t> GraphEdgeIndexOverlay::translatedBaseEdges(
        const std::vector<size_t>& baseEdges) const {
    std::vector<size_t> result;
    result.reserve(baseEdges.size());
    for (const size_t baseEdgeIndex : baseEdges) {
        if (const auto viewIndex = proposed.viewIndexForExisting(baseEdgeIndex)) {
            result.push_back(*viewIndex);
        }
    }
    return result;
}

std::vector<size_t> GraphEdgeIndexOverlay::nodeEdges(
        const String& nodeId,
        Direction direction) const {
    auto result = translatedBaseEdges(
            direction == Direction::Incoming
                    ? base.incomingEdges(nodeId)
                    : base.outgoingEdges(nodeId));
    for (size_t addedIndex = 0; addedIndex < proposed.addedEdges().size(); ++addedIndex) {
        const Edge& edge = proposed.addedEdges()[addedIndex];
        const bool matches = direction == Direction::Incoming
                ? edge.destNodeId == nodeId
                : edge.sourceNodeId == nodeId;
        if (matches) {
            result.push_back(proposed.retainedSize() + addedIndex);
        }
    }
    return result;
}

}
