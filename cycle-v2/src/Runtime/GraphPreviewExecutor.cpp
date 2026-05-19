#include "GraphPreviewExecutor.h"

namespace CycleV2 {

namespace {

const std::vector<float>* findSummary(
        const std::vector<NodePreviewResult>& summaries,
        const String& nodeId) {
    for (const auto& summary : summaries) {
        if (summary.nodeId == nodeId) {
            return &summary.primary;
        }
    }

    return nullptr;
}

std::vector<float> inputSummaryForStep(
        const GraphExecutionStep& step,
        const std::vector<NodePreviewResult>& summaries) {
    for (const auto& input : step.inputs) {
        const std::vector<float>* summary = findSummary(summaries, input.sourceNodeId);

        if (summary != nullptr && !summary->empty()) {
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
        auto inputSummary = inputSummaryForStep(step, summaries);

        if (!step.previewable) {
            if (!inputSummary.empty()) {
                summaries.push_back({
                        step.nodeId,
                        PreviewModuleRole::None,
                        std::move(inputSummary),
                        {}
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
        context.inputSummary = std::move(inputSummary);
        processor->render(context);

        NodePreviewResult preview {
                step.nodeId,
                step.previewRole,
                std::move(context.primary),
                std::move(context.secondary)
        };

        summaries.push_back(preview);
        result.nodes.push_back(std::move(preview));
    }

    return result;
}

}
