#include "Graph/GraphValidationContext.h"

#include "Graph/GraphValidator.h"

namespace CycleV2 {

GraphValidationContext::GraphValidationContext(const NodeGraph& graph) :
        source(&graph)
    ,   revision(graph.getRevision())
    ,   edges(graph.getEdges())
    ,   indexedEdges(edges)
    ,   domains(GraphDomainResolver().resolve(graph, edges))
    ,   audioScope(GraphAudioScopeAnalyzer().analyze(graph, edges))
    ,   audioFacts(graph, edges, audioScope)
    ,   voiceContexts(graph, edges)
    ,   issues(GraphValidator().validate(
                graph,
                edges,
                domains,
                audioScope,
                audioFacts)) {}

bool GraphValidationContext::matches(const NodeGraph& graph) const {
    return source == &graph && revision == graph.getRevision();
}

std::vector<GraphValidationIssue> GraphValidationContext::validateProposal(
        const NodeGraph& graph,
        std::vector<size_t> removedEdges,
        std::vector<Edge> addedEdges) const {
    jassert(matches(graph));
    if (!matches(graph)) {
        const GraphEdgeView proposedEdges(
                graph.getEdges(),
                std::move(removedEdges),
                std::move(addedEdges));
        return GraphValidator().validate(graph, proposedEdges);
    }

    return validateRetainedProposal(
            graph, std::move(removedEdges), std::move(addedEdges));
}

std::vector<GraphValidationIssue> GraphValidationContext::validateProposalAfterLayoutChanges(
        const NodeGraph& graph,
        std::vector<size_t> removedEdges,
        std::vector<Edge> addedEdges) const {
    jassert(source == &graph);
    if (source != &graph) {
        const GraphEdgeView proposedEdges(
                graph.getEdges(),
                std::move(removedEdges),
                std::move(addedEdges));
        return GraphValidator().validate(graph, proposedEdges);
    }

    return validateRetainedProposal(
            graph, std::move(removedEdges), std::move(addedEdges));
}

std::vector<GraphValidationIssue> GraphValidationContext::validateRetainedProposal(
        const NodeGraph& graph,
        std::vector<size_t> removedEdges,
        std::vector<Edge> addedEdges) const {
    const GraphEdgeView proposedEdges(
            graph.getEdges(),
            std::move(removedEdges),
            std::move(addedEdges));

    const GraphEdgeIndexOverlay proposedIndex(indexedEdges, proposedEdges);
    return GraphValidator().validateProposal(
            graph,
            proposedEdges,
            proposedIndex,
            *this);
}

}
