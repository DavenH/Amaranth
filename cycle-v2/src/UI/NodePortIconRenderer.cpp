#include <map>

#include "NodePortIconRenderer.h"
#include "EnvelopePurposeIconRenderer.h"
#include "NodePaletteEntryIconRenderer.h"
#include "PortIconData.h"

namespace CycleV2 {

namespace {

using IconMap = std::map<PortVisualSemantic, std::unique_ptr<Drawable>>;

const Colour kBadgeBackground { 0xff101318 };
const Colour kBadgeBorder { 0xff596776 };

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
                PortVisualSemantic::UnisonConfiguration
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
    if (semantic == PortVisualSemantic::PitchEnvelope) {
        return EnvelopePurposeIconRenderer::hasIcon(EnvelopePurpose::Pitch);
    }
    if (semantic == PortVisualSemantic::ScratchAttachment) {
        return EnvelopePurposeIconRenderer::hasIcon(EnvelopePurpose::Scratch);
    }
    if (semantic == PortVisualSemantic::VoiceContext) {
        return NodePaletteEntryIconRenderer::hasIcon(NodeKind::VoiceContext);
    }
    return drawableFor(semantic) != nullptr;
}

void NodePortIconRenderer::paint(
        Graphics& graphics,
        PortVisualSemantic semantic,
        Rectangle<float> area,
        bool mirrored,
        float opacity) {
    if (!hasIcon(semantic)) {
        return;
    }

    const Rectangle<float> badge = area.expanded(area.getWidth() * 0.10f);
    const float corner = area.getWidth() * 0.28f;
    graphics.setColour(kBadgeBackground.withAlpha(0.96f));
    graphics.fillRoundedRectangle(badge, corner);
    graphics.setColour(kBadgeBorder.withAlpha(0.62f));
    graphics.drawRoundedRectangle(badge, corner, jmax(0.8f, area.getWidth() * 0.06f));

    if (semantic == PortVisualSemantic::PitchEnvelope) {
        EnvelopePurposeIconRenderer::paint(
                graphics,
                EnvelopePurpose::Pitch,
                area,
                opacity);
        return;
    }
    if (semantic == PortVisualSemantic::ScratchAttachment) {
        EnvelopePurposeIconRenderer::paint(
                graphics,
                EnvelopePurpose::Scratch,
                area,
                opacity);
        return;
    }
    if (semantic == PortVisualSemantic::VoiceContext) {
        NodePaletteEntryIconRenderer::paint(
                graphics,
                NodeKind::VoiceContext,
                area,
                true);
        return;
    }

    const Drawable* drawable = drawableFor(semantic);

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
