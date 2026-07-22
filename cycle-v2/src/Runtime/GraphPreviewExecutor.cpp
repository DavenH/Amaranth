#include "GraphPreviewExecutor.h"

#include <functional>

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
        const std::vector<const NodeAudioResult*>& audioNodes,
        GraphPreviewResult& result) {
    std::vector<const NodeAudioResult*> index(plan.steps.size());
    size_t stepIndex = 0;
    for (const NodeAudioResult* node : audioNodes) {
        while (stepIndex < plan.steps.size()
                && plan.steps[stepIndex].nodeId != node->nodeId) {
            ++stepIndex;
            ++result.indexedNodeCount;
        }
        if (stepIndex == plan.steps.size()) {
            break;
        }

        index[stepIndex] = node;
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
        const std::vector<const NodeAudioResult*>& audioNodes,
        size_t pointCount,
        GraphPreviewResult result = {},
        const std::vector<uint8_t>* dirtyNodes = nullptr) {
    if (result.previewResultIndexByStep.size() != plan.steps.size()) {
        result.nodes.clear();
        result.previewResultIndexByStep.assign(plan.steps.size(), -1);
    }
    result.indexedNodeCount = 0;
    result.addressLookupCount = 0;
    result.aliasedInputCount = 0;
    result.reusedCapturedTraversalCount = 0;
    result.renderedNodeCount = 0;
    result.nodes.reserve(plan.steps.size());
    std::vector<PreviewResultView> workspace(plan.steps.size());
    const auto audioIndex = indexAudioResults(plan, audioNodes, result);
    NodePreviewProcessorFactory factory;

    std::vector<size_t> stepIndices;
    stepIndices.reserve(plan.steps.size());
    for (size_t stepIndex = 0; stepIndex < plan.steps.size(); ++stepIndex) {
        const int cachedIndex = result.previewResultIndexByStep[stepIndex];
        if (cachedIndex >= 0 && static_cast<size_t>(cachedIndex) < result.nodes.size()) {
            workspace[stepIndex] = viewOf(result.nodes[static_cast<size_t>(cachedIndex)]);
        }
        if (dirtyNodes == nullptr
                || (stepIndex < dirtyNodes->size() && (*dirtyNodes)[stepIndex] != 0)) {
            stepIndices.push_back(stepIndex);
        }
    }

    std::function<PreviewResultView(size_t)> resolveInput = [&](size_t stepIndex) {
        const auto& step = plan.steps[stepIndex];
        for (const auto& input : step.inputs) {
            ++result.addressLookupCount;
            if (input.sourceStepIndex < 0
                    || static_cast<size_t>(input.sourceStepIndex) >= workspace.size()) {
                continue;
            }
            const size_t sourceIndex = static_cast<size_t>(input.sourceStepIndex);
            if (workspace[sourceIndex].hasValues()) {
                return workspace[sourceIndex];
            }
            if (!plan.steps[sourceIndex].previewable) {
                workspace[sourceIndex] = resolveInput(sourceIndex);
                if (workspace[sourceIndex].hasValues()) {
                    return workspace[sourceIndex];
                }
            }
        }
        return PreviewResultView {};
    };

    for (const size_t stepIndex : stepIndices) {
        const auto& step = plan.steps[stepIndex];
        const auto inputPreview = dirtyNodes == nullptr
                ? inputPreviewForStep(step, workspace, result)
                : resolveInput(stepIndex);
        const int cachedIndex = result.previewResultIndexByStep[stepIndex];

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
        ++result.renderedNodeCount;
        if (context.reusedCapturedTraversal) {
            ++result.reusedCapturedTraversalCount;
        }

        NodePreviewResult preview {
                step.nodeId,
                step.previewRole,
                std::move(context.primary),
                std::move(context.secondary),
                context.gridColumns,
                context.gridRows,
                context.domain
        };
        if (cachedIndex >= 0 && static_cast<size_t>(cachedIndex) < result.nodes.size()) {
            result.nodes[static_cast<size_t>(cachedIndex)] = std::move(preview);
            workspace[stepIndex] = viewOf(result.nodes[static_cast<size_t>(cachedIndex)]);
        } else {
            result.nodes.push_back(std::move(preview));
            result.previewResultIndexByStep[stepIndex] = static_cast<int>(result.nodes.size() - 1);
            workspace[stepIndex] = viewOf(result.nodes.back());
        }
    }

    return result;
}

void appendProbePreviews(
        GraphPreviewResult& result,
        const GraphExecutionPlan& plan,
        const std::vector<const NodeAudioResult*>& audioNodes,
        const std::vector<SignalProbe>& probes) {
    const auto audioIndex = indexAudioResults(plan, audioNodes, result);

    result.probes.clear();
    result.probes.reserve(probes.size());
    for (size_t probeIndex = 0; probeIndex < probes.size(); ++probeIndex) {
        const auto& probe = probes[probeIndex];
        const SignalPayload* payload {};
        if (probeIndex < plan.signalProbes.size()) {
            const auto& address = plan.signalProbes[probeIndex];
            if (address.probeId == probe.id
                    && address.sourceStepIndex >= 0
                    && static_cast<size_t>(address.sourceStepIndex) < audioIndex.size()) {
                const NodeAudioResult* node = audioIndex[static_cast<size_t>(address.sourceStepIndex)];
                if (node != nullptr
                        && address.sourceOutputIndex >= 0
                        && static_cast<size_t>(address.sourceOutputIndex) < node->outputs.size()) {
                    payload = &node->outputs[static_cast<size_t>(address.sourceOutputIndex)].second;
                }
            }
        }
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
    return renderPreview(plan, {}, pointCount);
}

GraphPreviewResult GraphPreviewExecutor::render(
        const GraphExecutionPlan& plan,
        const GraphAudioResult& audioResult,
        size_t pointCount) const {
    std::vector<const NodeAudioResult*> nodes;
    nodes.reserve(audioResult.nodes.size());
    for (const auto& node : audioResult.nodes) {
        nodes.push_back(&node);
    }
    return renderPreview(plan, nodes, pointCount);
}

GraphPreviewResult GraphPreviewExecutor::render(
        const GraphExecutionPlan& plan,
        const GraphAudioResult& audioResult,
        const std::vector<SignalProbe>& probes,
        size_t pointCount) const {
    std::vector<const NodeAudioResult*> nodes;
    nodes.reserve(audioResult.nodes.size());
    for (const auto& node : audioResult.nodes) {
        nodes.push_back(&node);
    }
    GraphPreviewResult result = renderPreview(plan, nodes, pointCount);
    appendProbePreviews(result, plan, nodes, probes);
    return result;
}

GraphPreviewResult GraphPreviewExecutor::render(
        const GraphExecutionPlan& plan,
        const GraphAudioResultView& audioResult,
        const std::vector<SignalProbe>& probes,
        size_t pointCount) const {
    GraphPreviewResult result = renderPreview(plan, audioResult.nodes, pointCount);
    appendProbePreviews(result, plan, audioResult.nodes, probes);
    return result;
}

void GraphPreviewExecutor::renderIncremental(
        const GraphExecutionPlan& plan,
        const GraphAudioResultView& audioResult,
        const std::vector<SignalProbe>& probes,
        const std::vector<String>& dirtyNodeIds,
        size_t pointCount,
        GraphPreviewResult& result) const {
    std::vector<uint8_t> dirtyMask(plan.steps.size());
    for (const auto& nodeId : dirtyNodeIds) {
        const auto found = plan.dependencyIndex.nodeIndexById.find(nodeId);
        if (found != plan.dependencyIndex.nodeIndexById.end()) {
            dirtyMask[static_cast<size_t>(found->second)] = 1;
        }
    }
    renderIncremental(plan, audioResult, probes, dirtyMask, pointCount, result);
}

void GraphPreviewExecutor::renderIncremental(
        const GraphExecutionPlan& plan,
        const GraphAudioResultView& audioResult,
        const std::vector<SignalProbe>& probes,
        const std::vector<uint8_t>& dirtyNodes,
        size_t pointCount,
        GraphPreviewResult& result) const {
    result = renderPreview(plan, audioResult.nodes, pointCount, std::move(result), &dirtyNodes);
    appendProbePreviews(result, plan, audioResult.nodes, probes);
}

}
