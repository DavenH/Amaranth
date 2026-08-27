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

struct VoiceContextEdit;
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
    Rectangle<int> performanceKeyboardDockBounds() const;
    Rectangle<float> expandedEditorBoundsForOverlay() const;
    void setOverlayOcclusionChangedCallback(std::function<void()> callback);

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
    Point<float> lastMousePosition;
    String resolvedHoverText;
    bool pointerInsideCanvas {};
    bool draggingTrimeshMorph {};
    bool trimeshMorphUndoPushed {};
    bool draggingTrimeshVertexParameter {};
    bool trimeshVertexParameterUndoPushed {};
    bool canvasOpenGlAttached {};
    bool compiledStateRefreshPending {};
    std::optional<VoiceContextEdit::Control> draggingVoiceContextSlider;
    String draggingSpectralPanNodeId;
    float spectralPanDragStartValue {};
    SignalProbeRailState probeRailState;
    GuideCurveShelfState guideShelfState;
    float dockSplitRatio { 0.5f };
    SignalProbeDetailState probeDetailState;
    std::unique_ptr<WorkspaceDockInteractionController> dockInteraction;
    UnisonPreviewContext globalUnisonPreviewContext;
    String draggingProbeId;
    String expandedGuideId;
    std::optional<uint64_t> guideTransactionBaseRevision;
    uint32 compiledStateRefreshDueMs {};
    std::function<void()> overlayOcclusionChanged;

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
    uint32_t availableRenderInvalidations() const override;
    void flushRenderInvalidations(uint32_t categories) override;

    Point<float> viewportCentreWorld() const;
    void refreshCompiledState();
    void refreshCompiledStateAsync();
    void setPreviewVoiceLength(double seconds);
    void openProbeDetail(const String& probeId);
    void refreshProbeDetail();
    bool applyAuthoringResult(const NodeCanvasAuthoringResult& result);
    NodeCanvasAutomationPresentation automationPresentationState() const;
    void scheduleCompiledStateRefresh();
    void flushScheduledCompiledStateRefresh();
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
    bool cycleMeshOutputSide(const String& nodeId);
    bool cycleVoiceDomain(const String& nodeId);
    Rectangle<float> canvasContentBounds() const;
    WorkspaceDockLayout workspaceDockLayout() const;
    float tapPositionForEdge(int edgeIndex, Point<float> screenPosition) const;
    void showEdgeMenu(int edgeIndex, Point<float> screenPosition);
    void openGuideEditor(const String& guideId);
    void closeGuideEditor();

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
    TrimeshWidget* trimeshWidget(const Node& node) override;
    TrimeshWidget* findTrimeshWidget(const String& nodeId) override;
    TrimeshRenderProfile trimeshRenderProfile(const Node& node) const override;
    std::array<String, 6> trimeshGuideLabels(const Node& node) override;
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
    void beginCurveTransaction() override;
    void commitCurveTransaction() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeCanvas)
};

}
