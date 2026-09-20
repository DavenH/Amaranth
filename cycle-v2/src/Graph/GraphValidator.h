#pragma once

#include <vector>

#include "Graph/GraphAudioScope.h"
#include "Graph/GraphDomainResolver.h"
#include "Graph/GraphValidationTypes.h"
#include "Graph/NodeGraph.h"

namespace CycleV2 {

class GraphEdgeView;
class GraphEdgeIndexOverlay;
class GraphAudioValidationFacts;
class GraphValidationContext;

class GraphValidator {
public:
    std::vector<GraphValidationIssue> validate(const NodeGraph& graph) const;
    std::vector<GraphValidationIssue> validate(
            const NodeGraph& graph,
            const GraphEdgeView& edges) const;
    std::vector<GraphValidationIssue> validate(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            const GraphDomainResolution& resolution,
            const GraphAudioScopeAnalysis& scopeAnalysis) const;
    std::vector<GraphValidationIssue> validate(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            const GraphDomainResolution& resolution,
            const GraphAudioScopeAnalysis& scopeAnalysis,
            const GraphAudioValidationFacts& audioFacts) const;
    std::vector<GraphValidationIssue> validateProposal(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            const GraphEdgeIndexOverlay& edgeIndex,
            const GraphValidationContext& baseline) const;
    static bool acceptsProposedIssues(
            const std::vector<GraphValidationIssue>& before,
            const std::vector<GraphValidationIssue>& after);
    bool isValid(const NodeGraph& graph) const;
    bool edgeHasValidationIssue(const NodeGraph& graph, const Edge& edge) const;
    GraphValidationIssue validationIssueForEdge(const NodeGraph& graph, const Edge& edge) const;
    PortDomain resolvedDomainForEdge(const NodeGraph& graph, const Edge& edge) const;

private:
    GraphDomainResolver domainResolver;
};

}
