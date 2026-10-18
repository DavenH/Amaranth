#include <map>

#include "UI/EnvelopePurposeIconRenderer.h"
#include "EnvelopePurposeIconData.h"

namespace CycleV2 {

namespace {

using IconMap = std::map<EnvelopePurpose, std::unique_ptr<Drawable>>;

std::unique_ptr<Drawable> createIcon(const String& svg) {
    const std::unique_ptr<XmlElement> document = parseXML(svg);
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

const Drawable* monochromeDrawableFor(EnvelopePurpose purpose) {
    static const IconMap icons = [] {
        IconMap result;
        for (const auto& source : EnvelopePurposeIconData::sources) {
            const String svg = String::fromUTF8(source.svg)
                    .replace("#b8ff5c", "#8793a1");
            result.emplace(
                    envelopePurposeFromString(String::fromUTF8(source.name)),
                    createIcon(svg));
        }
        return result;
    }();

    const auto match = icons.find(purpose);
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

void EnvelopePurposeIconRenderer::paintMonochrome(
        Graphics& graphics,
        EnvelopePurpose purpose,
        Rectangle<float> area,
        float opacity) {
    const Drawable* drawable = monochromeDrawableFor(purpose);
    if (drawable != nullptr) {
        drawable->drawWithin(graphics, area, RectanglePlacement::centred, opacity);
    }
}

}
