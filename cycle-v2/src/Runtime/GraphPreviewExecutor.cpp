#include "GraphPreviewExecutor.h"

namespace CycleV2 {

namespace {

const NodePreviewResult* findSummary(
        const std::vector<NodePreviewResult>& summaries,
        const String& nodeId) {
    for (const auto& summary : summaries) {
        if (summary.nodeId == nodeId) {
            return &summary;
        }
    }

    return nullptr;
}

NodePreviewResult inputSummaryForStep(
        const GraphExecutionStep& step,
        const std::vector<NodePreviewResult>& summaries) {
    for (const auto& input : step.inputs) {
        const NodePreviewResult* summary = findSummary(summaries, input.sourceNodeId);

        if (summary != nullptr && (!summary->primary.empty() || !summary->secondary.empty())) {
            return *summary;
        }
    }

    return {};
}

}

GraphPreviewResult GraphPreviewExecutor::render(const GraphExecutionPlan& plan, size_t pointCount) const {
    GraphPreviewResult result;
    std::vector<NodePreviewResult> summaries;
    NodePreviewProcessorFactory factory;

    for (const auto& step : plan.steps) {
        auto inputPreview = inputSummaryForStep(step, summaries);

        if (!step.previewable) {
            if (!inputPreview.primary.empty() || !inputPreview.secondary.empty()) {
                inputPreview.nodeId = step.nodeId;
                inputPreview.role = PreviewModuleRole::None;
                summaries.push_back({
                        inputPreview.nodeId,
                        inputPreview.role,
                        std::move(inputPreview.primary),
                        std::move(inputPreview.secondary),
                        inputPreview.gridColumns,
                        inputPreview.gridRows,
                        inputPreview.domain
                });
            }

            continue;
        }

        auto processor = factory.create(step.previewRole);

        if (processor == nullptr) {
            continue;
        }

        PreviewProcessContext context;
        context.pointCount = pointCount;
        context.parameters = step.parameters;
        context.outputPorts.reserve(step.outputs.size());

        for (const auto& output : step.outputs) {
            context.outputPorts.push_back({
                    output.portId,
                    output.domain,
                    output.channelLayout
            });
        }

        context.inputSummary = inputPreview.primary;
        context.inputGrid = std::move(inputPreview.primary);
        context.inputGridColumns = inputPreview.gridColumns;
        context.inputGridRows = inputPreview.gridRows;
        context.domain = inputPreview.domain;
        processor->render(context);

        NodePreviewResult preview {
                step.nodeId,
                step.previewRole,
                std::move(context.primary),
                std::move(context.secondary),
                context.gridColumns,
                context.gridRows,
                context.domain
        };

        summaries.push_back(preview);
        result.nodes.push_back(std::move(preview));
    }

    return result;
}

}
