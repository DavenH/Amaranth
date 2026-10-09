#include "UI/NodePaletteEntryIconRenderer.h"

#include "UI/NodeIconRenderer.h"

namespace CycleV2 {

namespace {

const char* transformIconId(NodeKind kind) {
    switch (kind) {
        case NodeKind::Fft:  return "fourier";
        case NodeKind::Ifft: return "inverseFourier";
        default:            return nullptr;
    }
}

}

bool NodePaletteEntryIconRenderer::hasIcon(NodeKind kind) {
    const char* iconId = transformIconId(kind);
    return iconId != nullptr ? NodeIconRenderer::hasIcon(iconId) : NodeIconRenderer::hasIcon(kind);
}

void NodePaletteEntryIconRenderer::paint(
        Graphics& graphics,
        NodeKind kind,
        Rectangle<float> area,
        bool hover) {
    const float opacity = hover ? 1.f : 0.88f;
    const auto colour = hover ? NodeIconColour::Semantic : NodeIconColour::Monochrome;
    if (const char* iconId = transformIconId(kind)) {
        NodeIconRenderer::paint(graphics, iconId, area, opacity, colour);
    } else {
        NodeIconRenderer::paint(graphics, kind, area, opacity, colour);
    }
}

}
