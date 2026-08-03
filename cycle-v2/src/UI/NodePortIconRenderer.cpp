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

Path verticalRailOutline(Rectangle<float> rail, bool nodeOnRight, float corner) {
    Path path;
    path.startNewSubPath(rail.getRight(), rail.getY());
    path.lineTo(rail.getX() + corner, rail.getY());
    path.quadraticTo(rail.getX(), rail.getY(), rail.getX(), rail.getY() + corner);
    path.lineTo(rail.getX(), rail.getBottom() - corner);
    path.quadraticTo(rail.getX(), rail.getBottom(), rail.getX() + corner, rail.getBottom());
    path.lineTo(rail.getRight(), rail.getBottom());
    if (!nodeOnRight) {
        path.applyTransform(AffineTransform::scale(
                -1.f,
                1.f,
                rail.getCentreX(),
                rail.getCentreY()));
    }
    return path;
}

Path horizontalRailOutline(Rectangle<float> rail, bool nodeBelow, float corner) {
    Path path;
    path.startNewSubPath(rail.getX(), rail.getBottom());
    path.lineTo(rail.getX(), rail.getY() + corner);
    path.quadraticTo(rail.getX(), rail.getY(), rail.getX() + corner, rail.getY());
    path.lineTo(rail.getRight() - corner, rail.getY());
    path.quadraticTo(rail.getRight(), rail.getY(), rail.getRight(), rail.getY() + corner);
    path.lineTo(rail.getRight(), rail.getBottom());
    if (!nodeBelow) {
        path.applyTransform(AffineTransform::scale(
                1.f,
                -1.f,
                rail.getCentreX(),
                rail.getCentreY()));
    }
    return path;
}

Path attachedRailOutline(Rectangle<float> rail, PortSide side, float corner) {
    if (side == PortSide::Left || side == PortSide::Right) {
        return verticalRailOutline(rail, side == PortSide::Left, corner);
    }
    return horizontalRailOutline(rail, side == PortSide::Top, corner);
}

void paintAttachedRail(
        Graphics& graphics,
        Rectangle<float> rail,
        PortSide side,
        float corner) {
    graphics.setColour(kBadgeBackground);
    graphics.fillRoundedRectangle(rail, corner);
    if (side == PortSide::Left) {
        graphics.fillRect(rail.withLeft(rail.getCentreX()));
    } else if (side == PortSide::Right) {
        graphics.fillRect(rail.withRight(rail.getCentreX()));
    } else if (side == PortSide::Top) {
        graphics.fillRect(rail.withTop(rail.getCentreY()));
    } else {
        graphics.fillRect(rail.withBottom(rail.getCentreY()));
    }

    graphics.setColour(kBadgeBorder);
    graphics.strokePath(
            attachedRailOutline(rail, side, corner),
            PathStrokeType(jmax(0.8f, jmin(rail.getWidth(), rail.getHeight()) * 0.05f)));
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

void NodePortIconRenderer::paintRail(
        Graphics& graphics,
        Rectangle<float> iconSpan,
        PortSide side) {
    const float referenceSize = side == PortSide::Left || side == PortSide::Right
            ? iconSpan.getWidth()
            : iconSpan.getHeight();
    const Rectangle<float> rail = iconSpan.expanded(referenceSize * 0.10f);
    paintAttachedRail(graphics, rail, side, referenceSize * 0.28f);
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
