#include "UI/NodeIconRenderer.h"

#include "Graph/NodeDefinition.h"
#include "NodeIconData.h"

#include <map>

namespace CycleV2 {

namespace {

using IconMap = std::map<String, std::unique_ptr<Drawable>>;

std::unique_ptr<Drawable> createIcon(const char* svg) {
    const std::unique_ptr<XmlElement> document = parseXML(String::fromUTF8(svg));
    jassert(document != nullptr);
    return document != nullptr ? Drawable::createFromSVG(*document) : nullptr;
}

const Drawable* drawableFor(const String& semanticId) {
    static const IconMap icons = [] {
        IconMap result;

        for (const auto& source : NodeIconData::sources) {
            result.emplace(String::fromUTF8(source.name), createIcon(source.svg));
        }

        return result;
    }();

    const auto match = icons.find(semanticId);
    jassert(match != icons.end());
    return match != icons.end() ? match->second.get() : nullptr;
}

const Drawable* drawableFor(NodeKind kind) {
    const NodeDefinition* definition = NodeDefinitionRegistry::instance().find(kind);
    jassert(definition != nullptr);
    return definition != nullptr ? drawableFor(definition->typeId) : nullptr;
}

}

bool NodeIconRenderer::hasIcon(NodeKind kind) {
    return drawableFor(kind) != nullptr;
}

bool NodeIconRenderer::hasIcon(const String& semanticId) {
    return drawableFor(semanticId) != nullptr;
}

void NodeIconRenderer::paint(
        Graphics& graphics,
        NodeKind kind,
        Rectangle<float> area,
        float opacity) {
    const Drawable* drawable = drawableFor(kind);
    if (drawable == nullptr) {
        return;
    }

    drawable->drawWithin(
            graphics,
            area,
            RectanglePlacement::centred,
            jlimit(0.f, 1.f, opacity));
}

void NodeIconRenderer::paint(
        Graphics& graphics,
        const String& semanticId,
        Rectangle<float> area,
        float opacity) {
    const Drawable* drawable = drawableFor(semanticId);
    if (drawable == nullptr) {
        return;
    }

    drawable->drawWithin(
            graphics,
            area,
            RectanglePlacement::centred,
            jlimit(0.f, 1.f, opacity));
}

}
