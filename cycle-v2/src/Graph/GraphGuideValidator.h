#pragma once

#include <vector>

#include "Graph/NodeGraph.h"

namespace CycleV2 {

struct GraphValidationIssue;

class GraphGuideValidator {
public:
    void validate(
            const NodeGraph& graph,
            std::vector<GraphValidationIssue>& issues) const;
};

}
