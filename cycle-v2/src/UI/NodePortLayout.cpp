#include <algorithm>

#include "NodePortLayout.h"
#include "NodePortGeometry.h"
#include "NodePortVisualResolver.h"

namespace CycleV2 {

namespace {

bool hasInputIconOnSide(const Node& node, PortSide side) {
    return std::any_of(node.inputs.begin(), node.inputs.end(), [side](const Port& port) {
        return port.side == side
                && (NodePortVisualResolver::semanticFor(port) != PortVisualSemantic::None
                    || port.defaultModulationSlot != DefaultModulationSlot::None);
    });
}

}

NodePortGutters NodePortLayout::guttersFor(const Node& node, float zoom) {
    const float gutter = NodePortGeometry::iconGutter
            * zoom / NodePortGeometry::referenceZoom;
    return {
            hasInputIconOnSide(node, PortSide::Left) ? gutter : 0.f,
            hasInputIconOnSide(node, PortSide::Top) ? gutter : 0.f,
            hasInputIconOnSide(node, PortSide::Bottom) ? gutter : 0.f
    };
}

Rectangle<float> NodePortLayout::reservePortGutters(
        const Node& node,
        Rectangle<float> content,
        float zoom) {
    const NodePortGutters gutters = guttersFor(node, zoom);
    return content
            .withTrimmedLeft(gutters.left)
            .withTrimmedTop(gutters.top)
            .withTrimmedBottom(gutters.bottom);
}

}
