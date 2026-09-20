#include <atomic>
#include <cstring>
#include <functional>

#include "Runtime/GraphPreviewExecutor.h"

#include "Runtime/DefaultOutputPreview.h"
#include "Runtime/FingerprintBuilder.h"

namespace CycleV2 {

namespace {

size_t reducedProbeRowsForDomain(PortDomain domain, size_t timeRows) {
    if (domain == PortDomain::SpectralMagnitudeSignal
            || domain == PortDomain::SpectralPhaseSignal) {
        return timeRows / 2 + 1;
    }
    return timeRows;
}

uint64_t nextPreviewContentRevision() {
    static std::atomic<uint64_t> nextRevision { 1 };
    return nextRevision.fetch_add(1, std::memory_order_relaxed);
}

}

void GraphPreviewExecutor::reduceProbeRows(
        GraphPreviewResult::SignalProbePreview& preview,
        size_t timeRows) {
    if (!preview.connected || preview.gridColumns == 0 || preview.gridRows == 0) {
        return;
    }
    const size_t targetRows = reducedProbeRowsForDomain(preview.domain, timeRows);
    if (targetRows == 0 || targetRows >= preview.gridRows) {
        return;
    }

    const bool retainsNyquist = preview.domain == PortDomain::SpectralMagnitudeSignal
            || preview.domain == PortDomain::SpectralPhaseSignal;
    const size_t sourceIntervals = retainsNyquist
            ? preview.gridRows - 1
            : preview.gridRows;
    const size_t targetIntervals = retainsNyquist
            ? targetRows - 1
            : targetRows;
    if (targetIntervals == 0 || sourceIntervals % targetIntervals != 0) {
        return;
    }

    const int factor = (int) (sourceIntervals / targetIntervals);
    std::vector<float> reduced(preview.gridColumns * targetRows);
    for (size_t column = 0; column < preview.gridColumns; ++column) {
        Buffer<float> source(
                preview.values.data() + column * preview.gridRows,
                (int) preview.gridRows);
        Buffer<float> destination(
                reduced.data() + column * targetRows,
                (int) targetRows);
        destination.downsampleFrom(source, factor);
    }
    preview.values = std::move(reduced);
    preview.gridRows = targetRows;
}

uint64_t nodePreviewResultFingerprint(const NodePreviewResult& preview) {
    FingerprintBuilder fingerprint;
    fingerprint
            .add(preview.nodeId)
            .add((uint64_t) preview.role)
            .add((uint64_t) preview.primary.size());
    for (const float value : preview.primary) {
        uint32_t bits {};
        std::memcpy(&bits, &value, sizeof(bits));
        fingerprint.add(bits);
    }
    fingerprint.add((uint64_t) preview.secondary.size());
    for (const float value : preview.secondary) {
        uint32_t bits {};
        std::memcpy(&bits, &value, sizeof(bits));
        fingerprint.add(bits);
    }
    return fingerprint
            .add((uint64_t) preview.gridColumns)
            .add((uint64_t) preview.gridRows)
            .add((uint64_t) preview.domain)
            .add((uint64_t) preview.frequencySampling)
            .add((uint64_t) preview.frequencyMidiNote)
            .value();
}

bool nodePreviewResultsHaveEqualContent(
        const NodePreviewResult& first,
        const NodePreviewResult& second) {
    return first.nodeId == second.nodeId
            && first.role == second.role
            && first.primary == second.primary
            && first.secondary == second.secondary
            && first.gridColumns == second.gridColumns
            && first.gridRows == second.gridRows
            && first.domain == second.domain
            && first.frequencySampling == second.frequencySampling
            && first.frequencyMidiNote == second.frequencyMidiNote;
}

namespace {

struct PreviewResultView {
    const std::vector<float>* primary {};
    const std::vector<float>* secondary {};
    size_t gridColumns {};
    size_t gridRows {};
    PortDomain domain { PortDomain::TimeSignal };
    TraversalGridFrequencySampling frequencySampling {
            TraversalGridFrequencySampling::LinearBins };
    int frequencyMidiNote { 48 };

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
        context.input.gridSize = 0;
        context.input.gridColumns = 0;
        context.input.gridRows = 0;
        return;
    }

    context.input.grid = input->traversalGrid.values.data();
    context.input.gridSize = input->traversalGrid.values.size();
    context.input.gridColumns = input->traversalGrid.columns;
    context.input.gridRows = input->traversalGrid.rows;
    context.input.domain = input->traversalGrid.metadata.valueDomain;
    context.domain = context.input.domain;
    context.frequencySampling = input->traversalGrid.metadata.frequencySampling;
    context.frequencyMidiNote = input->traversalGrid.metadata.frequencyMidiNote;
}

PreviewResultView viewOf(const NodePreviewResult& preview) {
    return {
            &preview.primary,
            &preview.secondary,
            preview.gridColumns,
            preview.gridRows,
            preview.domain,
            preview.frequencySampling,
            preview.frequencyMidiNote
    };
}

GraphPreviewResult renderPreview(
        const GraphExecutionPlan& plan,
        const std::vector<const NodeAudioResult*>& audioNodes,
        size_t pointCount,
        GraphPreviewResult result = {},
        const std::vector<uint8_t>* dirtyNodes = nullptr,
        const PreviewControlContext* controlContext = nullptr) {
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
        context.controlContext = controlContext;
        context.configuration = &step.configuration;
        if (stepIndex < audioIndex.size() && audioIndex[stepIndex] != nullptr) {
            context.capturedOutput = &audioIndex[stepIndex]->output;
            if (context.capturedOutput->traversalGrid.isValid()) {
                context.frequencySampling = context.capturedOutput
                        ->traversalGrid.metadata.frequencySampling;
                context.frequencyMidiNote = context.capturedOutput
                        ->traversalGrid.metadata.frequencyMidiNote;
            }
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
        if (step.previewRole != PreviewModuleRole::SignalSpy
                && inputPreview.primary != nullptr) {
            context.input.grid = inputPreview.primary->data();
            context.input.gridSize = inputPreview.primary->size();
            context.input.gridColumns = inputPreview.gridColumns;
            context.input.gridRows = inputPreview.gridRows;
            context.input.domain = inputPreview.domain;
            context.domain = inputPreview.domain;
            context.frequencySampling = inputPreview.frequencySampling;
            context.frequencyMidiNote = inputPreview.frequencyMidiNote;
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
                context.domain,
                context.frequencySampling,
                context.frequencyMidiNote
        };
        if (cachedIndex >= 0 && static_cast<size_t>(cachedIndex) < result.nodes.size()) {
            const NodePreviewResult& cached = result.nodes[static_cast<size_t>(cachedIndex)];
            preview.contentRevision = nodePreviewResultsHaveEqualContent(preview, cached)
                    ? cached.contentRevision
                    : nextPreviewContentRevision();
            result.nodes[static_cast<size_t>(cachedIndex)] = std::move(preview);
            workspace[stepIndex] = viewOf(result.nodes[static_cast<size_t>(cachedIndex)]);
        } else {
            preview.contentRevision = nextPreviewContentRevision();
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

    const auto capture = [&](const CompiledSignalProbe& address) {
        GraphPreviewResult::SignalProbePreview preview;
        preview.probeId = address.probeId;
        if (address.sourceStepIndex < 0
                || (size_t) address.sourceStepIndex >= audioIndex.size()) {
            return preview;
        }
        const NodeAudioResult* node = audioIndex[(size_t) address.sourceStepIndex];
        if (node == nullptr || address.sourceOutputIndex < 0
                || (size_t) address.sourceOutputIndex >= node->outputs.size()) {
            return preview;
        }
        const size_t outputIndex = (size_t) address.sourceOutputIndex;
        const SignalPayload& payload = node->outputs[outputIndex].second;
        const SignalTraversalGrid* grid = &payload.traversalGrid;
        if (outputIndex < node->probeTraversalGrids.size()) {
            grid = &node->probeTraversalGrids[outputIndex];
        }
        preview.connected = grid->isValid();
        preview.sourceRole = plan.steps[(size_t) address.sourceStepIndex].previewRole;
        if (!preview.connected) {
            return preview;
        }
        preview.values.assign(grid->values.begin(), grid->values.end());
        preview.gridColumns = grid->columns;
        preview.gridRows = grid->rows;
        preview.domain = grid->metadata.valueDomain;
        preview.channelLayout = payload.channelLayout;
        preview.frequencySampling = grid->metadata.frequencySampling;
        preview.frequencyMidiNote = grid->metadata.frequencyMidiNote;
        return preview;
    };

    result.defaultOutput = plan.defaultOutputProbe.has_value()
            ? std::optional<GraphPreviewResult::SignalProbePreview>(
                    capture(*plan.defaultOutputProbe))
            : std::nullopt;
    result.defaultOutputSpectrum = result.defaultOutput.has_value()
            && result.defaultOutput->connected
            ? std::optional<GraphPreviewResult::SignalProbePreview>(
                    DefaultOutputPreview::spectrum(*result.defaultOutput))
            : std::nullopt;
    if (result.defaultOutput.has_value() && result.defaultOutput->connected) {
        result.defaultOutput = DefaultOutputPreview::normalizedTime(
                *result.defaultOutput);
    }

    result.probes.clear();
    result.probes.reserve(probes.size());
    for (size_t probeIndex = 0; probeIndex < probes.size(); ++probeIndex) {
        const auto& probe = probes[probeIndex];
        if (probeIndex < plan.signalProbes.size()) {
            const auto& address = plan.signalProbes[probeIndex];
            if (address.probeId == probe.id) {
                result.probes.push_back(capture(address));
                continue;
            }
        }
        GraphPreviewResult::SignalProbePreview preview;
        preview.probeId = probe.id;
        result.probes.push_back(std::move(preview));
    }
}

}

GraphPreviewResult GraphPreviewExecutor::render(const GraphExecutionPlan& plan, size_t pointCount) const {
    return renderPreview(plan, {}, pointCount);
}

GraphPreviewResult GraphPreviewExecutor::render(
        const GraphExecutionPlan& plan,
        size_t pointCount,
        const PreviewControlContext& controlContext) const {
    return renderPreview(plan, {}, pointCount, {}, nullptr, &controlContext);
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
        size_t pointCount,
        const PreviewControlContext* controlContext) const {
    std::vector<const NodeAudioResult*> nodes;
    nodes.reserve(audioResult.nodes.size());
    for (const auto& node : audioResult.nodes) {
        nodes.push_back(&node);
    }
    GraphPreviewResult result = renderPreview(
            plan, nodes, pointCount, {}, nullptr, controlContext);
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
        const auto found = plan.dependencyIndex.stepIndexById.find(nodeId);
        if (found != plan.dependencyIndex.stepIndexById.end()) {
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
        GraphPreviewResult& result,
        const PreviewControlContext* controlContext) const {
    result = renderPreview(
            plan,
            audioResult.nodes,
            pointCount,
            std::move(result),
            &dirtyNodes,
            controlContext);
    appendProbePreviews(result, plan, audioResult.nodes, probes);
}

void GraphPreviewExecutor::publishLocalNodePreview(
        GraphPreviewResult& result,
        size_t stepIndex,
        NodePreviewResult preview) {
    if (stepIndex >= result.previewResultIndexByStep.size()) {
        return;
    }

    const int cachedIndex = result.previewResultIndexByStep[stepIndex];
    if (cachedIndex >= 0 && static_cast<size_t>(cachedIndex) < result.nodes.size()) {
        const NodePreviewResult& cached = result.nodes[static_cast<size_t>(cachedIndex)];
        preview.contentRevision = nodePreviewResultsHaveEqualContent(preview, cached)
                ? cached.contentRevision
                : nextPreviewContentRevision();
        result.nodes[static_cast<size_t>(cachedIndex)] = std::move(preview);
        return;
    }

    preview.contentRevision = nextPreviewContentRevision();
    result.nodes.push_back(std::move(preview));
    result.previewResultIndexByStep[stepIndex] = static_cast<int>(result.nodes.size() - 1);
}

void GraphPreviewExecutor::renderNodePreviewsIncremental(
        const GraphExecutionPlan& plan,
        const GraphAudioResultView& audioResult,
        const std::vector<uint8_t>& dirtyNodes,
        size_t pointCount,
        GraphPreviewResult& result,
        const PreviewControlContext* controlContext) const {
    auto probes = std::move(result.probes);
    auto defaultOutput = std::move(result.defaultOutput);
    auto defaultOutputSpectrum = std::move(result.defaultOutputSpectrum);
    result = renderPreview(
            plan,
            audioResult.nodes,
            pointCount,
            std::move(result),
            &dirtyNodes,
            controlContext);
    result.probes = std::move(probes);
    result.defaultOutput = std::move(defaultOutput);
    result.defaultOutputSpectrum = std::move(defaultOutputSpectrum);
}

}
