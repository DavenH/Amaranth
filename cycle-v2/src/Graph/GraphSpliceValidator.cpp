#include "Graph/GraphSpliceValidator.h"

#include "Graph/GraphConnectionValidator.h"
#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphEdgeView.h"
#include "Graph/GraphValidator.h"

namespace CycleV2 {

GraphSpliceValidation GraphSpliceValidator::validate(
        const NodeGraph& graph,
        size_t edgeIndex,
        const String& nodeId) const {
    if (edgeIndex >= graph.getEdges().size()) {
        return { GraphEditCode::MissingEdge };
    }

    const Edge& edge = graph.getEdges()[edgeIndex];
    if (edge.sourceNodeId == nodeId || edge.destNodeId == nodeId) {
        return { GraphEditCode::ValidationRejected };
    }

    const Node* spliceNode = graph.findNode(nodeId);
    if (spliceNode == nullptr) {
        return { GraphEditCode::MissingNode };
    }

    const PortAddress source { edge.sourceNodeId, edge.sourcePortId, false };
    const PortAddress destination { edge.destNodeId, edge.destPortId, true };
    const GraphConnectionValidator connectionValidator;
    const GraphEdgeIndex indexedEdges(graph.getEdges());
    const GraphValidator validator;
    const GraphEdgeView withoutOriginal(graph.getEdges(), { edgeIndex }, {});
    const auto removedEdgeIssues = validator.validate(graph, withoutOriginal);

    for (const auto& input : spliceNode->inputs) {
        if (!input.input) {
            continue;
        }

        auto incoming = connectionValidator.propose(
                graph, source, { nodeId, input.id, true });
        if (!incoming.succeeded()) {
            continue;
        }

        auto firstRemoved = indexedEdges.edgesToInput(
                incoming.destination.nodeId,
                incoming.destination.portId);
        firstRemoved.push_back(edgeIndex);
        const GraphEdgeView firstProposal(
                graph.getEdges(), firstRemoved, { incoming.edge });
        const auto firstIssues = validator.validate(graph, firstProposal);
        if (!firstIssues.empty()
                && !GraphValidator::acceptsProposedIssues(
                        removedEdgeIssues, firstIssues)) {
            continue;
        }

        for (const auto& output : spliceNode->outputs) {
            if (output.input) {
                continue;
            }

            auto outgoing = connectionValidator.propose(
                    graph, { nodeId, output.id, false }, destination);
            if (!outgoing.succeeded()) {
                continue;
            }

            auto finalRemoved = firstRemoved;
            const auto& replacedDestination = indexedEdges.edgesToInput(
                    outgoing.destination.nodeId,
                    outgoing.destination.portId);
            finalRemoved.insert(
                    finalRemoved.end(),
                    replacedDestination.begin(),
                    replacedDestination.end());
            const GraphEdgeView finalProposal(
                    graph.getEdges(),
                    std::move(finalRemoved),
                    { incoming.edge, outgoing.edge });
            const auto finalIssues = validator.validate(graph, finalProposal);
            if (!finalIssues.empty()
                    && !GraphValidator::acceptsProposedIssues(
                            firstIssues, finalIssues)) {
                continue;
            }

            return {
                    GraphEditCode::Connected,
                    std::move(incoming.edge),
                    std::move(outgoing.edge)
            };
        }
    }

    return { GraphEditCode::ValidationRejected };
}

}
