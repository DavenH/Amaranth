#include "Graph/GraphSpliceValidator.h"

#include "Graph/GraphConnectionValidator.h"
#include "Graph/GraphValidationContext.h"
#include "Graph/GraphValidator.h"

namespace CycleV2 {

namespace {

GraphSpliceValidation validateSplice(
        const NodeGraph& graph,
        const GraphValidationContext& context,
        size_t edgeIndex,
        const String& nodeId,
        bool layoutChanged) {

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
    const auto validateProposal = [&](std::vector<size_t> removed, std::vector<Edge> added) {
        return layoutChanged
                ? context.validateProposalAfterLayoutChanges(
                        graph, std::move(removed), std::move(added))
                : context.validateProposal(graph, std::move(removed), std::move(added));
    };
    const auto removedEdgeIssues = validateProposal({ edgeIndex }, {});

    for (const auto& input : spliceNode->inputs) {
        if (!input.input) {
            continue;
        }

        auto incoming = connectionValidator.propose(
                graph, source, { nodeId, input.id, true });
        if (!incoming.succeeded()) {
            continue;
        }

        auto firstRemoved = context.edgeIndex().edgesToInput(
                incoming.destination.nodeId,
                incoming.destination.portId);
        firstRemoved.push_back(edgeIndex);
        const auto firstIssues = validateProposal(firstRemoved, { incoming.edge });
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
            const auto& replacedDestination = context.edgeIndex().edgesToInput(
                    outgoing.destination.nodeId,
                    outgoing.destination.portId);
            finalRemoved.insert(
                    finalRemoved.end(),
                    replacedDestination.begin(),
                    replacedDestination.end());
            const auto finalIssues = validateProposal(
                    std::move(finalRemoved),
                    { incoming.edge, outgoing.edge });
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

GraphSpliceValidation GraphSpliceValidator::validate(
        const NodeGraph& graph,
        size_t edgeIndex,
        const String& nodeId) const {
    const GraphValidationContext context(graph);
    return validateSplice(graph, context, edgeIndex, nodeId, false);
}

GraphSpliceValidation GraphSpliceValidator::validateAfterLayoutChanges(
        const NodeGraph& graph,
        const GraphValidationContext& context,
        size_t edgeIndex,
        const String& nodeId) const {
    return validateSplice(graph, context, edgeIndex, nodeId, true);
}

}
