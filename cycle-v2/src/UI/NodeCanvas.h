#pragma once

#include <JuceHeader.h>

#include "../Graph/NodeGraph.h"
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

private:
    struct PortLocation {
        Rectangle<float> bounds;
        Point<float> centre;
    };

    OpenGLContext openGLContext;
    NodeGraph graph;
    GraphCompileResult compileResult;
    RuntimeProcessTrace runtimeTrace;

    float zoom { 0.58f };
    Point<float> pan { 34.f, 38.f };

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void timerCallback() override;

    void drawGrid(Graphics& g);
    void drawEdges(Graphics& g);
    void drawNodes(Graphics& g);
    void drawNode(Graphics& g, const Node& node);
    void drawPreview(Graphics& g, const Node& node, Rectangle<float> area);
    void drawMiniMap(Graphics& g);

    Point<float> toScreen(Point<float> p) const;
    Rectangle<float> toScreen(Rectangle<float> r) const;
    PortLocation getPortLocation(const Node& node, const Port& port) const;
    const Node* findNode(const String& id) const;
    const Port* findPort(const Node& node, const String& portId, bool input) const;
    const RuntimeNodeTrace* findRuntimeTrace(const String& nodeId) const;
    int executionIndexForNode(const String& nodeId) const;
    Path createCablePath(Point<float> source, Point<float> dest, bool attachment) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeCanvas)
};

}
