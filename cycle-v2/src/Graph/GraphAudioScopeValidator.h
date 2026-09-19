#pragma once

#include <vector>

#include "Graph/GraphAudioScope.h"
#include "Graph/NodeGraph.h"

namespace CycleV2 {

class GraphEdgeView;
struct GraphValidationIssue;

class GraphAudioScopeValidator {
public:
    static bool usesExplicitAudioGraph(const NodeGraph& graph);

    void validate(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            const GraphAudioScopeAnalysis& analysis,
            std::vector<GraphValidationIssue>& issues) const;
};

}
