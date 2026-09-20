#pragma once

#include <JuceHeader.h>

#include <optional>
#include <memory>
#include <variant>
#include <vector>

#include "UI/NodeCanvasScene.h"
#include "UI/NodeCanvasViewport.h"
#include "Graph/GraphEditTypes.h"
#include "Graph/GraphValidationContext.h"

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

struct AreaSelectionGesture {
    Point<float> start;
    Point<float> current;
    bool moved {};

    Rectangle<float> bounds() const {
        return Rectangle<float>::leftTopRightBottom(
                jmin(start.x, current.x),
                jmin(start.y, current.y),
                jmax(start.x, current.x),
                jmax(start.y, current.y));
    }
};

struct NodeDragGesture {
    String nodeId;
    std::vector<String> nodeIds;
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
        AreaSelectionGesture,
        NodeDragGesture,
        PortConnectionGesture,
        ExpandedEditorGesture>;

struct PanDragUpdate {
    Point<float> pan;
};

struct AreaSelectionDragUpdate {
    Rectangle<float> bounds;
    bool moved {};
};

struct NodeDragUpdate {
    String nodeId;
    std::vector<String> nodeIds;
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
        AreaSelectionDragUpdate,
        NodeDragUpdate,
        ConnectionDragUpdate>;

struct NodeDragCompletion {
    String nodeId;
    std::vector<String> nodeIds;
    bool moved {};
};

struct AreaSelectionCompletion {
    Rectangle<float> bounds;
    bool moved {};
};

struct ConnectionCompletion {
    PortAddress source;
    std::optional<PortAddress> target;
};

using NodeCanvasGestureCompletion = std::variant<
        std::monostate,
        AreaSelectionCompletion,
        NodeDragCompletion,
        ConnectionCompletion>;

class NodeCanvasInteraction {
public:
    void beginPan(Point<float> startPan);
    void beginAreaSelection(Point<float> start);
    void beginNodeDrag(
            const NodeGraph& graph,
            const String& nodeId,
            std::vector<String> nodeIds,
            Rectangle<float> startBounds);
    void beginConnection(
            const NodeGraph& graph,
            const PortAddress& source,
            Point<float> endpoint);
    void captureExpandedEditor();
    void reset();

    const NodeCanvasGesture& gesture() const { return currentGesture; }
    bool isIdle() const;
    const GraphValidationContext* gestureValidationContext() const {
        return validationContext.get();
    }

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
    std::vector<String> nodeIdsIntersecting(
            const NodeGraph& graph,
            const NodeCanvasViewport& viewport,
            Rectangle<float> screenBounds) const;

    SnappedNodeBounds snapNode(
            const NodeGraph& graph,
            const Node& node,
            Rectangle<float> proposed,
            const std::vector<String>& excludedNodeIds = {}) const;
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
    std::unique_ptr<GraphValidationContext> validationContext;
};

}
