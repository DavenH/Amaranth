#pragma once

#include "NodePreviewProcessor.h"
#include "../Graph/GraphCompiler.h"

namespace CycleV2 {

struct NodePreviewResult {
    String nodeId;
    PreviewModuleRole role { PreviewModuleRole::None };
    std::vector<float> primary;
    std::vector<float> secondary;
};

struct GraphPreviewResult {
    std::vector<NodePreviewResult> nodes;
};

class GraphPreviewExecutor {
public:
    GraphPreviewResult render(const GraphExecutionPlan& plan, size_t pointCount) const;
};

}
