#pragma once

#include <JuceHeader.h>

#include <array>
#include <memory>

#include <App/Settings.h>

#include "../Graph/GraphEditor.h"
#include "../Graph/GraphCommandDispatcher.h"
#include "../Graph/GraphDocument.h"
#include "../Graph/NodeGraph.h"
#include "../Nodes/Effect2D/Effect2DWidget.h"
#include "../Nodes/Trimesh/TrimeshGuideAttachmentMenu.h"
#include "../Nodes/Trimesh/TrimeshGuideAttachmentTarget.h"
#include "../Nodes/Trimesh/TrimeshWidget.h"
#include "../Runtime/GraphPresentationModel.h"
#include "NodeCanvasAutomationController.h"
#include "NodeCanvasAuthoring.h"
#include "NodeCanvasEditorCoordinator.h"
#include "NodeCanvasPresentation.h"
#include "NodeCanvasQueryModel.h"
#include "NodeCableRenderer.h"
#include "NodeCanvasGlRenderer.h"
#include "NodeCanvasHitRouter.h"
#include "NodeCanvasInteraction.h"
#include "NodeCanvasScene.h"
#include "NodeCanvasViewport.h"
#include "NodeEditorHost.h"
#include "NodePalette.h"
#include "NodePreviewRenderer.h"
#include "NodePreviewResources.h"
#include "RenderInvalidationAccumulator.h"

namespace CycleV2 {

struct VoiceContextEdit;
enum class TransformMode;

class NodeCanvas :
        public Component
    ,   private OpenGLRenderer
    ,   private Timer
    ,   private NodeEditorPresentation
    ,   private NodeEditorResources
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
    var captureAudioForAutomation(size_t frameCount) const;

    void paint(Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    void focusLost(FocusChangeType cause) override;
    void mouseDown(const MouseEvent& event) override;
    void mouseMove(const MouseEvent& event) override;
    void mouseDrag(const MouseEvent& event) override;
    void mouseUp(const MouseEvent& event) override;
    void mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) override;
    void mouseMagnify(const MouseEvent& event, float scaleFactor) override;
    bool keyPressed(const KeyPress& key) override;

private:
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

    int activeTrimeshVertexIndex { -1 };
    Point<float> lastMousePosition;
    bool draggingTrimeshMorph {};
    bool trimeshMorphUndoPushed {};
    bool draggingTrimeshVertexParameter {};
    bool trimeshVertexParameterUndoPushed {};
    bool canvasOpenGlAttached {};
    bool compiledStateRefreshPending {};
    SignalProbeRailState probeRailState;
    String draggingProbeId;
    bool resizingProbeRail {};
    float probeRailResizeStartHeight {};
    float probeRailResizeStartY {};
    uint32 compiledStateRefreshDueMs {};

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void timerCallback() override;

    void setCanvasOpenGlAttached(bool shouldAttach);
    NodeCanvasPresentationFrame presentationFrame() const;
    void requestCanvasRepaint();
    uint32_t availableRenderInvalidations() const override;
    void flushRenderInvalidations(uint32_t categories) override;

    Point<float> viewportCentreWorld() const;
    void refreshCompiledState();
    void refreshCompiledStateAsync();
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
    bool cycleOperationPortLayout(const String& nodeId);
    bool cycleMeshOutputSide(const String& nodeId);
    bool cycleVoiceDomain(const String& nodeId);
    Rectangle<float> canvasContentBounds() const;
    float tapPositionForEdge(int edgeIndex, Point<float> screenPosition) const;
    void showEdgeMenu(int edgeIndex, Point<float> screenPosition);

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

    Effect2DWidget* effect2DWidget(const Node& node) override;
    TrimeshWidget* trimeshWidget(const Node& node) override;
    TrimeshRenderProfile trimeshRenderProfile(const Node& node) const override;
    std::array<String, 6> trimeshGuideLabels(const Node& node) override;
    void paintNodePreview(
            Graphics& graphics,
            const Node& node,
            Rectangle<float> bounds) override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeCanvas)
};

}
