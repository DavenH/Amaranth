#include "Graph/GraphValidator.h"

#include "Graph/GraphAudioScopeValidator.h"
#include "Graph/GraphEdgeValidator.h"
#include "Graph/GraphEdgeView.h"
#include "Graph/GraphGuideValidator.h"
#include "Graph/GraphTopologyValidator.h"

namespace CycleV2 {

namespace {

bool sameValidationIssue(
        const GraphValidationIssue& first,
        const GraphValidationIssue& second) {
    return first.code == second.code
            && first.sourceNodeId == second.sourceNodeId
            && first.sourcePortId == second.sourcePortId
            && first.destNodeId == second.destNodeId
            && first.destPortId == second.destPortId;
}

}

std::vector<GraphValidationIssue> GraphValidator::validate(const NodeGraph& graph) const {
    return validate(graph, GraphEdgeView(graph.getEdges()));
}

std::vector<GraphValidationIssue> GraphValidator::validate(
        const NodeGraph& graph,
        const GraphEdgeView& edges) const {
    std::vector<GraphValidationIssue> issues;
    const GraphDomainResolution resolution = domainResolver.resolve(graph, edges);
    const auto scopeAnalysis = GraphAudioScopeAnalyzer().analyze(graph, edges);
    return validate(graph, edges, resolution, scopeAnalysis);
}

std::vector<GraphValidationIssue> GraphValidator::validate(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphDomainResolution& resolution,
        const GraphAudioScopeAnalysis& scopeAnalysis) const {
    std::vector<GraphValidationIssue> issues;
    GraphEdgeValidator edgeValidator;
    const bool explicitAudioGraph = GraphAudioScopeValidator::usesExplicitAudioGraph(graph);

    for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        edgeValidator.validate(
                graph,
                edges[edgeIndex],
                resolution.domains[edgeIndex],
                explicitAudioGraph ? &scopeAnalysis : nullptr,
                issues);
    }

    GraphGuideValidator().validate(graph, issues);
    GraphTopologyValidator().validate(graph, edges, resolution, issues);
    GraphAudioScopeValidator().validate(graph, edges, scopeAnalysis, issues);

    return issues;
}

bool GraphValidator::acceptsProposedIssues(
        const std::vector<GraphValidationIssue>& before,
        const std::vector<GraphValidationIssue>& after) {
    if (after.empty()) {
        return true;
    }
    if (before.empty() || after.size() >= before.size()) {
        return false;
    }
    return std::all_of(after.begin(), after.end(), [&](const auto& issue) {
        return std::any_of(before.begin(), before.end(), [&](const auto& prior) {
            return sameValidationIssue(issue, prior);
        });
    });
}

bool GraphValidator::isValid(const NodeGraph& graph) const {
    return validate(graph).empty();
}

bool GraphValidator::edgeHasValidationIssue(const NodeGraph& graph, const Edge& edge) const {
    return validationIssueForEdge(graph, edge).message.isNotEmpty();
}

GraphValidationIssue GraphValidator::validationIssueForEdge(const NodeGraph& graph, const Edge& edge) const {
    const auto analysis = GraphAudioScopeAnalyzer().analyze(graph);
    const bool explicitAudioGraph = GraphAudioScopeValidator::usesExplicitAudioGraph(graph);
    return GraphEdgeValidator().firstIssue(
            graph,
            edge,
            domainResolver.resolvedDomainForEdge(graph, edge),
            explicitAudioGraph ? &analysis : nullptr);
}

PortDomain GraphValidator::resolvedDomainForEdge(const NodeGraph& graph, const Edge& edge) const {
    return domainResolver.resolvedDomainForEdge(graph, edge);
}

}
