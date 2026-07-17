#pragma once

#include <JuceHeader.h>

#include <optional>

#include "NodeCanvasQueryModel.h"
#include "NodeCanvasScene.h"
#include "NodeCanvasViewport.h"
#include "NodePalette.h"

namespace CycleV2 {

enum class CanvasNodeActionKind {
    CycleOperationLayout,
    CycleMeshOutputSide,
    CycleVoiceDomain
};

struct CanvasNodeAction {
    CanvasNodeActionKind kind;
    String nodeId;
};

class NodeCanvasHitRouter {
public:
    NodeCanvasHitRouter(
            const NodeGraph& graph,
            const NodePalette& palette,
            const NodeCanvasQueryModel& queries);

    std::optional<CanvasNodeAction> nodeActionAt(
            const NodeCanvasViewport& viewport,
            Point<float> screenPosition) const;
    int edgeAt(
            const NodeCanvasSceneSnapshot& scene,
            Point<float> screenPosition) const;
    int spliceTargetEdgeAt(
            const NodeCanvasSceneSnapshot& scene,
            Point<float> screenPosition,
            const String& nodeId) const;
    String hoverTextFor(
            const NodeCanvasViewport& viewport,
            const NodeCanvasSceneSnapshot& scene,
            Point<float> screenPosition) const;

    Point<float> paletteCreationWorldPosition(
            const NodeCanvasViewport& viewport,
            Rectangle<float> canvasBounds,
            NodeKind kind,
            Point<float> paletteClickPosition) const;
    Rectangle<float> paletteDragBounds(
            const NodeCanvasViewport& viewport,
            const Node& node,
            Point<float> paletteClickPosition) const;

private:
    const NodeGraph& graph;
    const NodePalette& palette;
    const NodeCanvasQueryModel& queries;
    NodeCanvasHitTester hitTester;
};

}
