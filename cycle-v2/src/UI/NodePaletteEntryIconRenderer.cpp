#include "UI/NodePaletteEntryIconRenderer.h"

#include "UI/NodeIconRenderer.h"

namespace CycleV2 {

bool NodePaletteEntryIconRenderer::hasIcon(NodeKind kind) {
    return NodeIconRenderer::hasIcon(kind);
}

void NodePaletteEntryIconRenderer::paint(
        Graphics& graphics,
        NodeKind kind,
        Rectangle<float> area,
        bool hover) {
    const float opacity = hover ? 1.f : 0.88f;
    const auto colour = hover ? NodeIconColour::Semantic : NodeIconColour::Monochrome;
    NodeIconRenderer::paint(graphics, kind, area, opacity, colour);
}

}
