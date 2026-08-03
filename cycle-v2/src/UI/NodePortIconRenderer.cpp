#include <map>

#include "NodePortIconRenderer.h"
#include "PortIconData.h"

namespace CycleV2 {

namespace {

using IconMap = std::map<PortVisualSemantic, std::unique_ptr<Drawable>>;

std::unique_ptr<Drawable> createIcon(const char* svg) {
    const std::unique_ptr<XmlElement> document = parseXML(String::fromUTF8(svg));
    jassert(document != nullptr);
    return document != nullptr ? Drawable::createFromSVG(*document) : nullptr;
}

const Drawable* drawableFor(PortVisualSemantic semantic) {
    static const IconMap icons = [] {
        IconMap result;
        const PortVisualSemantic semantics[] {
                PortVisualSemantic::ModulationYrb,
                PortVisualSemantic::ModulationRb,
                PortVisualSemantic::PitchEnvelope,
                PortVisualSemantic::UnisonConfiguration,
                PortVisualSemantic::VoiceContext,
                PortVisualSemantic::ScratchAttachment
        };
        int index = 0;
        for (const auto& source : PortIconData::sources) {
            result.emplace(semantics[index++], createIcon(source.svg));
        }
        return result;
    }();

    const auto match = icons.find(semantic);
    return match != icons.end() ? match->second.get() : nullptr;
}

}

bool NodePortIconRenderer::hasIcon(PortVisualSemantic semantic) {
    return drawableFor(semantic) != nullptr;
}

void NodePortIconRenderer::paint(
        Graphics& graphics,
        PortVisualSemantic semantic,
        Rectangle<float> area,
        bool mirrored,
        float opacity) {
    const Drawable* drawable = drawableFor(semantic);
    if (drawable == nullptr) {
        return;
    }

    if (!mirrored) {
        drawable->drawWithin(graphics, area, RectanglePlacement::centred, opacity);
        return;
    }

    Graphics::ScopedSaveState savedState(graphics);
    graphics.addTransform(AffineTransform::scale(
            -1.f,
            1.f,
            area.getCentreX(),
            area.getCentreY()));
    drawable->drawWithin(graphics, area, RectanglePlacement::centred, opacity);
}

}
