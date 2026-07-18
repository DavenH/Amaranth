#include "GraphPresentationModel.h"

namespace CycleV2 {

bool GraphPresentationModel::refresh(
        const NodeGraph& graph,
        uint64_t documentRevision,
        const GraphChangeSet& change) {
    requestedGraphRevision = documentRevision;
    const bool compile = current.graphRevision == 0 || requiresCompilation(change);
    const bool preview = compile || requiresPreview(change);

    GraphPresentationSnapshot next = current;
    next.graphRevision = documentRevision;
    if (compile) {
        next.compileResult = compiler.compile(graph);
        next.runtimeTrace = {};
        ++compilations;
        if (next.compileResult.succeeded()) {
            next.runtimeTrace = GraphRuntime().process(graph, next.compileResult.plan);
        }
    }

    if (preview) {
        next.previewResult = {};
        if (next.compileResult.succeeded()) {
            constexpr size_t previewFrameCount = 128;
            const AudioExecutionSpec spec {
                    previewFrameCount,
                    44100.0,
                    ChannelLayout::LinkedStereo
            };
            previewAudioExecutor.prepareExecution(next.compileResult.plan, spec);
            const GraphAudioResult audio = previewAudioExecutor.process(
                    graph,
                    next.compileResult.plan,
                    previewFrameCount);
            next.previewResult = GraphPreviewExecutor().render(
                    next.compileResult.plan,
                    audio,
                    graph.getSignalProbes(),
                    40);
            ++previewRenders;
        }
    }

    return acceptSnapshot(std::move(next));
}

bool GraphPresentationModel::acceptSnapshot(GraphPresentationSnapshot snapshotToAccept) {
    if (snapshotToAccept.graphRevision != requestedGraphRevision
            || snapshotToAccept.graphRevision < current.graphRevision) {
        return false;
    }

    current = std::move(snapshotToAccept);
    ++presentationRevision;
    return true;
}

GraphAudioResult GraphPresentationModel::captureAudio(
        const NodeGraph& graph,
        size_t frameCount) const {
    if (!current.compileResult.succeeded()) {
        return {};
    }

    AudioExecutionSpec spec;
    spec.maximumFrameCount = frameCount;
    GraphAudioExecutor captureExecutor;
    captureExecutor.prepareExecution(current.compileResult.plan, spec);
    return captureExecutor.process(graph, current.compileResult.plan, frameCount);
}

bool GraphPresentationModel::requiresCompilation(const GraphChangeSet& change) const {
    return change.topologyChanged
            || hasImpact(change.parameterImpacts, ParameterImpact::GraphSemantics)
            || hasImpact(change.parameterImpacts, ParameterImpact::DspConfiguration);
}

bool GraphPresentationModel::requiresPreview(const GraphChangeSet& change) const {
    return change.probesChanged
            || hasImpact(change.parameterImpacts, ParameterImpact::Preview)
            || hasImpact(change.parameterImpacts, ParameterImpact::Presentation);
}

}
