#pragma once

#include <vector>

#include "Graph/GraphAudioScope.h"
#include "Graph/NodeGraph.h"

namespace CycleV2 {

struct GraphValidationIssue;

class GraphEdgeValidator {
public:
    void validate(
            const NodeGraph& graph,
            const Edge& edge,
            PortDomain resolvedDomain,
            const GraphAudioScopeAnalysis* scopeAnalysis,
            std::vector<GraphValidationIssue>& issues) const;
    GraphValidationIssue firstIssue(
            const NodeGraph& graph,
            const Edge& edge,
            PortDomain resolvedDomain,
            const GraphAudioScopeAnalysis* scopeAnalysis) const;

private:
    class EdgeIssueReporter;

    void validate(
            const NodeGraph& graph,
            const Edge& edge,
            PortDomain resolvedDomain,
            const GraphAudioScopeAnalysis* scopeAnalysis,
            EdgeIssueReporter& reporter) const;
    bool isVoiceAwareDestination(const Port& port) const;
    bool domainsCompatible(const Port& source, const Port& dest) const;
    bool channelLayoutsCompatible(const Port& source, const Port& dest) const;
};

}
