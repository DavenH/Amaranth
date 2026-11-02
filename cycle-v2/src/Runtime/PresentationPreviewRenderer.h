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
            const GraphPresentationSnapshot& snapshot,
            const String& probeId,
            size_t rasterRowCount,
            int midiNote,
            GraphPresentationPerformanceMetrics& performance,
            double noteOffTime = 1.,
            PresetPreviewView view = PresetPreviewView::Time) const;
    std::vector<std::optional<GraphPreviewResult::SignalProbePreview>> captureProbePreviews(
            const NodeGraph& graph,
            const GraphPresentationSnapshot& snapshot,
            const std::vector<String>& probeIds,
            size_t rasterRowCount,
            int midiNote,
            GraphPresentationPerformanceMetrics& performance,
            PresetPreviewView outputView = PresetPreviewView::Time) const;

    static void publishProbePreview(
            GraphPresentationSnapshot& snapshot,
            GraphPreviewResult::SignalProbePreview preview);

    void resetExecutionState() { audioExecutor.resetExecutionState(); }
    size_t diagnosticProcessCount(const String& nodeId) const {
        return audioExecutor.diagnosticProcessCount(nodeId);
    }

private:
    void renderProbeAuditions(
            const NodeGraph& graph,
            GraphPresentationSnapshot& snapshot,
            GraphPresentationPerformanceMetrics& performance);

    static void renderProbeWaveforms(
            const NodeGraph& graph, GraphPresentationSnapshot& snapshot);

    GraphAudioExecutor audioExecutor;
};

}
