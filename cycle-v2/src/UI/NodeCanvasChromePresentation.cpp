#include "UI/NodeCanvasPresentation.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
#include "UI/CanvasUtilityDock.h"
#include "UI/NodePaletteEntryIconRenderer.h"

namespace CycleV2 {

namespace {

Rectangle<float> graphBounds(const NodeGraph& graph) {
    Rectangle<float> bounds;
    for (const auto& node : graph.getNodes()) {
        bounds = bounds.isEmpty() ? node.bounds : bounds.getUnion(node.bounds);
    }

    return bounds.expanded(120.f);
}

}

void NodeCanvasPresentation::paintMiniMap(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    const Rectangle<float> map = CanvasUtilityDock::layout(frame.utilityBounds).minimap;
    graphics.setColour(CanvasChromePalette::minimapBackground);
    graphics.fillRoundedRectangle(map, CanvasChromeMetrics::panelCornerRadius);

    if (frame.graph.getNodes().empty()) {
        return;
    }

    const Rectangle<float> worldBounds = graphBounds(frame.graph);
    const float scale = jmin(
            map.getWidth() / worldBounds.getWidth(),
            map.getHeight() / worldBounds.getHeight());
    const Rectangle<float> projectedBounds(
            map.getCentreX() - worldBounds.getWidth() * scale * 0.5f,
            map.getCentreY() - worldBounds.getHeight() * scale * 0.5f,
            worldBounds.getWidth() * scale,
            worldBounds.getHeight() * scale);
    const auto project = [&](Rectangle<float> bounds) {
        return Rectangle<float>(
                projectedBounds.getX() + (bounds.getX() - worldBounds.getX()) * scale,
                projectedBounds.getY() + (bounds.getY() - worldBounds.getY()) * scale,
                bounds.getWidth() * scale,
                bounds.getHeight() * scale);
    };

    for (const auto& node : frame.graph.getNodes()) {
        graphics.setColour(CanvasChromePalette::minimapContent.withAlpha(0.40f));
        graphics.fillRoundedRectangle(
                project(node.bounds),
                CanvasChromeMetrics::microCornerRadius);
    }

    const Point<float> pan = frame.viewport.getPan();
    const float zoom = frame.viewport.getZoom();
    const Rectangle<float> viewportWorld(
            -pan.x / zoom,
            -pan.y / zoom,
            frame.canvasBounds.getWidth() / zoom,
            frame.canvasBounds.getHeight() / zoom);
    const Rectangle<float> viewportInMap = project(viewportWorld).getIntersection(projectedBounds);
    graphics.setColour(CanvasChromePalette::minimapViewport.withAlpha(0.10f));
    graphics.fillRoundedRectangle(viewportInMap, CanvasChromeMetrics::insetCornerRadius);
    graphics.setColour(CanvasChromePalette::minimapViewport.withAlpha(0.50f));
    graphics.drawRoundedRectangle(
            viewportInMap,
            CanvasChromeMetrics::insetCornerRadius,
            CanvasChromeMetrics::restingBorderWidth);

}

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
    const Rectangle<float> legend = CanvasUtilityDock::layout(frame.utilityBounds).legend;
    if (legend.isEmpty()) {
        return;
    }
    Graphics::ScopedSaveState scopedState(graphics);
    graphics.reduceClipRegion(legend.toNearestInt());
    graphics.setFont(FontOptions(CanvasChromeMetrics::legendFontSize));

    const Rectangle<float> content = legend.reduced(
            CanvasChromeMetrics::legendHorizontalInset, 0.f);
    const float cellWidth = content.getWidth() / (float) entryCount;
    const float y = legend.getCentreY();
    for (int index = 0; index < entryCount; ++index) {
        const auto& entry = entries[index];
        const float x = content.getX() + cellWidth * (float) index;
        Path line;
        line.startNewSubPath(x, y);
        line.lineTo(x + CanvasChromeMetrics::legendLineLength, y);
        graphics.setColour(colourForDomain(entry.domain).withAlpha(0.90f));

        graphics.strokePath(line, PathStrokeType(CanvasChromeMetrics::legendLineWidth));

        graphics.setColour(CanvasChromePalette::mutedText);
        graphics.drawText(
                entry.label,
                Rectangle<float>(
                        x + CanvasChromeMetrics::legendLineLength
                                + CanvasChromeMetrics::legendTextGap,
                        y - CanvasChromeMetrics::legendTextHeight * 0.5f,
                        cellWidth - CanvasChromeMetrics::legendLineLength
                                - CanvasChromeMetrics::legendTextGap,
                        CanvasChromeMetrics::legendTextHeight),
                Justification::centredLeft);
    }
}

String NodeCanvasPresentation::canvasStatusText(
        const String& statusMessage,
        const String& hoverText) {
    return hoverText.isNotEmpty() ? hoverText : statusMessage;
}

void NodeCanvasPresentation::paintStatus(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    const String text = canvasStatusText(frame.statusMessage, frame.hoverText);
    if (text.isEmpty()) {
        return;
    }

    const Rectangle<float> status = CanvasUtilityDock::layout(frame.utilityBounds).status;
    if (status.getWidth() < 180.f) {
        return;
    }

    const Rectangle<float> textBounds = status.reduced(2.f, 1.f);
    graphics.setFont(FontOptions(CanvasChromeMetrics::sectionTitleFontSize));
    graphics.setColour(CanvasChromePalette::text.withAlpha(0.8f));
    graphics.drawText(text, textBounds, Justification::centredLeft);
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
        graphics.setFont(FontOptions(14.f));
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
