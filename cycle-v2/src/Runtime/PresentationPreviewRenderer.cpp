#include <algorithm>

#include <App/AppConstants.h>

#include "Runtime/PresentationPreviewRenderer.h"
#include "Runtime/GraphPresentationSnapshot.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "Graph/DefaultOutputProbeResolver.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Nodes/Control/ModulationSource.h"

namespace CycleV2 {

namespace {

constexpr size_t kCompactPreviewFrameCount = 512;
constexpr size_t kCompactPreviewColumnCount = 256;
constexpr size_t kExpandedProbeColumnCount = 512;
constexpr size_t kMaximumExpandedProbeRows = 512;

void reduceCompactProbeRows(
        GraphPreviewResult& result,
        const GraphExecutionPlan* plan = nullptr,
        const std::vector<size_t>* dirtyStepIndices = nullptr) {
    const bool incremental = plan != nullptr
            && dirtyStepIndices != nullptr
            && plan->observationIndex != nullptr
            && result.probes.size() == plan->signalProbes.size();
    if (incremental) {
        for (const size_t stepIndex : *dirtyStepIndices) {
            if (stepIndex >= plan->observationIndex->probeIndicesByStep.size()) {
                continue;
            }
            for (const size_t probeIndex :
                    plan->observationIndex->probeIndicesByStep[stepIndex]) {
                InteractionComplexityDiagnostics::recordPreviewProbeVisit();
                GraphPreviewExecutor::reduceProbeRows(
                        result.probes[probeIndex], kCompactPreviewFrameCount);
            }
        }
    } else {
        for (auto& probe : result.probes) {
            InteractionComplexityDiagnostics::recordPreviewProbeVisit();
            GraphPreviewExecutor::reduceProbeRows(probe, kCompactPreviewFrameCount);
        }
    }
    const bool refreshDefault = !incremental
            || (plan->defaultOutputProbe.has_value()
                    && plan->defaultOutputProbe->sourceStepIndex >= 0
                    && std::find(
                            dirtyStepIndices->begin(),
                            dirtyStepIndices->end(),
                            static_cast<size_t>(
                                    plan->defaultOutputProbe->sourceStepIndex))
                            != dirtyStepIndices->end());
    if (refreshDefault && result.defaultOutput.has_value()) {
        GraphPreviewExecutor::reduceProbeRows(
                *result.defaultOutput, kCompactPreviewFrameCount);
    }
    if (refreshDefault && result.defaultOutputSpectrum.has_value()) {
        GraphPreviewExecutor::reduceProbeRows(
                *result.defaultOutputSpectrum, kCompactPreviewFrameCount);
    }
}

GraphPreviewResult captureProbePreviews(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        size_t frameCount,
        int midiNote,
        int modWheelValue) {
    GraphAudioExecutor captureExecutor;

    AudioVoiceContext voice;
    voice.controls.noteNumber = jlimit(0, 127, midiNote);
    voice.controls.controllers[1] = (float) jlimit(0, 127, modWheelValue) / 127.f;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    const GraphAudioResult audio = captureExecutor.process(
            graph,
            plan,
            frameCount,
            {},
            voice,
            kExpandedProbeColumnCount);
    return GraphPreviewExecutor().render(
            plan,
            audio,
            graph.getSignalProbes(),
            frameCount);
}

}

void PresentationPreviewRenderer::preparePointPreviewPlan(
        const GraphPresentationSnapshot& snapshot) {
    AudioExecutionSpec spec;
    spec.maximumFrameCount = jmax(
            kCompactPreviewFrameCount,
            GraphPreviewExecutor::periodRowsForMidiNote(snapshot.previewMidiNote));
    spec.traversalColumnCount = kCompactPreviewColumnCount;
    audioExecutor.preparePointPreviewPlan(snapshot.compileResult.plan, spec);
}

bool PresentationPreviewRenderer::render(
        const NodeGraph& graph,
        GraphPresentationSnapshot& snapshot,
        const std::vector<PlannedNodeProduct>& products,
        bool renderFullGraph,
        PresentationRefreshScope scope,
        bool& previewRendered,
        GraphPresentationPerformanceMetrics& performance,
        bool stableProbeAddresses,
        GraphAudioExecutor::CancellationCheck cancellationCheck) {
    previewRendered = false;
    const bool hasPreviewWork = std::any_of(
            products.begin(), products.end(), [](const auto& product) {
                return product.product == UpdateProduct::PreviewTraversal
                        || product.product == UpdateProduct::CompactPreview
                        || product.product == UpdateProduct::ProbePreview;
            });
    if (!hasPreviewWork || !snapshot.compileResult.succeeded()) {
        return true;
    }

    const size_t sourceFrameCount = jmax(
            kCompactPreviewFrameCount,
            GraphPreviewExecutor::periodRowsForMidiNote(snapshot.previewMidiNote));
    AudioVoiceContext previewVoice;
    previewVoice.controls.noteNumber = snapshot.previewMidiNote;
    previewVoice.controls.controllers[1]
            = (float) snapshot.previewModWheelValue / 127.f;
    previewVoice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    PreviewControlContext previewControls;
    previewControls.noteNumber = snapshot.previewMidiNote;
    previewControls.controllers[1]
            = (float) snapshot.previewModWheelValue / 127.f;
    previewControls.lowestNote = Constants::LowestMidiNote;
    previewControls.highestNote = Constants::HighestMidiNote;
    previewControls.traverseVoiceTime = true;
    if (renderFullGraph) {
        const uint64_t audioStartedAt = performance.timestamp();
        const GraphAudioResult audio = audioExecutor.process(
                graph,
                snapshot.compileResult.plan,
                sourceFrameCount,
                {},
                previewVoice,
                kCompactPreviewColumnCount);
        performance.record(
                GraphPresentationPerformanceMetrics::Stage::PreviewAudio,
                performance.timestamp() - audioStartedAt);
        const uint64_t extractionStartedAt = performance.timestamp();
        snapshot.previewResult = GraphPreviewExecutor().render(
                snapshot.compileResult.plan,
                audio,
                graph.getSignalProbes(),
                40,
                &previewControls);
        reduceCompactProbeRows(snapshot.previewResult);
        performance.record(
                GraphPresentationPerformanceMetrics::Stage::PreviewExtraction,
                performance.timestamp() - extractionStartedAt);
        previewRendered = true;
        return true;
    }

    const bool sparse = stableProbeAddresses
            && scope != PresentationRefreshScope::LocalEditor;
    std::vector<uint8_t> dirtyNodes;
    if (!sparse) {
        dirtyNodes.resize(snapshot.compileResult.plan.steps.size());
    }
    InteractionComplexityDiagnostics::recordPreviewDirtyMaskSlots(dirtyNodes.size());
    std::vector<size_t> dirtyStepIndices;
    for (const auto& product : products) {
        if (product.product != UpdateProduct::PreviewTraversal
                && product.product != UpdateProduct::CompactPreview) {
            continue;
        }
        const auto step = snapshot.compileResult.plan.dependencyIndex.stepIndexById.find(
                product.nodeId);
        if (step != snapshot.compileResult.plan.dependencyIndex.stepIndexById.end()) {
            const auto index = static_cast<size_t>(step->second);
            if (sparse) {
                if (std::find(dirtyStepIndices.begin(), dirtyStepIndices.end(), index)
                        == dirtyStepIndices.end()) {
                    dirtyStepIndices.push_back(index);
                }
            } else if (dirtyNodes[index] == 0) {
                dirtyNodes[index] = 1;
                dirtyStepIndices.push_back(index);
            }
        }
    }
    const uint64_t audioStartedAt = performance.timestamp();
    const GraphAudioResultView audio = sparse
            ? audioExecutor.processIncrementalSteps(
                    graph,
                    snapshot.compileResult.plan,
                    sourceFrameCount,
                    dirtyStepIndices,
                    previewVoice,
                    cancellationCheck,
                    kCompactPreviewColumnCount)
            : audioExecutor.processIncrementalIndexed(
                    graph,
                    snapshot.compileResult.plan,
                    sourceFrameCount,
                    dirtyNodes,
                    previewVoice,
                    cancellationCheck,
                    kCompactPreviewColumnCount);
    performance.record(
            GraphPresentationPerformanceMetrics::Stage::PreviewAudio,
            performance.timestamp() - audioStartedAt);
    if (audio.cancelled || (cancellationCheck && !cancellationCheck())) {
        return false;
    }
    const uint64_t extractionStartedAt = performance.timestamp();
    if (scope == PresentationRefreshScope::LocalEditor) {
        GraphPreviewExecutor().renderNodePreviewsIncremental(
                snapshot.compileResult.plan,
                audio,
                dirtyNodes,
                40,
                snapshot.previewResult,
                &previewControls);
    } else {
        GraphPreviewExecutor().renderIncremental(
                snapshot.compileResult.plan,
                audio,
                graph.getSignalProbes(),
                dirtyNodes,
                40,
                snapshot.previewResult,
                &previewControls,
                stableProbeAddresses ? &dirtyStepIndices : nullptr);
        reduceCompactProbeRows(
                snapshot.previewResult,
                stableProbeAddresses ? &snapshot.compileResult.plan : nullptr,
                stableProbeAddresses ? &dirtyStepIndices : nullptr);
    }
    performance.record(
            GraphPresentationPerformanceMetrics::Stage::PreviewExtraction,
            performance.timestamp() - extractionStartedAt);
    previewRendered = true;
    return true;
}

std::optional<GraphPreviewResult::SignalProbePreview>
PresentationPreviewRenderer::captureProbePreview(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        const String& probeId,
        size_t rasterRowCount,
        int midiNote,
        int modWheelValue) const {
    GraphPreviewResult previews = captureProbePreviews(
            graph,
            plan,
            rasterRowCount,
            midiNote,
            modWheelValue);
    if (probeId == DefaultOutputProbeResolver::probeId) {
        if (!previews.defaultOutput.has_value()
                || !previews.defaultOutput->connected) {
            return std::nullopt;
        }
        GraphPreviewExecutor::reduceProbeRows(
                *previews.defaultOutput,
                std::min(rasterRowCount, kMaximumExpandedProbeRows));
        return previews.defaultOutput;
    }
    auto found = std::find_if(
            previews.probes.begin(),
            previews.probes.end(),
            [&](const auto& preview) {
                return preview.probeId == probeId;
            });
    if (found == previews.probes.end() || !found->connected) {
        return std::nullopt;
    }

    GraphPreviewExecutor::reduceProbeRows(
            *found,
            std::min(rasterRowCount, kMaximumExpandedProbeRows));
    return *found;
}

}
