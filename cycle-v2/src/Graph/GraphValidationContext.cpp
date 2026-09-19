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
    ,   issues(GraphValidator().validate(
                graph,
                edges,
                domains,
                audioScope)) {}

bool GraphValidationContext::matches(const NodeGraph& graph) const {
    return source == &graph && revision == graph.getRevision();
}

}
