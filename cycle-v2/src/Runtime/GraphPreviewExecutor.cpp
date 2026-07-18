#include "GraphPreviewExecutor.h"

#include <unordered_map>

namespace CycleV2 {

namespace {

struct PreviewResultView {
    const std::vector<float>* primary {};
    const std::vector<float>* secondary {};
    size_t gridColumns {};
    size_t gridRows {};
    PortDomain domain { PortDomain::TimeSignal };

    bool hasValues() const {
        return (primary != nullptr && !primary->empty())
                || (secondary != nullptr && !secondary->empty());
    }
};

std::vector<const NodeAudioResult*> indexAudioResults(
        const GraphExecutionPlan& plan,
        const GraphAudioResult* audioResult,
        GraphPreviewResult& result) {
    std::vector<const NodeAudioResult*> index(plan.steps.size());
    if (audioResult == nullptr) {
        return index;
    }

    size_t stepIndex = 0;
    for (const auto& node : audioResult->nodes) {
        while (stepIndex < plan.steps.size()
                && plan.steps[stepIndex].nodeId != node.nodeId) {
            ++stepIndex;
            ++result.indexedNodeCount;
        }
        if (stepIndex == plan.steps.size()) {
            break;
        }

        index[stepIndex] = &node;
        ++stepIndex;
        ++result.indexedNodeCount;
    }
    result.indexedNodeCount += plan.steps.size() - stepIndex;
    return index;
}

PreviewResultView inputPreviewForStep(
        const GraphExecutionStep& step,
        const std::vector<PreviewResultView>& workspace,
        GraphPreviewResult& result) {
    for (const auto& input : step.inputs) {
        ++result.addressLookupCount;
        if (input.sourceStepIndex < 0
                || (size_t) input.sourceStepIndex >= workspace.size()) {
            continue;
        }

        const auto& source = workspace[(size_t) input.sourceStepIndex];
        if (source.hasValues()) {
            return source;
        }
    }

    return {};
}

const SignalPayload* inputPayloadForStep(
        const GraphExecutionStep& step,
        const std::vector<const NodeAudioResult*>& audioIndex,
        GraphPreviewResult& result) {
    for (const auto& input : step.inputs) {
        ++result.addressLookupCount;
        if (input.sourceStepIndex < 0
                || (size_t) input.sourceStepIndex >= audioIndex.size()) {
            continue;
        }

        const NodeAudioResult* source = audioIndex[(size_t) input.sourceStepIndex];
        if (source == nullptr) {
            continue;
        }
        if (input.sourceOutputIndex >= 0
                && (size_t) input.sourceOutputIndex < source->outputs.size()) {
            return &source->outputs[(size_t) input.sourceOutputIndex].second;
        }

        return &source->output;
    }

    return nullptr;
}

void addAudioTraversalGridToContext(
        PreviewProcessContext& context,
        const GraphExecutionStep& step,
        const std::vector<const NodeAudioResult*>& audioIndex,
        GraphPreviewResult& result) {
    if (step.previewRole != PreviewModuleRole::SignalSpy) {
        return;
    }

    const SignalPayload* input = inputPayloadForStep(step, audioIndex, result);
    if (input == nullptr || !input->traversalGrid.isValid()) {
        context.input.grid = nullptr;
        context.input.gridColumns = 0;
        context.input.gridRows = 0;
        return;
    }

    context.input.grid = &input->traversalGrid.values;
    context.input.gridColumns = input->traversalGrid.columns;
    context.input.gridRows = input->traversalGrid.rows;
    context.input.domain = input->traversalGrid.metadata.valueDomain;
    context.domain = context.input.domain;
}

PreviewResultView viewOf(const NodePreviewResult& preview) {
    return {
            &preview.primary,
            &preview.secondary,
            preview.gridColumns,
            preview.gridRows,
            preview.domain
    };
}

GraphPreviewResult renderPreview(
        const GraphExecutionPlan& plan,
        const GraphAudioResult* audioResult,
        size_t pointCount) {
    GraphPreviewResult result;
    result.nodes.reserve(plan.steps.size());
    std::vector<PreviewResultView> workspace(plan.steps.size());
    const auto audioIndex = indexAudioResults(plan, audioResult, result);
    NodePreviewProcessorFactory factory;

    for (size_t stepIndex = 0; stepIndex < plan.steps.size(); ++stepIndex) {
        const auto& step = plan.steps[stepIndex];
        const auto inputPreview = inputPreviewForStep(step, workspace, result);

        if (!step.previewable) {
            workspace[stepIndex] = inputPreview;
            if (inputPreview.hasValues()) {
                ++result.aliasedInputCount;
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
        if (stepIndex < audioIndex.size() && audioIndex[stepIndex] != nullptr) {
            context.capturedOutput = &audioIndex[stepIndex]->output;
        }
        context.parameters = step.parameters;
        context.outputPorts.reserve(step.outputs.size());

        for (const auto& output : step.outputs) {
            context.outputPorts.push_back({
                    output.portId,
                    output.domain,
                    output.channelLayout
            });
        }

        context.input.summary = inputPreview.primary;
        if (step.previewRole != PreviewModuleRole::SignalSpy) {
            context.input.grid = inputPreview.primary;
            context.input.gridColumns = inputPreview.gridColumns;
            context.input.gridRows = inputPreview.gridRows;
            context.input.domain = inputPreview.domain;
            context.domain = inputPreview.domain;
        }
        addAudioTraversalGridToContext(context, step, audioIndex, result);
        processor->render(context);
        if (context.reusedCapturedTraversal) {
            ++result.reusedCapturedTraversalCount;
        }

        result.nodes.push_back({
                step.nodeId,
                step.previewRole,
                std::move(context.primary),
                std::move(context.secondary),
                context.gridColumns,
                context.gridRows,
                context.domain
        });
        workspace[stepIndex] = viewOf(result.nodes.back());
    }

    return result;
}

struct StringHash {
    size_t operator()(const String& value) const {
        return (size_t) value.hashCode64();
    }
};

void appendProbePreviews(
        GraphPreviewResult& result,
        const GraphAudioResult& audioResult,
        const std::vector<SignalProbe>& probes) {
    std::unordered_map<String, const SignalPayload*, StringHash> outputs;
    for (const auto& node : audioResult.nodes) {
        for (const auto& output : node.outputs) {
            outputs.emplace(node.nodeId + "\n" + output.first, &output.second);
        }
    }

    result.probes.reserve(probes.size());
    for (const auto& probe : probes) {
        const auto found = outputs.find(probe.sourceNodeId + "\n" + probe.sourcePortId);
        const SignalPayload* payload = found != outputs.end() ? found->second : nullptr;
        const bool connected = payload != nullptr && payload->traversalGrid.isValid();
        GraphPreviewResult::SignalProbePreview preview;
        preview.probeId = probe.id;
        preview.connected = connected;
        if (connected) {
            preview.values = payload->traversalGrid.values;
            preview.gridColumns = payload->traversalGrid.columns;
            preview.gridRows = payload->traversalGrid.rows;
            preview.domain = payload->traversalGrid.metadata.valueDomain;
            preview.channelLayout = payload->channelLayout;
        }
        result.probes.push_back(std::move(preview));
    }
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

GraphPreviewResult GraphPreviewExecutor::render(
        const GraphExecutionPlan& plan,
        const GraphAudioResult& audioResult,
        const std::vector<SignalProbe>& probes,
        size_t pointCount) const {
    GraphPreviewResult result = renderPreview(plan, &audioResult, pointCount);
    appendProbePreviews(result, audioResult, probes);
    return result;
}

}
