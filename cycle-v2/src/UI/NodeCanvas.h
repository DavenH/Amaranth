#pragma once

#include <JuceHeader.h>

#include "../Graph/GraphEditor.h"
#include "../Graph/NodeGraph.h"
#include "../Graph/GraphSerializer.h"
#include "../Runtime/GraphRuntime.h"

namespace CycleV2 {

class NodeCanvas :
        public Component
    ,   private OpenGLRenderer
    ,   private Timer {
public:
    NodeCanvas();
    ~NodeCanvas() override;

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

    OpenGLContext openGLContext;
    NodeGraph graph;
    GraphCompileResult compileResult;
    RuntimeProcessTrace runtimeTrace;
    std::vector<String> undoStack;
    std::vector<String> redoStack;

    float zoom { 0.58f };
    Point<float> pan { 34.f, 38.f };
    Point<float> dragStartPan;
    Rectangle<float> dragStartNodeBounds;
    String selectedNodeId;
    String expandedNodeId;
    String editStatusMessage;
    int selectedEdgeIndex { -1 };
    PortAddress connectingPort;
    Point<float> connectingPoint;
    Point<float> lastMousePosition;
    bool draggingNode {};
    bool connectingCable {};
    bool nodeDragUndoPushed {};

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void timerCallback() override;

    void drawGrid(Graphics& g);
    void drawEdges(Graphics& g);
    void drawConnectionPreview(Graphics& g);
    void drawNodes(Graphics& g);
    void drawNode(Graphics& g, const Node& node);
    void drawPreview(Graphics& g, const Node& node, Rectangle<float> area);
    void drawSpectrumBars(Graphics& g, Rectangle<float> area, Colour colour, int seed);
    void drawPhaseTrace(Graphics& g, Rectangle<float> area, Colour colour, int seed);
    void drawEnvelopeCurve(Graphics& g, Rectangle<float> area);
    void drawExpandedEditor(Graphics& g, const Node& node);
    void drawMiniMap(Graphics& g);
    void drawGraphStatus(Graphics& g);
    void drawEdgeLegend(Graphics& g);
    void drawNodePalette(Graphics& g);
    void drawHoverConsole(Graphics& g);

    Point<float> toScreen(Point<float> p) const;
    Point<float> toWorld(Point<float> p) const;
    Rectangle<float> toScreen(Rectangle<float> r) const;
    PortLocation getPortLocation(const Node& node, const Port& port) const;
    PortLocation getPortLocation(const PortAddress& address) const;
    bool findPortAt(Point<float> screenPosition, PortAddress& result) const;
    bool findConnectablePortAt(Point<float> screenPosition, const PortAddress& source, PortAddress& result) const;
    bool findPaletteKindAt(Point<float> screenPosition, NodeKind& kind) const;
    bool findOperationLayoutButtonAt(Point<float> screenPosition, String& nodeId) const;
    int findEdgeAt(Point<float> screenPosition) const;
    const Node* findNode(const String& id) const;
    Node* findMutableNode(const String& id);
    const Node* findNodeAt(Point<float> worldPosition) const;
    const Port* findPort(const Node& node, const String& portId, bool input) const;
    const RuntimeNodeTrace* findRuntimeTrace(const String& nodeId) const;
    int executionIndexForNode(const String& nodeId) const;
    int attachmentCount() const;
    String hoverTextFor(Point<float> screenPosition) const;
    String textForPort(const PortAddress& address) const;
    String textForNode(const Node& node) const;
    Point<float> viewportCentreWorld() const;
    Point<float> paletteCreationWorldPosition(Point<float> paletteClickPosition) const;
    void refreshCompiledState();
    File snapshotFile() const;
    bool saveSnapshot();
    bool loadSnapshot();
    bool undo();
    bool redo();
    void pushUndoSnapshot();
    void pushUndoSnapshot(String xml);
    bool restoreGraphXml(const String& xml, const String& statusMessage);
    bool clearSelection();
    bool cycleOperationPortLayout(const String& nodeId);
    bool canConnectPorts(const PortAddress& first, const PortAddress& second) const;
    Path createCablePath(Point<float> source, Point<float> dest, bool attachment) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeCanvas)
};

}
