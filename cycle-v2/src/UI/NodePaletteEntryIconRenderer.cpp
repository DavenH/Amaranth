#include "NodePaletteEntryIconRenderer.h"

#include "../Graph/NodeDefinition.h"
#include "NodeIconData.h"

#include <map>

namespace CycleV2 {

namespace {

using IconMap = std::map<NodeKind, std::unique_ptr<Drawable>>;

std::unique_ptr<Drawable> createIcon(const char* svg) {
    const std::unique_ptr<XmlElement> document = parseXML(String::fromUTF8(svg));
    jassert(document != nullptr);
    return document != nullptr ? Drawable::createFromSVG(*document) : nullptr;
}

const Drawable* drawableFor(NodeKind kind) {
    static const IconMap icons = [] {
        IconMap result;
        const NodeDefinitionRegistry& registry = NodeDefinitionRegistry::instance();

        for (const auto& source : NodeIconData::sources) {
            const NodeDefinition* definition = registry.find(String::fromUTF8(source.name));
            jassert(definition != nullptr);
            if (definition != nullptr) {
                result.emplace(definition->kind, createIcon(source.svg));
            }
        }

        return result;
    }();

    const auto match = icons.find(kind);
    jassert(match != icons.end());
    return match != icons.end() ? match->second.get() : nullptr;
}

void useNeutralPalette(Drawable& icon) {
    const Colour accents[] {
            Colour(0xff35d6d2), Colour(0xff4fc2bf), Colour(0xff5f91e8),
            Colour(0xff63aeb2), Colour(0xff669f9f), Colour(0xff67a7ff),
            Colour(0xff72d49a), Colour(0xffb284ff), Colour(0xffd65a5a),
            Colour(0xffd7bf5f), Colour(0xfff4d35e), Colour(0xffffb347)
    };
    for (const Colour accent : accents) {
        icon.replaceColour(accent, Colour(0xffc5cad3));
    }
}

const Drawable* neutralDrawableFor(NodeKind kind) {
    static const IconMap icons = [] {
        IconMap result;
        const NodeDefinitionRegistry& registry = NodeDefinitionRegistry::instance();
        for (const auto& sourceData : NodeIconData::sources) {
            const NodeDefinition* definition = registry.find(String::fromUTF8(sourceData.name));
            if (definition == nullptr) {
                continue;
            }
            const Drawable* source = drawableFor(definition->kind);
            if (source == nullptr) {
                continue;
            }
            auto icon = source->createCopy();
            useNeutralPalette(*icon);
            result.emplace(definition->kind, std::move(icon));
        }
        return result;
    }();

    const auto match = icons.find(kind);
    return match != icons.end() ? match->second.get() : nullptr;
}

}

bool NodePaletteEntryIconRenderer::hasIcon(NodeKind kind) {
    return drawableFor(kind) != nullptr;
}

void NodePaletteEntryIconRenderer::paint(
        Graphics& graphics,
        NodeKind kind,
        Rectangle<float> area,
        bool hover) {
    const Drawable* drawable = drawableFor(kind);
    if (drawable == nullptr) {
        return;
    }

    drawable->drawWithin(
            graphics,
            area,
            RectanglePlacement::centred,
            hover ? 1.f : 0.88f);
}

void NodePaletteEntryIconRenderer::paintNeutral(
        Graphics& graphics,
        NodeKind kind,
        Rectangle<float> area,
        float opacity) {
    const Drawable* drawable = neutralDrawableFor(kind);
    if (drawable != nullptr) {
        drawable->drawWithin(graphics, area, RectanglePlacement::centred, opacity);
    }
}

}
