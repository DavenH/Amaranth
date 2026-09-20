#pragma once

#include "Graph/GraphAudioScope.h"
#include "Graph/GraphAudioValidationFacts.h"
#include "Graph/GraphDomainResolver.h"
#include "Graph/GraphEdgeIndex.h"
#include "Graph/GraphEdgeView.h"
#include "Graph/GraphValidationTypes.h"
#include "Graph/GraphVoiceContextAssignments.h"

namespace CycleV2 {

class GraphValidationContext {
public:
    explicit GraphValidationContext(const NodeGraph& graph);

    bool matches(const NodeGraph& graph) const;
    const GraphEdgeView& edgeView() const { return edges; }
    const GraphEdgeIndex& edgeIndex() const { return indexedEdges; }
    const GraphDomainResolution& domainResolution() const { return domains; }
    const GraphAudioScopeAnalysis& audioScopeAnalysis() const { return audioScope; }
    const GraphAudioValidationFacts& audioValidationFacts() const { return audioFacts; }
    const std::vector<GraphValidationIssue>& validationIssues() const { return issues; }
    const GraphVoiceContextAssignments& voiceContextAssignments() const {
        return voiceContexts;
    }
    std::vector<GraphValidationIssue> validateProposal(
            const NodeGraph& graph,
            std::vector<size_t> removedEdges,
            std::vector<Edge> addedEdges) const;

private:
    const NodeGraph* source {};
    uint64_t revision {};
    GraphEdgeView edges;
    GraphEdgeIndex indexedEdges;
    GraphDomainResolution domains;
    GraphAudioScopeAnalysis audioScope;
    GraphAudioValidationFacts audioFacts;
    GraphVoiceContextAssignments voiceContexts;
    std::vector<GraphValidationIssue> issues;
};

}
