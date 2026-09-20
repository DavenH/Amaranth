#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Graph/GraphAudioScope.h"
#include "Graph/GraphValidationTypes.h"

namespace CycleV2 {

class GraphEdgeIndexOverlay;
class GraphEdgeView;

class GraphAudioValidationFacts {
public:
    GraphAudioValidationFacts(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            const GraphAudioScopeAnalysis& scopes);
    bool usesExplicitAudioGraph() const;
    void appendIssues(
            const GraphAudioScopeAnalysis& scopes,
            std::vector<GraphValidationIssue>& issues) const;

private:
    using NodeIdSet = std::unordered_set<
            String,
            GraphAudioScopeAnalysis::StringHash>;
    using TerminalCounts = std::unordered_map<
            String,
            int,
            GraphAudioScopeAnalysis::StringHash>;

    std::vector<String> globalInputs;
    std::vector<String> voiceOutputs;
    std::vector<String> outputs;
    NodeIdSet globalNodes;
    NodeIdSet reachableFromInput;
    NodeIdSet reachingOutput;
    TerminalCounts bypassingTerminals;
    int bypassingTerminalCount {};
};

}
