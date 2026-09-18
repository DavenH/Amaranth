#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/GraphPreviewExecutor.h"
#include "Runtime/GraphPresentationPerformanceMetrics.h"
#include "Runtime/GraphRuntime.h"
#include "Runtime/MessageThreadWorker.h"
#include "Runtime/NodeUpdateGraph.h"
#include "Runtime/PresentationGestureSession.h"
#include "Runtime/PresentationUpdateRequestBuilder.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphEditor.h"

namespace CycleV2 {

struct GraphPresentationSnapshot {
    uint64_t graphRevision {};
    int previewMidiNote { 48 };
    int previewModWheelValue {};
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
    bool refreshPreviewMidiNote(
            const NodeGraph& graph,
            uint64_t documentRevision,
            int midiNote);
    bool refreshPreviewModWheelValue(
            const NodeGraph& graph,
            uint64_t documentRevision,
            int value);
    void stagePreviewModWheelValue(int value);
    GraphChangeSet modWheelPreviewChange(GraphChangeSet change) const;
    void refreshAsync(
            NodeGraph graph,
            uint64_t documentRevision,
            GraphChangeSet change,
            PresentationRefreshScope scope,
            std::function<void()> completion = {});
    void refreshAsync(
            std::shared_ptr<const NodeGraph> graph,
            uint64_t documentRevision,
            GraphChangeSet change,
            PresentationRefreshScope scope,
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
    int previewMidiNote() const { return current.previewMidiNote; }
    int previewModWheelValue() const { return current.previewModWheelValue; }
    uint64_t revision() const { return presentationRevision; }
    uint64_t audioPlanRevision() const { return audioRevision; }
    size_t compilationCount() const { return compilations; }
    size_t previewRenderCount() const { return previewRenders; }
    size_t previewAudioProcessCount(const String& nodeId) const {
        return previewAudioExecutor.diagnosticProcessCount(nodeId);
    }
    const UpdateAuditTrace& updateTrace() const { return updateGraph.trace(); }
    PresentationGestureSession& editSession() { return gestureSession; }
    juce::var performanceMetrics() const { return performance.toVar(); }
    void resetPerformanceMetrics() { performance.reset(); }

    GraphAudioResult captureAudio(const NodeGraph& graph, size_t frameCount) const;
    static int auditionMidiNoteForProbe(
            const NodeGraph& graph,
            const String& probeId,
            int fallbackMidiNote = 48);
    std::optional<GraphPreviewResult::SignalProbePreview> captureProbePreview(
            const NodeGraph& graph,
            const String& probeId,
            size_t rasterRowCount,
            int midiNote) const;

private:
    struct AsyncState {
        std::atomic<bool> alive { true };
        std::atomic<uint64_t> generation {};
    };

    struct AsyncRefresh {
        std::shared_ptr<AsyncState> state;
        uint64_t generation {};
        std::shared_ptr<const NodeGraph> graph;
        GraphChangeSet change;
        PresentationRefreshScope scope { PresentationRefreshScope::Downstream };
        CausalUpdateRequest request;
        CausalUpdateResult updateResult;
        GraphPresentationSnapshot snapshot;
        std::function<void()> completion;
        uint64_t requestedAtMicroseconds {};
        uint64_t workerFinishedAtMicroseconds {};
        bool previewRendered {};
    };

    bool requiresCompilation(const GraphChangeSet& change) const;
    bool requiresPreview(const GraphChangeSet& change) const;
    bool refreshPreviewControls(
            const NodeGraph& graph,
            uint64_t documentRevision,
            std::vector<String> rootNodeIds);
    void refreshConfigurations(
            const NodeGraph& graph,
            GraphExecutionPlan& plan,
            const std::vector<String>& nodeIds);
    bool prepareAsyncRefresh(AsyncRefresh& refresh);
    bool executeAsyncProducts(
            AsyncRefresh& refresh,
            const std::vector<PlannedNodeProduct>& products);
    bool renderPreviewProducts(
            const NodeGraph& graph,
            GraphPresentationSnapshot& snapshot,
            const std::vector<PlannedNodeProduct>& products,
            bool renderFullGraph,
            PresentationRefreshScope scope,
            bool& previewRendered,
            GraphAudioExecutor::CancellationCheck cancellationCheck = {});
    std::function<void()> publishAsyncRefresh(std::shared_ptr<AsyncRefresh> refresh);
    bool isCurrent(const AsyncRefresh& refresh) const;
    CausalUpdateRequest updateRequest(
            const NodeGraph& graph,
            const GraphExecutionPlan& plan,
            uint64_t documentRevision,
            const GraphChangeSet& change,
            bool compile,
            bool preview,
            PresentationRefreshScope scope);

    bool hasExplicitPreviewMidiNote {};

    GraphPresentationSnapshot current;
    GraphCompiler compiler;
    NodeDspConfigurationFactory configurationFactory;
    NodeUpdateGraph updateGraph;
    PresentationGestureSession gestureSession;
    mutable GraphAudioExecutor previewAudioExecutor;
    uint64_t requestedGraphRevision {};
    uint64_t presentationRevision { 1 };
    uint64_t audioRevision { 1 };
    size_t compilations {};
    size_t previewRenders {};
    uint64_t publishedGeneration {};
    std::vector<String> modWheelPreviewRootNodeIds;
    MessageThreadWorker asyncWorker;
    std::shared_ptr<AsyncState> asyncState;
    GraphPresentationPerformanceMetrics performance;
};

}
