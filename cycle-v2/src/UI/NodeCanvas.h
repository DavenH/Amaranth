#pragma once

#include <JuceHeader.h>

#include <array>
#include <memory>

#include "../Graph/GraphEditor.h"
#include "../Graph/GraphCommandDispatcher.h"
#include "../Graph/GraphDocument.h"
#include "../Graph/GraphRenderSemanticResolver.h"
#include "../Graph/NodeGraph.h"
#include "../Nodes/Effect2D/Effect2DWidget.h"
#include "../Nodes/Trimesh/TrimeshGuideAttachmentMenu.h"
#include "../Nodes/Trimesh/TrimeshGuideAttachmentTarget.h"
#include "../Nodes/Trimesh/TrimeshWidget.h"
#include "../Runtime/GraphPresentationModel.h"
#include "NodeAutomationFacade.h"
#include "NodeCanvasGlRenderer.h"
#include "NodeCanvasScene.h"
#include "NodeCanvasViewport.h"
#include "NodeEditorHost.h"
#include "NodePalette.h"
#include "RenderInvalidationAccumulator.h"

namespace CycleV2 {

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
    String exportGraphXml() const;
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
    void mouseDown(const MouseEvent& event) override;
    void mouseMove(const MouseEvent& event) override;
    void mouseDrag(const MouseEvent& event) override;
    void mouseUp(const MouseEvent& event) override;
    void mouseWheelMove(const MouseEvent& event, const MouseWheelDetails& wheel) override;
    void mouseMagnify(const MouseEvent& event, float scaleFactor) override;
    bool keyPressed(const KeyPress& key) override;

private:
    struct PortLocation {
        Rectangle<float> bounds;
        Point<float> centre;
    };

    struct CableEndpoint {
        Point<float> centre;
        PortSide side { PortSide::Left };
        bool portLike { true };
    };

    struct CachedPreviewSprite {
        Image image;
        String signature;
        PortDomain domain { PortDomain::ControlSignal };
        int width {};
        int height {};
    };

    OpenGLContext openGLContext;
    NodeCanvasRenderer renderer;
    mutable NodeCanvasViewport viewport;
    mutable NodeCanvasScene sceneBuilder;
    NodeCanvasHitTester hitTester;
    GraphDocument document;
    GraphCommandDispatcher commands;
    NodeAutomationFacade automation;
    const NodeGraph& graph;
    GraphPresentationModel presentation;
    const GraphCompileResult& compileResult;
    const RuntimeProcessTrace& runtimeTrace;
    const GraphPreviewResult& previewResult;
    std::vector<std::pair<String, CachedPreviewSprite>> previewSpriteCache;
    std::vector<std::pair<String, std::unique_ptr<TrimeshWidget>>> trimeshWidgets;
    std::vector<std::pair<String, std::unique_ptr<Effect2DWidget>>> effect2DWidgets;
    NodeEditorCommandService editorCommands;
    NodeEditorHost editorHost;
    RenderInvalidationAccumulator renderInvalidation;
    NodePalette palette;

    Point<float> dragStartPan;
    Rectangle<float> dragStartNodeBounds;
    String selectedNodeId;
    String expandedNodeId;
    String editStatusMessage;
    int activeTrimeshVertexIndex { -1 };
    int selectedEdgeIndex { -1 };
    int spliceTargetEdgeIndex { -1 };
    PortAddress connectingPort;
    Point<float> connectingPoint;
    Point<float> lastMousePosition;
    float activeSnapWorldX {};
    float activeSnapWorldY {};
    bool draggingNode {};
    bool connectingCable {};
    bool nodeDragUndoPushed {};
    bool nodeDragMoved {};
    bool activeSnapHasX {};
    bool activeSnapHasY {};
    bool draggingTrimeshMorph {};
    bool trimeshMorphUndoPushed {};
    bool draggingTrimeshVertexParameter {};
    bool trimeshVertexParameterUndoPushed {};
    bool expandedEditorDragCaptured {};
    bool canvasOpenGlAttached {};
    bool compiledStateRefreshPending {};
    uint32 compiledStateRefreshDueMs {};

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void timerCallback() override;

    void drawGrid(Graphics& g);
    void drawGlEdges();
    void drawGlNodeShells();
    void drawGlEffect2DPreviews();
    void drawGlExpandedEditor();
    void drawEdges(Graphics& g);
    void drawConnectionPreview(Graphics& g);
    void drawNodes(Graphics& g);
    void drawSnapGuides(Graphics& g);
    void drawNode(Graphics& g, const Node& node);
    void drawPreview(Graphics& g, const Node& node, Rectangle<float> area);
    void drawPreviewUncached(Graphics& g, const Node& node, Rectangle<float> area, PortDomain previewDomain);
    bool drawCachedPreviewHeatmapSurface(
            Graphics& g,
            const String& cacheKey,
            Rectangle<float> area,
            const NodePreviewResult& preview);
    CachedPreviewSprite& cachedPreviewSpriteFor(const String& nodeId);
    TrimeshWidget& trimeshWidgetFor(const String& nodeId);
    Effect2DWidget& effect2DWidgetFor(const Node& node);
    void drawSpectrumBars(Graphics& g, Rectangle<float> area, Colour colour, int seed);
    void drawPhaseTrace(Graphics& g, Rectangle<float> area, Colour colour, int seed);
    void drawEnvelopeCurve(Graphics& g, Rectangle<float> area);
    void drawExpandedEditor(Graphics& g, const Node& node);
    void setCanvasOpenGlAttached(bool shouldAttach);
    void updateExpandedEditorHost(const Node* node);
    void hideExpandedEditorHosts();
    void hideExpandedEditorHostsExcept(const String& nodeId);
    void detachExpandedEditorHosts();
    void drawMiniMap(Graphics& g);
    void drawGraphStatus(Graphics& g);
    void drawEdgeLegend(Graphics& g);
    void drawNodePalette(Graphics& g);
    void drawHoverConsole(Graphics& g);
    void requestCanvasRepaint();
    uint32_t availableRenderInvalidations() const override;
    void flushRenderInvalidations(uint32_t categories) override;

    Point<float> toScreen(Point<float> p) const;
    Point<float> toWorld(Point<float> p) const;
    Rectangle<float> toScreen(Rectangle<float> r) const;
    Rectangle<float> snappedNodeBounds(const Node& node, Rectangle<float> proposed);
    PortLocation getPortLocation(const Node& node, const Port& port) const;
    PortLocation getPortLocation(const PortAddress& address) const;
    bool resolveCableEndpoints(
            const Edge& edge,
            CableEndpoint& sourceEndpoint,
            CableEndpoint& destEndpoint) const;
    bool isDynamicTrimeshGuideTarget(const Node& node, const String& portId) const;
    CableEndpoint dynamicTrimeshGuideEndpoint(const Node& node, const String& portId) const;
    bool findPortAt(Point<float> screenPosition, PortAddress& result) const;
    bool findConnectablePortAt(Point<float> screenPosition, const PortAddress& source, PortAddress& result) const;
    bool findOperationLayoutButtonAt(Point<float> screenPosition, String& nodeId) const;
    bool findMeshOutputSideButtonAt(Point<float> screenPosition, String& nodeId) const;
    bool findVoiceDomainButtonAt(Point<float> screenPosition, String& nodeId) const;
    int findEdgeAt(Point<float> screenPosition) const;
    int findSpliceTargetEdgeAt(Point<float> screenPosition, const String& nodeId) const;
    const Node* findNode(const String& id) const;
    const Node* findNodeAt(Point<float> worldPosition) const;
    const Port* findPort(const Node& node, const String& portId, bool input) const;
    const RuntimeNodeTrace* findRuntimeTrace(const String& nodeId) const;
    const NodePreviewResult* findPreviewResult(const String& nodeId) const;
    PortDomain displayDomainForEdge(const Edge& edge) const;
    PortDomain displayDomainForNodeOutput(const Node& node, const String& portId) const;
    TrimeshRenderProfile renderProfileForNodeOutput(const Node& node, const String& portId) const;
    bool edgeHasValidationIssue(const Edge& edge) const;
    GraphValidationIssue validationIssueForEdge(const Edge& edge) const;
    int executionIndexForNode(const String& nodeId) const;
    int attachmentCount() const;
    String hoverTextFor(Point<float> screenPosition) const;
    String textForPort(const PortAddress& address) const;
    String textForNode(const Node& node) const;
    Point<float> viewportCentreWorld() const;
    Point<float> paletteCreationWorldPosition(NodeKind kind, Point<float> paletteClickPosition) const;
    void refreshCompiledState();
    void scheduleCompiledStateRefresh();
    void flushScheduledCompiledStateRefresh();
    File snapshotFile() const;
    bool saveSnapshot();
    bool loadSnapshot();
    bool undo();
    bool redo();
    bool restoreGraphXml(const String& xml, const String& statusMessage);
    bool spliceSelectedNodeIntoEdgeAt(Point<float> screenPosition);
    void spaceNodesAfterSplice(
            const String& upstreamNodeId,
            const String& insertedNodeId,
            const String& downstreamNodeId);
    bool clearSelection();
    bool cycleOperationPortLayout(const String& nodeId);
    bool cycleMeshOutputSide(const String& nodeId);
    bool cycleVoiceDomain(const String& nodeId);
    bool handleVoiceContextEditorClick(Point<float> screenPosition);
    bool handleTransformEditorClick(Point<float> screenPosition);
    bool setVoiceContextParameter(const String& parameterId, const String& label, const String& value, const String& statusMessage);
    bool setTransformParameter(const String& parameterId, const String& label, const String& value, const String& statusMessage);
    std::array<String, 6> trimeshGuideAttachmentLabelsForNode(const Node& meshNode);

    void closeNodeEditor() override;
    void repaintNodeEditor(bool openGl) override;
    void selectEditedNode(const String& nodeId) override;
    void setNodeEditorStatus(const String& message) override;
    void scheduleNodeEditorRefresh() override;
    void flushNodeEditorRefresh() override;
    void refreshNodeEditorPresentation() override;
    Point<float> nodeEditorCreationPosition() const override;
    void rebindNodeEditor() override;

    Effect2DWidget* effect2DWidget(const Node& node) override;
    TrimeshWidget* trimeshWidget(const Node& node) override;
    TrimeshRenderProfile trimeshRenderProfile(const Node& node) const override;
    std::array<String, 6> trimeshGuideLabels(const Node& node) override;
    bool canConnectPorts(const PortAddress& first, const PortAddress& second) const;
    Path createCablePath(
            Point<float> source,
            Point<float> dest,
            PortSide sourceSide,
            PortSide destSide,
            bool attachment) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeCanvas)
};

}
