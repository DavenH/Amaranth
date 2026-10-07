#include <algorithm>

#include <App/AppConstants.h>

#include "Runtime/PresentationPreviewRenderer.h"
#include "Runtime/GraphPresentationSnapshot.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "Graph/DefaultOutputProbeResolver.h"
#include "Nodes/Control/ModulationSource.h"

namespace CycleV2 {

namespace {

constexpr size_t kCompactPreviewFrameCount = 512;
constexpr size_t kCompactPreviewColumnCount = 256;
constexpr size_t kExpandedProbeColumnCount = 512;
constexpr size_t kMaximumExpandedProbeRows = 512;

void reduceCompactProbeRows(GraphPreviewResult& result) {
    for (auto& probe : result.probes) {
        GraphPreviewExecutor::reduceProbeRows(probe, kCompactPreviewFrameCount);
    }
    if (result.defaultOutput.has_value()) {
        GraphPreviewExecutor::reduceProbeRows(
                *result.defaultOutput, kCompactPreviewFrameCount);
    }
    if (result.defaultOutputSpectrum.has_value()) {
        GraphPreviewExecutor::reduceProbeRows(
                *result.defaultOutputSpectrum, kCompactPreviewFrameCount);
    }
}

std::optional<CompiledSignalProbe> probeAddressFor(
        const NodeGraph& graph,
        const GraphExecutionPlan& plan,
        const String& probeId) {
    if (probeId == DefaultOutputProbeResolver::probeId) {
        return plan.defaultOutputProbe;
    }
    if (graph.findSignalProbe(probeId) == nullptr) {
        return std::nullopt;
    }
    const auto found = std::find_if(
            plan.signalProbes.begin(),
            plan.signalProbes.end(),
            [&](const auto& address) {
                return address.probeId == probeId;
            });
    return found != plan.signalProbes.end()
            ? std::optional<CompiledSignalProbe>(*found)
            : std::nullopt;
}

GraphPreviewResult::SignalProbePreview captureSelectedProbe(
        const GraphExecutionPlan& plan,
        const CompiledSignalProbe& address,
        size_t frameCount,
        int midiNote,
        int modWheelValue,
        GraphPresentationPerformanceMetrics& performance) {
    GraphAudioExecutor captureExecutor;

    AudioVoiceContext voice;
    voice.controls.noteNumber = jlimit(0, 127, midiNote);
    voice.controls.controllers[1] = (float) jlimit(0, 127, modWheelValue) / 127.f;
    voice.events.push_back({ NoteLifecycleType::NoteOn, 0, 0 });
    const uint64_t executionStartedAt = performance.timestamp();
    const GraphAudioResult audio = captureExecutor.processProbe(
            plan,
            address,
            frameCount,
            {},
            voice,
            kExpandedProbeColumnCount);
    performance.record(
            GraphPresentationPerformanceMetrics::Stage::ExpandedProbeExecution,
            performance.timestamp() - executionStartedAt);
    const uint64_t extractionStartedAt = performance.timestamp();
    auto result = GraphPreviewExecutor().captureProbe(plan, audio, address);
    performance.record(
            GraphPresentationPerformanceMetrics::Stage::ExpandedProbeExtraction,
            performance.timestamp() - extractionStartedAt);
    return result;
}

}

bool PresentationPreviewRenderer::render(
        const NodeGraph& graph,
        GraphPresentationSnapshot& snapshot,
        const std::vector<PlannedNodeProduct>& products,
        bool renderFullGraph,
        PresentationRefreshScope scope,
        bool& previewRendered,
        GraphPresentationPerformanceMetrics& performance,
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

    std::vector<uint8_t> dirtyNodes(snapshot.compileResult.plan.steps.size());
    for (const auto& product : products) {
        if (product.product != UpdateProduct::PreviewTraversal
                && product.product != UpdateProduct::CompactPreview) {
            continue;
        }
        const auto step = snapshot.compileResult.plan.dependencyIndex.stepIndexById.find(
                product.nodeId);
        if (step != snapshot.compileResult.plan.dependencyIndex.stepIndexById.end()) {
            dirtyNodes[static_cast<size_t>(step->second)] = 1;
        }
    }
    const uint64_t audioStartedAt = performance.timestamp();
    const GraphAudioResultView audio = audioExecutor.processIncrementalIndexed(
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
                &previewControls);
        reduceCompactProbeRows(snapshot.previewResult);
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
        int modWheelValue,
        GraphPresentationPerformanceMetrics& performance) const {
    const uint64_t startedAt = performance.timestamp();
    const auto address = probeAddressFor(graph, plan, probeId);
    if (!address.has_value()) {
        return std::nullopt;
    }
    auto preview = captureSelectedProbe(
            plan, *address, rasterRowCount, midiNote, modWheelValue, performance);
    const auto recordTotal = [&] {
        performance.record(
                GraphPresentationPerformanceMetrics::Stage::ExpandedProbeTotal,
                performance.timestamp() - startedAt);
    };
    if (!preview.connected) {
        recordTotal();
        return std::nullopt;
    }
    GraphPreviewExecutor::reduceProbeRows(
            preview,
            std::min(rasterRowCount, kMaximumExpandedProbeRows));
    recordTotal();
    return preview;
}

}
