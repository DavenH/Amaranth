#pragma once

#include <JuceHeader.h>

#include "../Graph/NodeGraph.h"

namespace CycleV2 {

struct NodePortGeometry {
    static constexpr float referenceZoom = 0.58f;
    static constexpr float socketDiameter = 8.4f;
    static constexpr float hitPadding = 10.f;
    static constexpr float socketIconGap = 3.6f;
    static constexpr float iconSize = 14.f;
    static constexpr float iconGutter = 24.f;
    static constexpr float firstSidePortOffset = 58.f;
    static constexpr float sidePortSpacing = 34.f;

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
