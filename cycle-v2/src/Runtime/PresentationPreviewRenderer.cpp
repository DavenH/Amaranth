#include <algorithm>

#include <App/AppConstants.h>

#include "Runtime/PresentationPreviewRenderer.h"
#include "Runtime/GraphPresentationSnapshot.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "Nodes/Control/ModulationSource.h"

namespace CycleV2 {

namespace {

constexpr size_t kCompactPreviewFrameCount = 512;

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

    const AudioExecutionSpec spec {
            kCompactPreviewFrameCount,
            44100.0,
            ChannelLayout::LinkedStereo
    };
    audioExecutor.prepareExecution(snapshot.compileResult.plan, spec);
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
                kCompactPreviewFrameCount,
                {},
                previewVoice);
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
            kCompactPreviewFrameCount,
            dirtyNodes,
            previewVoice,
            cancellationCheck);
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
    }
    performance.record(
            GraphPresentationPerformanceMetrics::Stage::PreviewExtraction,
            performance.timestamp() - extractionStartedAt);
    previewRendered = true;
    return true;
}

}
