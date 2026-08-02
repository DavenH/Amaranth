#include <map>

#include "EnvelopePurposeIconRenderer.h"
#include "EnvelopePurposeIconData.h"

namespace CycleV2 {

namespace {

using IconMap = std::map<EnvelopePurpose, std::unique_ptr<Drawable>>;

std::unique_ptr<Drawable> createIcon(const char* svg) {
    const std::unique_ptr<XmlElement> document = parseXML(String::fromUTF8(svg));
    jassert(document != nullptr);
    return document != nullptr ? Drawable::createFromSVG(*document) : nullptr;
}

const Drawable* drawableFor(EnvelopePurpose purpose) {
    static const IconMap icons = [] {
        IconMap result;
        for (const auto& source : EnvelopePurposeIconData::sources) {
            result.emplace(
                    envelopePurposeFromString(String::fromUTF8(source.name)),
                    createIcon(source.svg));
        }
        return result;
    }();

    const auto match = icons.find(purpose);
    jassert(match != icons.end());
    return match != icons.end() ? match->second.get() : nullptr;
}

}

bool EnvelopePurposeIconRenderer::hasIcon(EnvelopePurpose purpose) {
    return drawableFor(purpose) != nullptr;
}

void EnvelopePurposeIconRenderer::paint(
        Graphics& graphics,
        EnvelopePurpose purpose,
        Rectangle<float> area,
        float opacity) {
    const Drawable* drawable = drawableFor(purpose);
    if (drawable == nullptr) {
        return;
    }

    drawable->drawWithin(graphics, area, RectanglePlacement::centred, opacity);
}

}
