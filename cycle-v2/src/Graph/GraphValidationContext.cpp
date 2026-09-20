#include "Graph/GraphValidationContext.h"

#include "Graph/GraphAudioScopeValidator.h"
#include "Graph/GraphValidator.h"

namespace CycleV2 {

GraphValidationContext::GraphValidationContext(const NodeGraph& graph) :
        source(&graph)
    ,   revision(graph.getRevision())
    ,   edges(graph.getEdges())
    ,   indexedEdges(edges)
    ,   domains(GraphDomainResolver().resolve(graph, edges))
    ,   audioScope(GraphAudioScopeAnalyzer().analyze(graph, edges))
    ,   voiceContexts(graph, edges)
    ,   issues(GraphValidator().validate(
                graph,
                edges,
                domains,
                audioScope))
    ,   explicitAudioGraph(GraphAudioScopeValidator::usesExplicitAudioGraph(graph)) {}

bool GraphValidationContext::matches(const NodeGraph& graph) const {
    return source == &graph && revision == graph.getRevision();
}

std::vector<GraphValidationIssue> GraphValidationContext::validateProposal(
        const NodeGraph& graph,
        std::vector<size_t> removedEdges,
        std::vector<Edge> addedEdges) const {
    const GraphEdgeView proposedEdges(
            graph.getEdges(),
            std::move(removedEdges),
            std::move(addedEdges));
    jassert(matches(graph));
    if (!matches(graph)) {
        return GraphValidator().validate(graph, proposedEdges);
    }

    const GraphEdgeIndexOverlay proposedIndex(indexedEdges, proposedEdges);
    return GraphValidator().validateProposal(
            graph,
            proposedEdges,
            proposedIndex,
            *this);
}

}
