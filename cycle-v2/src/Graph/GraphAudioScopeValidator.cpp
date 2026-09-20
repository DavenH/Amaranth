#include <algorithm>

#include "Graph/GraphAudioScopeValidator.h"

#include "Graph/GraphAudioValidationFacts.h"
#include "Graph/GraphEdgeView.h"

namespace CycleV2 {

bool GraphAudioScopeValidator::usesExplicitAudioGraph(const NodeGraph& graph) {
    return std::any_of(
            graph.getNodes().begin(),
            graph.getNodes().end(),
            [](const Node& node) {
                return node.kind == NodeKind::GlobalInput
                        || node.kind == NodeKind::VoiceOutput;
            });
}

void GraphAudioScopeValidator::validate(
        const NodeGraph& graph,
        const GraphEdgeView& edges,
        const GraphAudioScopeAnalysis& analysis,
        std::vector<GraphValidationIssue>& issues) const {
    GraphAudioValidationFacts(graph, edges, analysis).appendIssues(
            analysis,
            issues);
}

}
