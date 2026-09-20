#include "Graph/GraphAudioScopeValidator.h"

#include "Graph/GraphAudioValidationFacts.h"
#include "Graph/GraphEdgeView.h"

namespace CycleV2 {

void GraphAudioScopeValidator::validate(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphAudioScopeAnalysis& analysis,
        std::vector<GraphValidationIssue>& issues) const {
    const GraphAudioValidationFacts facts(graph, edges, analysis);
    validate(analysis, facts, issues);
}

void GraphAudioScopeValidator::validate(
        const GraphAudioScopeAnalysis& analysis,
        const GraphAudioValidationFacts& facts,
        std::vector<GraphValidationIssue>& issues) const {
    facts.appendIssues(analysis, issues);
}

}
