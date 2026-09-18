#pragma once

#include <optional>
#include <vector>

#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/GraphPresentationPerformanceMetrics.h"
#include "Runtime/GraphPresentationSnapshot.h"
#include "Runtime/NodeUpdateGraph.h"
#include "Runtime/PresentationUpdateRequestBuilder.h"

namespace CycleV2 {

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
    std::optional<GraphPreviewResult::SignalProbePreview> captureProbePreview(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            const String& probeId,
            size_t rasterRowCount,
            int midiNote,
            int modWheelValue) const;

    void resetExecutionState() { audioExecutor.resetExecutionState(); }
    size_t diagnosticProcessCount(const String& nodeId) const {
        return audioExecutor.diagnosticProcessCount(nodeId);
    }

private:
    GraphAudioExecutor audioExecutor;
};

}
