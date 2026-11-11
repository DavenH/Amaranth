#include "UI/NodeCanvasPresentation.h"
#include "UI/ExplanationEditor.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
#include "UI/CanvasUtilityDock.h"
#include "UI/NodePaletteEntryIconRenderer.h"

namespace CycleV2 {

void NodeCanvasPresentation::paintLegend(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    struct LegendEntry {
        PortDomain domain;
        const char* label;
    };
    const LegendEntry entries[] = {
            { PortDomain::TimeSignal, "Time" },
            { PortDomain::SpectralMagnitudeSignal, "Magnitude" },
            { PortDomain::SpectralPhaseSignal, "Phase" },
            { PortDomain::ControlSignal, "Control" }
    };
    constexpr int entryCount = 4;
    const Rectangle<float> legend = CanvasUtilityDock::layout(
            frame.utilityBounds).legend;
    if (legend.isEmpty()) {
        return;
    }
    Graphics::ScopedSaveState scopedState(graphics);
    graphics.reduceClipRegion(legend.toNearestInt());
    graphics.setFont(FontOptions(CanvasChromeMetrics::legendFontSize));

    const Font font { FontOptions(CanvasChromeMetrics::legendFontSize) };
    float totalWidth = 0.f;
    for (const auto& entry : entries) {
        totalWidth += CanvasChromeMetrics::legendLineLength + CanvasChromeMetrics::legendTextGap
                + font.getStringWidthFloat(entry.label);
    }
    constexpr float entryGap = 16.f;
    totalWidth += entryGap * (entryCount - 1);
    float x = legend.getRight() - totalWidth;
    const float y = legend.getCentreY();
    for (const auto& entry : entries) {
        graphics.setColour(colourForDomain(entry.domain).withAlpha(0.90f));
        graphics.drawLine(x, y, x + CanvasChromeMetrics::legendLineLength, y,
                CanvasChromeMetrics::legendLineWidth);
        x += CanvasChromeMetrics::legendLineLength + CanvasChromeMetrics::legendTextGap;
        const float width = font.getStringWidthFloat(entry.label);
        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.drawText(entry.label, Rectangle<float>(x, legend.getY(), width, legend.getHeight()),
                Justification::centredLeft);
        x += width + entryGap;
    }
}

String NodeCanvasPresentation::canvasStatusText(
        const String& statusMessage,
        const String& hoverText) {
    return (hoverText.isNotEmpty() ? hoverText : statusMessage)
            .replaceCharacters("\r\n\t", "   ").trim();
}

String NodeCanvasPresentation::controlHelpText(Component* hovered, const Component& scope) {
    if (hovered == nullptr || (&scope != hovered && !scope.isParentOf(hovered))
            || hovered->isCurrentlyBlockedByAnotherModalComponent()) {
        return {};
    }
    for (auto* component = hovered; component != nullptr; component = component->getParentComponent()) {
        if (auto* client = dynamic_cast<TooltipClient*>(component)) {
            const String text = client->getTooltip();
            if (text.isNotEmpty()) {
                return text;
            }
        }
        if (component == &scope) {
            break;
        }
    }
    return {};
}

void NodeCanvasPresentation::paintStatus(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    const auto status = CanvasUtilityDock::layout(frame.utilityBounds).status;
    graphics.setColour(CanvasChromePalette::dockSurface);
    graphics.fillRect(status.toNearestInt());
    const auto textBounds = status.withTrimmedLeft((float) ExplanationBadge::presetButtonWidth + 8.f);
    graphics.setFont(FontOptions(CanvasChromeMetrics::sectionTitleFontSize));
    graphics.setColour(CanvasChromePalette::text.withAlpha(0.8f));
    graphics.drawText(canvasStatusText(frame.statusMessage, frame.hoverText), textBounds,
            Justification::centredLeft, true);
}

void NodeCanvasPresentation::paintPalette(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    if (!frame.palette.isVisible()) {
        return;
    }
    graphics.setColour(CanvasChromePalette::dockSurface);
    graphics.fillRect(frame.palette.workspaceBounds());
    const int activeSectionIndex = frame.palette.activeSection();
    const int hoveredEntryIndex = frame.palette.activeEntry();

    const float physicalScale = graphics.getInternalContext().getPhysicalPixelScaleFactor();
    const Rectangle<float> bounds = frame.palette.railBounds();
    const int imageWidth = jmax(1, roundToInt(bounds.getWidth() * physicalScale));
    const int imageHeight = jmax(1, roundToInt(bounds.getHeight() * physicalScale));
    const bool cacheHit = paletteCacheImage.isValid()
            && paletteCacheImage.getWidth() == imageWidth
            && paletteCacheImage.getHeight() == imageHeight
            && paletteCacheBounds == bounds
            && paletteCacheScale == physicalScale
            && paletteCacheActiveSection == activeSectionIndex
            && paletteCacheHoveredEntry == hoveredEntryIndex;
    if (!cacheHit) {
        paletteCacheImage = Image(Image::ARGB, imageWidth, imageHeight, true);
        paletteCacheBounds = bounds;
        paletteCacheScale = physicalScale;
        paletteCacheActiveSection = activeSectionIndex;
        paletteCacheHoveredEntry = hoveredEntryIndex;

        Graphics imageGraphics(paletteCacheImage);
        imageGraphics.addTransform(AffineTransform(
                physicalScale,
                0.f,
                -bounds.getX() * physicalScale,
                0.f,
                physicalScale,
                -bounds.getY() * physicalScale));
        paintPaletteContent(imageGraphics, frame);
    }

    const float imageToLogicalX = paletteCacheBounds.getWidth()
            / (float) paletteCacheImage.getWidth();
    const float imageToLogicalY = paletteCacheBounds.getHeight()
            / (float) paletteCacheImage.getHeight();
    graphics.drawImageTransformed(
            paletteCacheImage,
            AffineTransform(
                    imageToLogicalX,
                    0.f,
                    paletteCacheBounds.getX(),
                    0.f,
                    imageToLogicalY,
                    paletteCacheBounds.getY()),
            false);
}

void NodeCanvasPresentation::paintPaletteContent(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    for (int sectionIndex = 0; sectionIndex < frame.palette.sectionCount(); ++sectionIndex) {
        const auto& section = frame.palette.section(sectionIndex);
        const auto group = frame.palette.groupBounds(sectionIndex);
        graphics.setColour(Colour(section.accentColour));
        graphics.fillRoundedRectangle(
                group.withX(group.getX() - 10.f).withWidth(3.f), 1.5f);
        graphics.setFont(FontOptions(14.f).withStyle("Bold"));
        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.drawText(section.title, group.withHeight(20.f), Justification::centredLeft);

        for (int entryIndex = 0; entryIndex < section.entryCount; ++entryIndex) {
            const auto& entry = section.entries[entryIndex];
            const auto tile = frame.palette.entryBounds(sectionIndex, entryIndex);
            const bool hover = sectionIndex == frame.palette.activeSection()
                    && entryIndex == frame.palette.activeEntry();
            const auto colours = CanvasChromePalette::control(hover
                    ? CanvasChromeControlState::Hovered
                    : CanvasChromeControlState::Resting);
            graphics.setColour(colours.surface);
            graphics.fillRoundedRectangle(tile, CanvasChromeMetrics::controlCornerRadius);
            graphics.setColour(colours.border);
            graphics.drawRoundedRectangle(tile, CanvasChromeMetrics::controlCornerRadius,
                    hover ? CanvasChromeMetrics::activeBorderWidth : CanvasChromeMetrics::restingBorderWidth);

            const float iconSize = jmin(41.6f, tile.getHeight() - 20.8f);
            const Rectangle<float> icon(tile.getCentreX() - iconSize * 0.5f,
                    tile.getY() + 4.f, iconSize, iconSize);
            NodePaletteEntryIconRenderer::paint(graphics, entry.kind, icon, hover);
            graphics.setFont(FontOptions(10.5f));
            graphics.setColour(CanvasChromePalette::text);
            graphics.drawFittedText(String::fromUTF8(entry.label),
                    tile.withTop(tile.getBottom() - 16.f).reduced(3.f, 0.f).toNearestInt(),
                    Justification::centred, 1, 0.8f);
        }
    }
}

}
