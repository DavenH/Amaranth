#pragma once

#include <JuceHeader.h>

#include <array>
#include <functional>
#include <memory>

#include <App/Settings.h>

#include "Graph/GraphEditor.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphDocument.h"
#include "Graph/NodeGraph.h"
#include "Nodes/Curve/Editor/CurveEditorWidget.h"
#include "Nodes/Guide/Editor/GuideCurveEditorComponent.h"
#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentMenu.h"
#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentTarget.h"
#include "Nodes/Trimesh/Editor/TrimeshWidget.h"
#include "Runtime/GraphPresentationModel.h"
#include "UI/CanvasPerformanceMetrics.h"
#include "UI/NodeCanvasAutomationController.h"
#include "UI/NodeCanvasAuthoring.h"
#include "UI/NodeCanvasEditorCoordinator.h"
#include "UI/NodeCanvasPresentation.h"
#include "UI/NodeCanvasQueryModel.h"
#include "UI/NodeCableRenderer.h"
#include "UI/NodeCanvasGlRenderer.h"
#include "UI/NodeCanvasHitRouter.h"
#include "UI/NodeCanvasInteraction.h"
#include "UI/NodeCanvasScene.h"
#include "UI/NodeCanvasViewport.h"
#include "UI/NodeEditorHost.h"
#include "UI/NodePalette.h"
#include "UI/NodePreviewRenderer.h"
#include "UI/NodePreviewResources.h"
#include "UI/RenderInvalidationAccumulator.h"
#include "UI/WorkspaceDockInteractionController.h"

namespace CycleV2 {

enum class TransformMode;

class NodeCanvas :
        public Component
    ,   private OpenGLRenderer
    ,   private Timer
    ,   private NodeEditorPresentation
    ,   private NodeEditorResources
    ,   private CurveExpandedEditorDelegate
    ,   private RenderInvalidationTarget {
public:
    NodeCanvas();
    ~NodeCanvas() override;

    bool saveGraphToFile(const File& file);
    bool loadGraphFromFile(const File& file);
    bool isGraphDirty() const { return document.isDirty(); }
    const File& graphFile() const { return document.file(); }
    void setGraphDocumentStateChangedCallback(std::function<void()> callback);
    var exportAutomationState() const;
    String exportGraphJson() const;
    bool openNodeEditorForAutomation(const String& nodeId);
    bool addNodeForAutomation(const String& kind, Point<float> position, String& nodeId);
    bool moveNodeForAutomation(const String& nodeId, Point<float> position);
    bool connectPortsForAutomation(
            const String& sourceNodeId,
            const String& sourcePortId,
            const String& destNodeId,
            const String& destPortId);
    bool deleteNodeForAutomation(const String& nodeId);
    bool deleteEdgeForAutomation(int edgeIndex);
    bool deleteGuideCurveForAutomation(const String& guideId);
    bool loadGuideHeatmapForAutomation(const String& guideId, const File& file);
    bool clearGuideHeatmapForAutomation(const String& guideId);
    bool undoForAutomation();
    bool setGuideParameterForAutomation(
            const String& guideId,
            const String& parameterId,
            const String& value);
    bool setNodeParameterForAutomation(
            const String& nodeId,
            const String& parameterId,
            const String& label,
            const String& value);
    bool setMorphSliderForAutomation(const String& nodeId, const String& axis, float value);
    bool setPrimaryAxisForAutomation(const String& nodeId, const String& axis);
    bool toggleLinkForAutomation(const String& nodeId, const String& axis);
    bool selectVertexForAutomation(const String& nodeId, int vertexIndex);
    bool setVertexParameterForAutomation(const String& nodeId, const String& parameterId, float value);
    bool getNodeParameterForAutomation(const String& nodeId, const String& parameterId, String& value) const;
    var inspectNodeControlsForAutomation(const String& nodeId) const;
    var inspectPointerTargetsForAutomation() const;
    var inspectOpenGLDiagnosticsForAutomation() const;
    var inspectPerformanceMetricsForAutomation() const;
    void resetPerformanceMetricsForAutomation();
    void requestOpenGLFrameForAutomation();
    var captureAudioForAutomation(size_t frameCount) const;
    bool copyAudioPlan(GraphExecutionPlan& plan, uint64_t& revision) const;
    float graphOutputGain() const;
    int previewMidiNote() const { return presentation.previewMidiNote(); }
    bool setPreviewMidiNote(int midiNote);
    bool setPreviewModWheelValue(int value);
    void beginPreviewModWheelGesture();
    bool updatePreviewModWheelGesture(int value);
    void endPreviewModWheelGesture();
    Rectangle<int> performanceKeyboardDockBounds() const;
    Rectangle<float> expandedEditorBoundsForOverlay() const;
    void setOverlayOcclusionChangedCallback(std::function<void()> callback);
    void setPreviewPlaybackToggleCallback(std::function<void()> callback) {
        previewPlaybackToggle = std::move(callback);
    }
    void setRealtimeOutputMeterLevels(std::optional<OutputMeterLevels> measured);
    std::optional<OutputMeterLevels> realtimeOutputMeterLevels() const {
        return liveOutputMeterLevels;
    }

    void paint(Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    void focusLost(FocusChangeType cause) override;
    void mouseDown(const MouseEvent& event) override;
    void mouseMove(const MouseEvent& event) override;
    void mouseExit(const MouseEvent& event) override;
    void mouseDrag(const MouseEvent& event) override;
    void mouseUp(const MouseEvent& event) override;
    void mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) override;
    void mouseMagnify(const MouseEvent& event, float scaleFactor) override;
    bool keyPressed(const KeyPress& key) override;

private:
    enum class HoverRepaint {
        None,
        Status,
        Canvas
    };

    OpenGLContext openGLContext;
    NodeCanvasRenderer renderer;
    mutable NodeCanvasViewport viewport;
    bool documentViewportFitted {};
    mutable NodeCanvasScene sceneBuilder;
    Settings settings;
    GraphDocument document;
    GraphCommandDispatcher commands;
    const NodeGraph& graph;
    GraphPresentationModel presentation;
    const GraphCompileResult& compileResult;
    const RuntimeProcessTrace& runtimeTrace;
    const GraphPreviewResult& previewResult;
    NodeCanvasQueryModel queries;
    CanvasPerformanceMetrics performanceMetrics;
    NodeEditorCommandService editorCommands;
    NodeCanvasAuthoring authoring;
    NodeCanvasInteraction interaction;
    String& selectedNodeId;
    std::vector<String>& selectedNodeIds;
    String& expandedNodeId;
    String& editStatusMessage;
    int& selectedEdgeIndex;
    int& spliceTargetEdgeIndex;
    NodeCanvasEditorCoordinator editorCoordinator;
    NodeCanvasPresentation canvasPresentation;
    NodeCanvasAutomationController automation;
    RenderInvalidationAccumulator renderInvalidation;
    NodePalette palette;
    NodeCanvasHitRouter hitRouter;
    std::unique_ptr<CurveEditorWidget> guideEditorWidget;
    std::unique_ptr<GuideCurveEditorComponent> guideEditor;

    int activeTrimeshVertexIndex { -1 };
    int hoveredEdgeIndex { -1 };
    Point<float> lastMousePosition;
    String resolvedHoverText;
    bool pointerInsideCanvas {};
    bool draggingTrimeshMorph {};
    bool trimeshMorphUndoPushed {};
    bool draggingTrimeshVertexParameter {};
    bool trimeshVertexParameterUndoPushed {};
    bool canvasOpenGlAttached {};
    bool compiledStateRefreshPending {};
    PresentationRefreshScope compiledStateRefreshScope {
            PresentationRefreshScope::Downstream };
    String draggingSpectralPanNodeId;
    String draggingOutputGainNodeId;
    float spectralPanDragStartValue {};
    float outputGainDragStartValue { 0.5f };
    SignalProbeRailState probeRailState;
    GuideCurveShelfState guideShelfState;
    SignalProbeDetailState probeDetailState;
    OutputMeterBallistics outputMeterBallistics;
    std::optional<OutputMeterLevels> liveOutputMeterLevels;
    std::unique_ptr<WorkspaceDockInteractionController> dockInteraction;
    UnisonPreviewContext globalUnisonPreviewContext;
    String draggingProbeId;
    String expandedGuideId;
    std::optional<uint64_t> guideTransactionBaseRevision;
    std::function<void()> graphDocumentStateChangedCallback;
    uint32 compiledStateRefreshDueMs {};
    std::function<void()> overlayOcclusionChanged;
    std::function<void()> previewPlaybackToggle;
    bool previewModWheelGestureActive {};
    bool previewModWheelGestureChanged {};
    int previewModWheelGestureValue {};
    ProbeRefreshMode previewModWheelGestureRefreshMode {
            ProbeRefreshMode::OnGestureCommit };
    std::shared_ptr<const NodeGraph> previewModWheelGestureGraph;

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void timerCallback() override;
    HoverRepaint updateHoverAt(juce::Point<float> position);
    static HoverRepaint hoverRepaintFor(bool canvasChanged, bool statusChanged);

    void setCanvasOpenGlAttached(bool shouldAttach);
    NodeCanvasPresentationFrame presentationFrame() const;
    void requestCanvasRepaint();
    void requestCanvasStatusRepaint();
    void requestHoverRepaint(HoverRepaint repaint);
    void notifyOverlayOcclusionChanged();
    std::optional<NodeAudioResourceSummary> audioResourceSummary(
            const String& nodeId) const override;
    uint32_t availableRenderInvalidations() const override;
    void flushRenderInvalidations(uint32_t categories) override;

    Point<float> viewportCentreWorld() const;
    void refreshCompiledState();
    void refreshCompiledStateAsync(
            PresentationRefreshScope scope = PresentationRefreshScope::Downstream);
    void openProbeDetail(const String& probeId);
    void refreshProbeDetail();
    void finishPreviewModWheelRefresh();
    bool persistPreviewMorph(int midiNote, int modWheelValue);
    void synchronizeOpenedEditorMorph();
    bool applyAuthoringResult(const NodeCanvasAuthoringResult& result);
    NodeCanvasAutomationPresentation automationPresentationState() const;
    void scheduleCompiledStateRefresh(
            PresentationRefreshScope scope = PresentationRefreshScope::Downstream);
    void flushScheduledCompiledStateRefresh();
    void resetDocumentPresentation();
    void fitDocumentInViewport();
    File snapshotFile() const;
    bool saveSnapshot();
    bool loadSnapshot();
    bool undo();
    bool redo();
    bool spliceSelectedNodeIntoEdgeAt(Point<float> screenPosition);
    bool clearSelection();
    bool handleDockNavigationKey(const KeyPress& key);
    void clearDockEphemeralState();
    bool cycleOperationPortLayout(const String& nodeId);
    bool cycleSinglePortLayout(const String& nodeId);
    bool cycleOutputSide(const String& nodeId);
    Rectangle<float> canvasContentBounds() const;
    WorkspaceDockLayout workspaceDockLayout() const;
    void showEdgeMenu(int edgeIndex, Point<float> screenPosition);
    void openGuideEditor(const String& guideId);
    void closeGuideEditor();
    void rebindGuideEditor();
    bool setGuideHeatmap(
            const String& guideId,
            GuideHeatmapAssetPtr asset,
            uint64_t expectedRevision);
    bool clearGuideHeatmap(const String& guideId, uint64_t expectedRevision);

    void closeNodeEditor() override;
    void repaintNodeEditor(bool openGl) override;
    void selectEditedNode(const String& nodeId) override;
    void setNodeEditorStatus(const String& message) override;
    void scheduleNodeEditorRefresh() override;
    void flushNodeEditorRefresh() override;
    void refreshNodeEditorPresentation() override;
    Point<float> nodeEditorCreationPosition() const override;
    void rebindNodeEditor() override;
    void rebindNodeEditorTransient() override;
    ProbeRefreshMode probeRefreshMode() const override { return probeRailState.refreshMode; }
    void recordNodeEditorMovement(
            const String& nodeId,
            const String& field,
            uint64_t effectiveFingerprint) override;
    void commitNodeEditorLocalState(
            const String& nodeId,
            const String& field,
            uint64_t effectiveFingerprint,
            uint64_t documentRevision) override;

    CurveEditorWidget* curveEditorWidget(const Node& node) override;
    void syncCurveGuideContext(CurveEditorWidget& widget, const Node& node) override;
    TrimeshWidget* trimeshWidget(const Node& node) override;
    TrimeshWidget* findTrimeshWidget(const String& nodeId) override;
    TrimeshRenderProfile trimeshRenderProfile(const Node& node) const override;
    std::array<String, 6> trimeshGuideLabels(const Node& node) override;
    std::array<String, 6> envelopeGuideLabels(
            const Node& node,
            int cubeIndex) override;
    void paintNodePreview(
            Graphics& graphics,
            const Node& node,
            Rectangle<float> bounds) override;
    UnisonPreviewContext unisonPreviewContext() const override;

    void closeCurveEditor() override;
    void repaintCurveEditorOpenGL() override;
    bool publishCurveState(
            NodeModelStatePtr model,
            const std::vector<NodeParameter>& controls) override;
    bool setNodeParameterText(
            const String& parameterId,
            const String& label,
            const String& value) override;
    void beginCurveTransaction() override;
    void commitCurveTransaction() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeCanvas)
};

}
