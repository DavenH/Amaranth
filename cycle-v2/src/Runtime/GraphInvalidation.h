#pragma once

#include "../Graph/GraphCompiler.h"

#include <vector>

namespace CycleV2 {

struct GraphInvalidationResult {
    std::vector<String> audioNodes;
    std::vector<String> previewNodes;
    bool requiresRecompile {};
};

class GraphInvalidation {
public:
    GraphInvalidationResult invalidateFrom(
            const GraphExecutionPlan& plan,
            const String& nodeId,
            ParameterImpact impacts) const;

private:
    void appendDependents(
            const GraphExecutionPlan& plan,
            const String& nodeId,
            std::vector<String>& nodes) const;
    bool contains(const std::vector<String>& nodes, const String& nodeId) const;
};

}
