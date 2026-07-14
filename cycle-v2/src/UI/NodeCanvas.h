#pragma once

#include <JuceHeader.h>

#include <array>
#include <memory>

#include "../Graph/GraphEditor.h"
#include "../Graph/GraphRenderSemanticResolver.h"
#include "../Graph/NodeGraph.h"
#include "../Graph/GraphSerializer.h"
#include "../Nodes/Effect2D/Effect2DExpandedEditorComponent.h"
#include "../Nodes/Effect2D/Effect2DWidget.h"
#include "../Nodes/Trimesh/TrimeshExpandedEditorComponent.h"
#include "../Nodes/Trimesh/TrimeshGuideAttachmentMenu.h"
#include "../Nodes/Trimesh/TrimeshGuideAttachmentTarget.h"
#include "../Nodes/Trimesh/TrimeshMeshEditState.h"
#include "../Nodes/Trimesh/TrimeshWidget.h"
#include "../Runtime/GraphPreviewExecutor.h"
#include "../Runtime/GraphAudioExecutor.h"
#include "../Runtime/GraphRuntime.h"
#include "NodeCanvasGlRenderer.h"

namespace CycleV2 {

class NodeCanvas :
        public Component
    ,   private OpenGLRenderer
    ,   private Timer {
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
    NodeCanvasGlRenderer glRenderer;
    NodeGraph graph;
    GraphCompiler graphCompiler;
    GraphCompileResult compileResult;
    RuntimeProcessTrace runtimeTrace;
    GraphPreviewResult previewResult;
    mutable GraphAudioExecutor audioExecutor;
    mutable GraphAudioExecutor previewAudioExecutor;
    std::vector<String> undoStack;
    std::vector<String> redoStack;
    std::vector<std::pair<String, CachedPreviewSprite>> previewSpriteCache;
    std::vector<std::pair<String, std::unique_ptr<TrimeshWidget>>> trimeshWidgets;
    std::vector<std::pair<String, std::unique_ptr<Effect2DWidget>>> effect2DWidgets;
    std::unique_ptr<TrimeshExpandedEditorComponent> trimeshExpandedEditor;
    std::unique_ptr<Effect2DExpandedEditorComponent> effect2DExpandedEditor;
    String trimeshExpandedEditorNodeId;
    String effect2DExpandedEditorNodeId;

    float zoom { 0.58f };
    Point<float> pan { 34.f, 38.f };
    Point<float> dragStartPan;
    Rectangle<float> dragStartNodeBounds;
    String selectedNodeId;
    String expandedNodeId;
    String activeTrimeshMorphNodeId;
    String activeTrimeshMorphParameterId;
    String activeTrimeshVertexNodeId;
    String activeTrimeshVertexParameterId;
    String editStatusMessage;
    int activeTrimeshVertexIndex { -1 };
    int selectedEdgeIndex { -1 };
    int spliceTargetEdgeIndex { -1 };
    int activePaletteSectionIndex { -1 };
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
    bool drawGlEffect2DExpandedPanel();
    void drawGlTrimeshExpandedPanel();
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
    bool findPaletteKindAt(Point<float> screenPosition, NodeKind& kind) const;
    bool findOperationLayoutButtonAt(Point<float> screenPosition, String& nodeId) const;
    bool findMeshOutputSideButtonAt(Point<float> screenPosition, String& nodeId) const;
    bool findVoiceDomainButtonAt(Point<float> screenPosition, String& nodeId) const;
    int findEdgeAt(Point<float> screenPosition) const;
    int findSpliceTargetEdgeAt(Point<float> screenPosition, const String& nodeId) const;
    const Node* findNode(const String& id) const;
    Node* findMutableNode(const String& id);
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
    void pushUndoSnapshot();
    void pushUndoSnapshot(String xml);
    bool restoreGraphXml(const String& xml, const String& statusMessage);
    bool spliceSelectedNodeIntoEdgeAt(Point<float> screenPosition);
    void shoveNodesForwardAfterSplice(const String& insertedNodeId, const String& downstreamNodeId);
    bool clearSelection();
    bool cycleOperationPortLayout(const String& nodeId);
    bool cycleMeshOutputSide(const String& nodeId);
    bool cycleVoiceDomain(const String& nodeId);
    bool handleVoiceContextEditorClick(Point<float> screenPosition);
    bool handleTransformEditorClick(Point<float> screenPosition);
    bool setVoiceContextParameter(const String& parameterId, const String& label, const String& value, const String& statusMessage);
    bool setTransformParameter(const String& parameterId, const String& label, const String& value, const String& statusMessage);
    bool setTrimeshPrimaryAxisValue(const String& axisValue);
    bool toggleTrimeshLinkAxisValue(const String& axisValue);
    bool beginTrimeshMorphEdit(const String& parameterId, float value);
    bool updateTrimeshMorphEditValue(float value);
    void endTrimeshMorphEdit();
    bool beginTrimeshVertexParameterEdit(const String& parameterId, float value);
    bool updateTrimeshVertexParameterEditValue(float value);
    void endTrimeshVertexParameterEdit();
    void persistTrimeshMeshEdits(const String& nodeId);
    bool showTrimeshGuideAttachmentMenu(const String& parameterField, Rectangle<int> targetScreenArea);
    std::array<String, 6> trimeshGuideAttachmentLabelsForNode(const Node& meshNode);
    bool selectTrimeshVertexIndex(int vertexIndex);
    bool canConnectPorts(const PortAddress& first, const PortAddress& second) const;
    void updatePaletteHover(Point<float> screenPosition);
    Path createCablePath(
            Point<float> source,
            Point<float> dest,
            PortSide sourceSide,
            PortSide destSide,
            bool attachment) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeCanvas)
};

}
