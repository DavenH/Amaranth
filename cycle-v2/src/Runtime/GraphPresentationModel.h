#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

#include "GraphAudioExecutor.h"
#include "GraphPreviewExecutor.h"
#include "GraphRuntime.h"
#include "MessageThreadWorker.h"
#include "NodeUpdateGraph.h"
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
    GraphPresentationModel();
    ~GraphPresentationModel();

    bool refresh(
            const NodeGraph& graph,
            uint64_t documentRevision,
            const GraphChangeSet& change = {});
    bool acceptSnapshot(GraphPresentationSnapshot snapshot);
    void refreshAsync(
            NodeGraph graph,
            uint64_t documentRevision,
            GraphChangeSet change,
            std::function<void()> completion = {});
    void recordEditorMovement(
            const String& nodeId,
            const String& field,
            uint64_t effectiveFingerprint,
            bool deferredUntilCommit);
    void commitLocalEditorState(
            const String& nodeId,
            const String& field,
            uint64_t effectiveFingerprint,
            uint64_t documentRevision);

    const GraphPresentationSnapshot& snapshot() const { return current; }
    const GraphCompileResult& compileResult() const { return current.compileResult; }
    const RuntimeProcessTrace& runtimeTrace() const { return current.runtimeTrace; }
    const GraphPreviewResult& previewResult() const { return current.previewResult; }
    uint64_t revision() const { return presentationRevision; }
    size_t compilationCount() const { return compilations; }
    size_t previewRenderCount() const { return previewRenders; }
    const UpdateAuditTrace& updateTrace() const { return updateGraph.trace(); }

    GraphAudioResult captureAudio(const NodeGraph& graph, size_t frameCount) const;

private:
    struct AsyncState {
        std::atomic<bool> alive { true };
        std::atomic<uint64_t> generation {};
    };

    struct AsyncRefresh {
        std::shared_ptr<AsyncState> state;
        uint64_t generation {};
        NodeGraph graph;
        GraphChangeSet change;
        CausalUpdateRequest request;
        CausalUpdateResult updateResult;
        uint64_t requestFingerprint {};
        GraphPresentationSnapshot snapshot;
        std::function<void()> completion;
        bool previewRendered {};
    };

    bool requiresCompilation(const GraphChangeSet& change) const;
    bool requiresPreview(const GraphChangeSet& change) const;
    void refreshConfigurations(
            const NodeGraph& graph,
            GraphExecutionPlan& plan,
            const std::vector<String>& nodeIds);
    bool prepareAsyncRefresh(AsyncRefresh& refresh);
    bool executeAsyncProducts(
            AsyncRefresh& refresh,
            const std::vector<PlannedNodeProduct>& products);
    std::function<void()> publishAsyncRefresh(std::shared_ptr<AsyncRefresh> refresh);
    bool isCurrent(const AsyncRefresh& refresh) const;
    CausalUpdateRequest updateRequest(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            uint64_t documentRevision,
            const GraphChangeSet& change,
            bool compile,
            bool preview);

    GraphPresentationSnapshot current;
    GraphCompiler compiler;
    NodeDspConfigurationFactory configurationFactory;
    NodeUpdateGraph updateGraph;
    SemanticEditGate editGate;
    mutable GraphAudioExecutor previewAudioExecutor;
    uint64_t requestedGraphRevision {};
    uint64_t presentationRevision { 1 };
    size_t compilations {};
    size_t previewRenders {};
    uint64_t publishedEditFingerprint {};
    uint64_t publishedGeneration {};
    std::optional<EditIdentity> latestMovementIdentity;
    String latestMovementStream;
    MessageThreadWorker asyncWorker;
    std::shared_ptr<AsyncState> asyncState;
};

}
