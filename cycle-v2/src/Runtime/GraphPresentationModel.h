#pragma once

#include "GraphAudioExecutor.h"
#include "GraphPreviewExecutor.h"
#include "GraphRuntime.h"
#include "../Graph/GraphCompiler.h"
#include "../Graph/GraphEditor.h"

namespace CycleV2 {

struct GraphPresentationSnapshot {
    uint64_t graphRevision {};
    GraphCompileResult compileResult;
    RuntimeProcessTrace runtimeTrace;
    GraphPreviewResult previewResult;
};

class GraphPresentationModel {
public:
    bool refresh(
            const NodeGraph& graph,
            uint64_t documentRevision,
            const GraphChangeSet& change = {});
    bool acceptSnapshot(GraphPresentationSnapshot snapshot);

    const GraphPresentationSnapshot& snapshot() const { return current; }
    const GraphCompileResult& compileResult() const { return current.compileResult; }
    const RuntimeProcessTrace& runtimeTrace() const { return current.runtimeTrace; }
    const GraphPreviewResult& previewResult() const { return current.previewResult; }
    uint64_t revision() const { return presentationRevision; }
    size_t compilationCount() const { return compilations; }
    size_t previewRenderCount() const { return previewRenders; }

    GraphAudioResult captureAudio(const NodeGraph& graph, size_t frameCount) const;

private:
    bool requiresCompilation(const GraphChangeSet& change) const;
    bool requiresPreview(const GraphChangeSet& change) const;

    GraphPresentationSnapshot current;
    GraphCompiler compiler;
    mutable GraphAudioExecutor previewAudioExecutor;
    uint64_t requestedGraphRevision {};
    uint64_t presentationRevision { 1 };
    size_t compilations {};
    size_t previewRenders {};
};

}
