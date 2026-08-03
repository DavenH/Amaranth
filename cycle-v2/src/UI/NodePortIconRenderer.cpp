#include <map>

#include "NodePortIconRenderer.h"
#include "EnvelopePurposeIconRenderer.h"
#include "NodePaletteEntryIconRenderer.h"
#include "PortIconData.h"

namespace CycleV2 {

namespace {

using IconMap = std::map<PortVisualSemantic, std::unique_ptr<Drawable>>;

const Colour kBadgeBackground { 0xff171d24 };
const Colour kBadgeBorder { 0xff3d4a58 };

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

Path attachedBadgeOutline(Rectangle<float> badge, PortSide side, float corner) {
    Path path;
    path.startNewSubPath(badge.getRight(), badge.getY());
    path.lineTo(badge.getX() + corner, badge.getY());
    path.quadraticTo(badge.getX(), badge.getY(), badge.getX(), badge.getY() + corner);
    path.lineTo(badge.getX(), badge.getBottom() - corner);
    path.quadraticTo(badge.getX(), badge.getBottom(), badge.getX() + corner, badge.getBottom());
    path.lineTo(badge.getRight(), badge.getBottom());

    if (side == PortSide::Right) {
        path.applyTransform(AffineTransform::scale(
                -1.f,
                1.f,
                badge.getCentreX(),
                badge.getCentreY()));
    } else if (side == PortSide::Top) {
        path.applyTransform(AffineTransform::rotation(
                MathConstants<float>::halfPi,
                badge.getCentreX(),
                badge.getCentreY()));
    } else if (side == PortSide::Bottom) {
        path.applyTransform(AffineTransform::rotation(
                -MathConstants<float>::halfPi,
                badge.getCentreX(),
                badge.getCentreY()));
    }
    return path;
}

void paintAttachedBadge(
        Graphics& graphics,
        Rectangle<float> badge,
        PortSide side,
        float corner) {
    graphics.setColour(kBadgeBackground);
    graphics.fillRoundedRectangle(badge, corner);
    if (side == PortSide::Left) {
        graphics.fillRect(badge.withLeft(badge.getCentreX()));
    } else if (side == PortSide::Right) {
        graphics.fillRect(badge.withRight(badge.getCentreX()));
    } else if (side == PortSide::Top) {
        graphics.fillRect(badge.withTop(badge.getCentreY()));
    } else {
        graphics.fillRect(badge.withBottom(badge.getCentreY()));
    }

    graphics.setColour(kBadgeBorder);
    graphics.strokePath(
            attachedBadgeOutline(badge, side, corner),
            PathStrokeType(jmax(0.8f, badge.getWidth() * 0.05f)));
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
        PortSide side,
        float opacity) {
    if (!hasIcon(semantic)) {
        return;
    }

    const Rectangle<float> badge = area.expanded(area.getWidth() * 0.10f);
    const float corner = area.getWidth() * 0.28f;
    paintAttachedBadge(graphics, badge, side, corner);

    if (semantic == PortVisualSemantic::PitchEnvelope) {
        EnvelopePurposeIconRenderer::paintNeutral(
                graphics,
                EnvelopePurpose::Pitch,
                area,
                opacity);
        return;
    }
    if (semantic == PortVisualSemantic::ScratchAttachment) {
        EnvelopePurposeIconRenderer::paintNeutral(
                graphics,
                EnvelopePurpose::Scratch,
                area,
                opacity);
        return;
    }
    if (semantic == PortVisualSemantic::VoiceContext) {
        NodePaletteEntryIconRenderer::paintNeutral(
                graphics,
                NodeKind::VoiceContext,
                area,
                opacity);
        return;
    }

    const Drawable* drawable = drawableFor(semantic);

    if (side != PortSide::Right) {
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
