#pragma once

#include <JuceHeader.h>

#include "NodeCanvasScene.h"

namespace CycleV2 {

struct NodeCableStyle {
    Colour colour;
    bool attachment {};
    bool invalid {};
    bool selected {};
    bool spliceTarget {};
};

struct PendingNodeConnection {
    Path path;
    Point<float> source;
    Point<float> destination;
    Colour colour;
};

class NodeCableRenderer {
public:
    static float scaleForZoom(float zoom);
    static Rectangle<float> visibleBounds(const NodeSceneEdge& edge, float zoom);
    static void paint(
            Graphics& graphics,
            const NodeSceneEdge& edge,
            const NodeCableStyle& style,
            float zoom);
    static void paintPending(
            Graphics& graphics,
            const PendingNodeConnection& connection,
            float zoom);
};

}
