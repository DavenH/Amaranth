#pragma once

#include <JuceHeader.h>

#include <memory>

#include "../Graph/GraphEditor.h"
#include "../Graph/GraphRenderSemanticResolver.h"
#include "../Graph/NodeGraph.h"
#include "../Graph/GraphSerializer.h"
#include "../Nodes/Trimesh/TrimeshExpandedEditorComponent.h"
#include "../Nodes/Trimesh/TrimeshWidget.h"
#include "../Runtime/GraphPreviewExecutor.h"
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
    GraphCompileResult compileResult;
    RuntimeProcessTrace runtimeTrace;
    GraphPreviewResult previewResult;
    std::vector<String> undoStack;
    std::vector<String> redoStack;
    std::vector<std::pair<String, CachedPreviewSprite>> previewSpriteCache;
    std::vector<std::pair<String, std::unique_ptr<TrimeshWidget>>> trimeshWidgets;
    std::unique_ptr<TrimeshExpandedEditorComponent> trimeshExpandedEditor;
    String trimeshExpandedEditorNodeId;

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

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void timerCallback() override;

    void drawGrid(Graphics& g);
    void drawGlEdges();
    void drawGlNodeShells();
    void drawGlExpandedPanels();
    void drawEdges(Graphics& g);
    void drawConnectionPreview(Graphics& g);
    void drawNodes(Graphics& g);
    void drawSnapGuides(Graphics& g);
    void drawNode(Graphics& g, const Node& node);
    void drawPreview(Graphics& g, const Node& node, Rectangle<float> area);
    void drawPreviewUncached(Graphics& g, const Node& node, Rectangle<float> area, PortDomain previewDomain);
    CachedPreviewSprite& cachedPreviewSpriteFor(const String& nodeId);
    TrimeshWidget& trimeshWidgetFor(const String& nodeId);
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
    bool selectTrimeshVertexIndex(int vertexIndex);
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
