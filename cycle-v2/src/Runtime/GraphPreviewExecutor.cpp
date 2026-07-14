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

const SignalPayload* findAudioOutput(
        const GraphAudioResult& audioResult,
        const String& nodeId,
        const String& portId) {
    for (const auto& node : audioResult.nodes) {
        if (node.nodeId != nodeId) {
            continue;
        }

        for (const auto& output : node.outputs) {
            if (output.first == portId) {
                return &output.second;
            }
        }

        return &node.output;
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

const SignalPayload* inputPayloadForStep(
        const GraphExecutionStep& step,
        const GraphAudioResult& audioResult) {
    for (const auto& input : step.inputs) {
        const SignalPayload* payload = findAudioOutput(audioResult, input.sourceNodeId, input.sourcePortId);

        if (payload != nullptr) {
            return payload;
        }
    }

    return nullptr;
}

void addAudioTraversalGridToContext(
        PreviewProcessContext& context,
        const GraphExecutionStep& step,
        const GraphAudioResult* audioResult) {
    if (audioResult == nullptr || step.previewRole != PreviewModuleRole::SignalSpy) {
        return;
    }

    const SignalPayload* input = inputPayloadForStep(step, *audioResult);

    if (input == nullptr || !input->traversalGrid.isValid()) {
        context.inputGrid.clear();
        context.inputGridColumns = 0;
        context.inputGridRows = 0;
        return;
    }

    context.inputGrid = input->traversalGrid.values;
    context.inputGridColumns = input->traversalGrid.columns;
    context.inputGridRows = input->traversalGrid.rows;
    context.domain = input->traversalGrid.metadata.valueDomain;
}

GraphPreviewResult renderPreview(
        const GraphExecutionPlan& plan,
        const GraphAudioResult* audioResult,
        size_t pointCount) {
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
        context.configuration = &step.configuration;
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
        if (step.previewRole != PreviewModuleRole::SignalSpy) {
            context.inputGrid = std::move(inputPreview.primary);
            context.inputGridColumns = inputPreview.gridColumns;
            context.inputGridRows = inputPreview.gridRows;
            context.domain = inputPreview.domain;
        }
        addAudioTraversalGridToContext(context, step, audioResult);
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

GraphPreviewResult GraphPreviewExecutor::render(const GraphExecutionPlan& plan, size_t pointCount) const {
    return renderPreview(plan, nullptr, pointCount);
}

GraphPreviewResult GraphPreviewExecutor::render(
        const GraphExecutionPlan& plan,
        const GraphAudioResult& audioResult,
        size_t pointCount) const {
    return renderPreview(plan, &audioResult, pointCount);
}

}
