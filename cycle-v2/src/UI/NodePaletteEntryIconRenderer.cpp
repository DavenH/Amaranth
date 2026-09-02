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
    NodeIconRenderer::paint(graphics, kind, area, hover ? 1.f : 0.88f);
}

}
