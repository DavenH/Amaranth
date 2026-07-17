#pragma once

#include "../Graph/GraphCompiler.h"

#include <vector>

namespace CycleV2 {

struct GraphInvalidationResult {
    std::vector<String> audioNodes;
    std::vector<String> previewNodes;
    bool requiresRecompile {};
    size_t visitedNodeCount {};
    size_t inspectedDependencyCount {};
};

class GraphInvalidation {
public:
    GraphInvalidationResult invalidateFrom(
            const GraphExecutionPlan& plan,
            const String& nodeId,
            ParameterImpact impacts) const;

private:
    std::vector<String> dependentsFrom(
            const GraphDependencyIndex& index,
            const String& nodeId,
            GraphInvalidationResult& diagnostics) const;
};

}
