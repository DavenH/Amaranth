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

const Drawable* neutralDrawableFor(EnvelopePurpose purpose) {
    static const IconMap icons = [] {
        IconMap result;
        for (const EnvelopePurpose candidate : kEnvelopePurposes) {
            const Drawable* source = drawableFor(candidate);
            if (source == nullptr) {
                continue;
            }
            auto icon = source->createCopy();
            icon->replaceColour(Colour(0xffb8ff5c), Colour(0xffc5cad3));
            icon->replaceColour(Colour(0xff67a7ff), Colour(0xffc5cad3));
            result.emplace(candidate, std::move(icon));
        }
        return result;
    }();

    const auto match = icons.find(purpose);
    return match != icons.end() ? match->second.get() : nullptr;
}

void paintDrawable(
        Graphics& graphics,
        const Drawable* drawable,
        Rectangle<float> area,
        float opacity) {
    if (drawable != nullptr) {
        drawable->drawWithin(graphics, area, RectanglePlacement::centred, opacity);
    }
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
    paintDrawable(graphics, drawableFor(purpose), area, opacity);
}

void EnvelopePurposeIconRenderer::paintNeutral(
        Graphics& graphics,
        EnvelopePurpose purpose,
        Rectangle<float> area,
        float opacity) {
    paintDrawable(graphics, neutralDrawableFor(purpose), area, opacity);
}

}
