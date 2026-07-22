#include "NodePaletteIconRenderer.h"

#include "NodePaletteIconData.h"

#include <array>

namespace CycleV2 {

namespace {

constexpr size_t kIconCount = static_cast<size_t>(PaletteIcon::Count);

const char* svgFor(PaletteIcon icon) {
    switch (icon) {
        case PaletteIcon::Context:   return PaletteIconData::context;
        case PaletteIcon::Transform: return PaletteIconData::transform;
        case PaletteIcon::Math:      return PaletteIconData::math;
        case PaletteIcon::Source:    return PaletteIconData::source;
        case PaletteIcon::Control:   return PaletteIconData::control;
        case PaletteIcon::Fx:        return PaletteIconData::fx;
        case PaletteIcon::Channel:   return PaletteIconData::channel;
        case PaletteIcon::Count:     break;
    }

    jassertfalse;
    return nullptr;
}

std::unique_ptr<Drawable> createIcon(PaletteIcon icon) {
    const char* svg = svgFor(icon);
    if (svg == nullptr) {
        return {};
    }

    const std::unique_ptr<XmlElement> document = parseXML(String::fromUTF8(svg));
    jassert(document != nullptr);
    return document != nullptr ? Drawable::createFromSVG(*document) : nullptr;
}

const Drawable* drawableFor(PaletteIcon icon) {
    static const std::array<std::unique_ptr<Drawable>, kIconCount> icons = [] {
        std::array<std::unique_ptr<Drawable>, kIconCount> result;
        for (size_t iconIndex = 0; iconIndex < result.size(); ++iconIndex) {
            result[iconIndex] = createIcon(static_cast<PaletteIcon>(iconIndex));
        }

        return result;
    }();

    const size_t iconIndex = static_cast<size_t>(icon);
    jassert(iconIndex < icons.size());
    return iconIndex < icons.size() ? icons[iconIndex].get() : nullptr;
}

}

void NodePaletteIconRenderer::paint(
        Graphics& graphics,
        PaletteIcon icon,
        Rectangle<float> area,
        bool active) {
    const Drawable* drawable = drawableFor(icon);
    if (drawable == nullptr) {
        return;
    }

    drawable->drawWithin(
            graphics,
            area,
            RectanglePlacement::centred,
            active ? 1.f : 0.78f);
}

}
