#pragma once

#include <vector>

#include "Graph/GraphDomainResolver.h"
#include "Graph/NodeGraph.h"

namespace CycleV2 {

class GraphEdgeView;
struct GraphValidationIssue;

class GraphTopologyValidator {
public:
    void validate(
            const NodeGraph& graph,
            const GraphEdgeView& edges,
            const GraphDomainResolution& resolution,
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
