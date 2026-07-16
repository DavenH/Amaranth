#pragma once

#include "NodePreviewProcessor.h"
#include "GraphAudioExecutor.h"
#include "../Graph/GraphCompiler.h"

namespace CycleV2 {

struct NodePreviewResult {
    String nodeId;
    PreviewModuleRole role { PreviewModuleRole::None };
    std::vector<float> primary;
    std::vector<float> secondary;
    size_t gridColumns {};
    size_t gridRows {};
    PortDomain domain { PortDomain::TimeSignal };
};

struct GraphPreviewResult {
    std::vector<NodePreviewResult> nodes;
    size_t indexedNodeCount {};
    size_t addressLookupCount {};
    size_t aliasedInputCount {};
};

class GraphPreviewExecutor {
public:
    GraphPreviewResult render(const GraphExecutionPlan& plan, size_t pointCount) const;
    GraphPreviewResult render(
            const GraphExecutionPlan& plan,
            const GraphAudioResult& audioResult,
            size_t pointCount) const;
};

}
