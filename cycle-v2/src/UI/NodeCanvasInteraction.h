#pragma once

#include <JuceHeader.h>

#include <optional>
#include <variant>

#include "NodeCanvasScene.h"
#include "NodeCanvasViewport.h"
#include "../Graph/GraphEditor.h"

namespace CycleV2 {

struct NodeSnapGuides {
    std::optional<float> x;
    std::optional<float> y;
};

struct SnappedNodeBounds {
    Rectangle<float> bounds;
    NodeSnapGuides guides;
};

struct CanvasPanGesture {
    Point<float> startPan;
};

struct NodeDragGesture {
    String nodeId;
    Rectangle<float> startBounds;
    NodeSnapGuides guides;
    bool moved {};
    bool transactionRequested {};
};

struct PortConnectionGesture {
    PortAddress source;
    Point<float> endpoint;
};

struct ExpandedEditorGesture {
};

using NodeCanvasGesture = std::variant<
        std::monostate,
        CanvasPanGesture,
        NodeDragGesture,
        PortConnectionGesture,
        ExpandedEditorGesture>;

struct PanDragUpdate {
    Point<float> pan;
};

struct NodeDragUpdate {
    String nodeId;
    Rectangle<float> bounds;
    NodeSnapGuides guides;
    bool beginTransaction {};
    bool moved {};
};

struct ConnectionDragUpdate {
    PortAddress source;
    Point<float> endpoint;
    std::optional<PortAddress> target;
};

using NodeCanvasDragUpdate = std::variant<
        std::monostate,
        PanDragUpdate,
        NodeDragUpdate,
        ConnectionDragUpdate>;

struct NodeDragCompletion {
    String nodeId;
    bool moved {};
};

struct ConnectionCompletion {
    PortAddress source;
    std::optional<PortAddress> target;
};

using NodeCanvasGestureCompletion = std::variant<
        std::monostate,
        NodeDragCompletion,
        ConnectionCompletion>;

class NodeCanvasInteraction {
public:
    void beginPan(Point<float> startPan);
    void beginNodeDrag(const String& nodeId, Rectangle<float> startBounds);
    void beginConnection(const PortAddress& source, Point<float> endpoint);
    void captureExpandedEditor();
    void reset();

    const NodeCanvasGesture& gesture() const { return currentGesture; }
    bool isIdle() const;

    std::optional<NodeSceneTarget> hitAt(
            const NodeCanvasSceneSnapshot& scene,
            Point<float> screenPosition) const;
    std::optional<PortAddress> portAt(
            const NodeCanvasSceneSnapshot& scene,
            Point<float> screenPosition) const;
    std::optional<PortAddress> connectionTargetAt(
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene,
            const PortAddress& source,
            Point<float> screenPosition) const;

    SnappedNodeBounds snapNode(
            const NodeGraph& graph,
            const Node& node,
            Rectangle<float> proposed) const;
    NodeCanvasDragUpdate drag(
            const NodeGraph& graph,
            const NodeCanvasViewport& viewport,
            const NodeCanvasSceneSnapshot& scene,
            Point<float> screenPosition,
            Point<float> dragOffset);
    NodeCanvasGestureCompletion finish(
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene,
            Point<float> screenPosition);

private:
    NodeCanvasGesture currentGesture;
    NodeCanvasHitTester hitTester;
};

}
