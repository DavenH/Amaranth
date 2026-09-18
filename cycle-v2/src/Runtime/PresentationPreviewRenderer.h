#pragma once

#include <vector>

#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/GraphPresentationPerformanceMetrics.h"
#include "Runtime/NodeUpdateGraph.h"
#include "Runtime/PresentationUpdateRequestBuilder.h"

namespace CycleV2 {

struct GraphPresentationSnapshot;

class PresentationPreviewRenderer final {
public:
    bool render(
            const NodeGraph& graph,
            GraphPresentationSnapshot& snapshot,
            const std::vector<PlannedNodeProduct>& products,
            bool renderFullGraph,
            PresentationRefreshScope scope,
            bool& previewRendered,
            GraphPresentationPerformanceMetrics& performance,
            GraphAudioExecutor::CancellationCheck cancellationCheck = {});

    void resetExecutionState() { audioExecutor.resetExecutionState(); }
    size_t diagnosticProcessCount(const String& nodeId) const {
        return audioExecutor.diagnosticProcessCount(nodeId);
    }

private:
    GraphAudioExecutor audioExecutor;
};

}
