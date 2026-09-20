#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "Runtime/GraphAudioExecutor.h"
#include "Runtime/GraphPresentationPerformanceMetrics.h"
#include "Runtime/GraphPresentationSnapshot.h"
#include "Runtime/NodeUpdateGraph.h"
#include "Runtime/PresentationPreviewRenderer.h"
#include "Runtime/PresentationRefreshScheduler.h"
#include "Graph/GraphCompiler.h"
#include "Graph/GraphEditTypes.h"
#include "Graph/PreviewMorphTarget.h"

namespace CycleV2 {

class GraphPresentationModel {
public:
    GraphPresentationModel() = default;
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
    bool refreshLocalNodePreview(
            const Node& node,
            std::function<void()> completion);
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
    bool hasModWheelPreviewRoots() const { return !modWheelPreviewRootNodeIds.empty(); }
    const std::vector<PreviewMorphTarget>& keyScaleMorphTargets() const {
        return keyScaleTargets;
    }
    const std::vector<PreviewMorphTarget>& modWheelMorphTargets() const {
        return modWheelTargets;
    }
    const std::vector<PreviewMorphTarget>& allPerformanceMorphTargets() const {
        return allMorphTargets;
    }
    uint64_t revision() const { return presentationRevision; }
    uint64_t audioPlanRevision() const { return audioRevision; }
    size_t compilationCount() const { return compilations; }
    size_t previewRenderCount() const { return previewRenders; }
    size_t previewAudioProcessCount(const String& nodeId) const {
        return previewRenderer.diagnosticProcessCount(nodeId);
    }
    const UpdateAuditTrace& updateTrace() const { return scheduler.trace(); }
    PresentationGestureSession& editSession() { return scheduler.editSession(); }
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
    using AsyncRefresh = PresentationRefreshScheduler::AsyncRefresh;

    bool requiresCompilation(const GraphChangeSet& change) const;
    bool requiresPreview(const GraphChangeSet& change) const;
    bool canAcceptSnapshot(const GraphPresentationSnapshot& snapshot) const;
    bool acceptSnapshot(
            GraphPresentationSnapshot snapshot,
            const NodeGraph& graph,
            bool reuseStructure);
    bool refreshPreviewControls(
            const NodeGraph& graph,
            uint64_t documentRevision,
            std::vector<String> rootNodeIds);
    void refreshConfigurations(
            const NodeGraph& graph,
            GraphExecutionPlan& plan,
            const std::vector<String>& nodeIds);
    void refreshPreviewMorphBindings();
    bool executeAsyncProducts(
            AsyncRefresh& refresh,
            const std::vector<PlannedNodeProduct>& products);

    bool hasExplicitPreviewMidiNote {};

    GraphPresentationSnapshot current;
    GraphCompiler compiler;
    NodeDspConfigurationFactory configurationFactory;
    PresentationRefreshScheduler scheduler;
    PresentationPreviewRenderer previewRenderer;
    uint64_t requestedGraphRevision {};
    uint64_t presentationRevision { 1 };
    uint64_t audioRevision { 1 };
    size_t compilations {};
    size_t previewRenders {};
    std::vector<String> modWheelPreviewRootNodeIds;
    std::vector<PreviewMorphTarget> allMorphTargets;
    std::vector<PreviewMorphTarget> keyScaleTargets;
    std::vector<PreviewMorphTarget> modWheelTargets;
    GraphPresentationPerformanceMetrics performance;
};

}
