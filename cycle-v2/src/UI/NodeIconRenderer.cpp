#include <map>

#include "UI/NodeIconRenderer.h"

#include "Graph/NodeDefinition.h"
#include "NodeIconData.h"

namespace CycleV2 {

namespace {

struct Icon {
    std::unique_ptr<Drawable> drawable;
    std::unique_ptr<Drawable> monochrome;
    Rectangle<float> canvas;
};

using IconMap = std::map<String, Icon>;

void desaturateSvg(XmlElement& element) {
    for (const auto* attribute : { "fill", "stroke" }) {
        const String value = element.getStringAttribute(attribute);
        if (value.startsWithChar('#') && value.length() == 7) {
            const auto colour = Colour::fromString("ff" + value.substring(1)).withSaturation(0.f);
            element.setAttribute(attribute, "#" + colour.toDisplayString(false));
        }
    }
    for (auto* child = element.getFirstChildElement(); child != nullptr; child = child->getNextElement()) {
        desaturateSvg(*child);
    }
}

Icon createIcon(const char* svg) {
    const std::unique_ptr<XmlElement> document = parseXML(String::fromUTF8(svg));
    jassert(document != nullptr);
    if (document == nullptr) {
        return {};
    }

    const auto coordinates = StringArray::fromTokens(document->getStringAttribute("viewBox"), false);
    if (coordinates.size() != 4) {
        return {};
    }
    const Rectangle<float> canvas(coordinates[0].getFloatValue(), coordinates[1].getFloatValue(),
            coordinates[2].getFloatValue(), coordinates[3].getFloatValue());
    document->setAttribute("width", canvas.getWidth());
    document->setAttribute("height", canvas.getHeight());
    auto drawable = Drawable::createFromSVG(*document);
    desaturateSvg(*document);
    return { std::move(drawable), Drawable::createFromSVG(*document), canvas };
}

void paintIcon(Graphics& graphics, const Icon* icon, Rectangle<float> area,
        float opacity, NodeIconColour colour) {
    if (icon == nullptr) {
        return;
    }
    const auto* drawable = colour == NodeIconColour::Monochrome
            ? icon->monochrome.get() : icon->drawable.get();
    if (drawable == nullptr) {
        return;
    }
    const auto transform = RectanglePlacement(RectanglePlacement::centred)
            .getTransformToFit(icon->canvas, area);
    drawable->draw(graphics, jlimit(0.f, 1.f, opacity), transform);
}

const Icon* iconFor(const String& semanticId) {
    static const IconMap icons = [] {
        IconMap result;

        for (const auto& source : NodeIconData::sources) {
            result.emplace(String::fromUTF8(source.name), createIcon(source.svg));
        }

        return result;
    }();

    const auto match = icons.find(semanticId);
    jassert(match != icons.end());
    return match != icons.end() ? &match->second : nullptr;
}

const Icon* iconFor(NodeKind kind) {
    if (kind == NodeKind::Fft) {
        return iconFor("fourier");
    }
    if (kind == NodeKind::Ifft) {
        return iconFor("inverseFourier");
    }
    const NodeDefinition* definition = NodeDefinitionRegistry::instance().find(kind);
    jassert(definition != nullptr);
    return definition != nullptr ? iconFor(definition->typeId) : nullptr;
}

}

bool NodeIconRenderer::hasIcon(NodeKind kind) {
    const auto* icon = iconFor(kind);
    return icon != nullptr && icon->drawable != nullptr;
}

bool NodeIconRenderer::hasIcon(const String& semanticId) {
    const auto* icon = iconFor(semanticId);
    return icon != nullptr && icon->drawable != nullptr;
}

void NodeIconRenderer::paint(
        Graphics& graphics,
        NodeKind kind,
        Rectangle<float> area,
        float opacity,
        NodeIconColour colour) {
    paintIcon(graphics, iconFor(kind), area, opacity, colour);
}

void NodeIconRenderer::paint(
        Graphics& graphics,
        const String& semanticId,
        Rectangle<float> area,
        float opacity,
        NodeIconColour colour) {
    paintIcon(graphics, iconFor(semanticId), area, opacity, colour);
}

}
