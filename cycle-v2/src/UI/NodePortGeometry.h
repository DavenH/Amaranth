#pragma once

#include <JuceHeader.h>

#include "../Graph/NodeGraph.h"

namespace CycleV2 {

struct NodePortGeometry {
    static constexpr float referenceZoom = 0.58f;
    static constexpr float socketDiameter = 8.4f;
    static constexpr float hitPadding = 10.f;
    static constexpr float socketIconGap = 3.6f;
    static constexpr float iconSize = 18.f;
    static constexpr float firstSidePortOffset = 58.f;
    static constexpr float sidePortSpacing = 54.f;
    static constexpr float iconLaneExtent =
            socketDiameter * 0.5f + socketIconGap + iconSize;
    static constexpr float iconLaneExtentWorld = iconLaneExtent / referenceZoom;

    static float sidePortCentreY(Rectangle<float> nodeBounds, int index) {
        return nodeBounds.getY() + firstSidePortOffset + (float) index * sidePortSpacing;
    }

    static Point<float> socketCentreForAttachedIcon(
            Point<float> nodeBoundary,
            PortSide side) {
        switch (side) {
            case PortSide::Left:   nodeBoundary.x -= iconLaneExtentWorld; break;
            case PortSide::Right:  nodeBoundary.x += iconLaneExtentWorld; break;
            case PortSide::Top:    nodeBoundary.y -= iconLaneExtentWorld; break;
            case PortSide::Bottom: nodeBoundary.y += iconLaneExtentWorld; break;
        }
        return nodeBoundary;
    }

    static Rectangle<float> iconBounds(
            Point<float> socketCentre,
            PortSide side,
            float zoom) {
        const float scale = zoom / referenceZoom;
        const float size = iconSize * scale;
        const float offset = (socketDiameter * 0.5f + socketIconGap + iconSize * 0.5f) * scale;
        Point<float> centre = socketCentre;
        switch (side) {
            case PortSide::Left:   centre.x += offset; break;
            case PortSide::Right:  centre.x -= offset; break;
            case PortSide::Top:    centre.y += offset; break;
            case PortSide::Bottom: centre.y -= offset; break;
        }
        return Rectangle<float>(size, size).withCentre(centre);
    }
};

}
