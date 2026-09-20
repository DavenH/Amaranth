#pragma once

#include <vector>

#include "Graph/GraphDomainResolver.h"
#include "Graph/NodeGraph.h"

namespace CycleV2 {

class GraphEdgeView;
class GraphEdgeIndexOverlay;
class GraphVoiceContextAssignments;
struct GraphValidationIssue;

class GraphTopologyValidator {
public:
    void validate(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            const GraphDomainResolution& resolution,
            std::vector<GraphValidationIssue>& issues) const;
    void validateOperationNode(
            const Node& node,
            const GraphEdgeView& edges,
            const GraphEdgeIndexOverlay& edgeIndex,
            const GraphDomainResolution& resolution,
            std::vector<GraphValidationIssue>& issues) const;
    void validateVoiceContextAssignments(
            const NodeGraph& graph,
            const GraphVoiceContextAssignments& assignments,
            std::vector<GraphValidationIssue>& issues) const;

private:
    void validateOperationInputs(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            const GraphDomainResolution& resolution,
            std::vector<GraphValidationIssue>& issues) const;
    void validateVoiceContextAssignments(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            std::vector<GraphValidationIssue>& issues) const;
};

}
