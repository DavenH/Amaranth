#include "GraphPreviewExecutor.h"

namespace CycleV2 {

GraphPreviewResult GraphPreviewExecutor::render(const GraphExecutionPlan& plan, size_t pointCount) const {
    GraphPreviewResult result;
    NodePreviewProcessorFactory factory;

    for (const auto& step : plan.steps) {
        if (!step.previewable) {
            continue;
        }

        auto processor = factory.create(step.previewRole);

        if (processor == nullptr) {
            continue;
        }

        PreviewProcessContext context;
        context.pointCount = pointCount;
        context.parameters = step.parameters;
        processor->render(context);

        result.nodes.push_back({
                step.nodeId,
                step.previewRole,
                std::move(context.primary),
                std::move(context.secondary)
        });
    }

    return result;
}

}
