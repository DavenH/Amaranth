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

}
