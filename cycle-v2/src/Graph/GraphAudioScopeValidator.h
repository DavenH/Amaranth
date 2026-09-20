#pragma once

#include <vector>

#include "Graph/GraphAudioScope.h"
#include "Graph/NodeGraph.h"

namespace CycleV2 {

class GraphEdgeView;
class GraphAudioValidationFacts;
struct GraphValidationIssue;

class GraphAudioScopeValidator {
public:
    void validate(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            const GraphAudioScopeAnalysis& analysis,
            std::vector<GraphValidationIssue>& issues) const;
    void validate(
            const GraphAudioScopeAnalysis& analysis,
            const GraphAudioValidationFacts& facts,
            std::vector<GraphValidationIssue>& issues) const;
};

}
