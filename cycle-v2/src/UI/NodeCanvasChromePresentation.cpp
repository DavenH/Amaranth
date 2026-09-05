#include "UI/NodeCanvasPresentation.h"

#include "UI/CanvasChromeMetrics.h"
#include "UI/CanvasChromePalette.h"
#include "UI/CanvasUtilityDock.h"
#include "UI/NodePaletteEntryIconRenderer.h"
#include "UI/NodePaletteIconRenderer.h"

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
    const Rectangle<float> map = CanvasUtilityDock::layout(frame.canvasBounds).minimap;
    CanvasUtilityDock::paintSurface(graphics, map);

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
        graphics.setColour(CanvasChromePalette::strongBorder.withAlpha(0.62f));
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
    graphics.setColour(CanvasChromePalette::navigationAccent.withAlpha(0.24f));
    graphics.fillRoundedRectangle(viewportInMap, CanvasChromeMetrics::insetCornerRadius);
    graphics.setColour(CanvasChromePalette::navigationAccent.withAlpha(0.85f));
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
    const Rectangle<float> legend = CanvasUtilityDock::layout(frame.canvasBounds).legend;
    Graphics::ScopedSaveState scopedState(graphics);
    graphics.reduceClipRegion(legend.toNearestInt());
    CanvasUtilityDock::paintSurface(graphics, legend);
    graphics.setFont(FontOptions(CanvasChromeMetrics::legendFontSize));

    float y = legend.getY() + CanvasChromeMetrics::legendTopInset;
    for (const auto& entry : entries) {
        const float x = legend.getX() + CanvasChromeMetrics::legendHorizontalInset;
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
                        CanvasChromeMetrics::legendTextWidth,
                        CanvasChromeMetrics::legendTextHeight),
                Justification::centredLeft);
        y += CanvasChromeMetrics::legendRowStride;
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

    const Rectangle<float> status = CanvasUtilityDock::layout(frame.canvasBounds).status;
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
    const int activeSectionIndex = frame.palette.activeSection();
    for (int sectionIndex = 0; sectionIndex < frame.palette.sectionCount(); ++sectionIndex) {
        const auto& section = frame.palette.section(sectionIndex);
        const bool active = sectionIndex == activeSectionIndex;
        const Rectangle<float> button = frame.palette.groupBounds(sectionIndex);
        const auto colours = CanvasChromePalette::control(active
                ? CanvasChromeControlState::Selected
                : CanvasChromeControlState::Resting);
        graphics.setColour(colours.surface);
        graphics.fillRoundedRectangle(button, CanvasChromeMetrics::tileCornerRadius);
        graphics.setColour(colours.border);
        graphics.drawRoundedRectangle(
                button,
                CanvasChromeMetrics::tileCornerRadius,
                active
                        ? CanvasChromeMetrics::activeBorderWidth
                        : CanvasChromeMetrics::restingBorderWidth);

        Rectangle<float> content = button;
        const Rectangle<float> label = content.removeFromBottom(18.f);
        NodePaletteIconRenderer::paint(
                graphics,
                section.icon,
                content.reduced(8.f, 4.f),
                active);
        graphics.setFont(FontOptions(CanvasChromeMetrics::microFontSize));
        graphics.setColour(colours.text);
        graphics.drawText(section.shortLabel, label.reduced(3.f, 0.f), Justification::centred);
    }

    if (activeSectionIndex < 0) {
        return;
    }

    const auto& section = frame.palette.section(activeSectionIndex);
    for (int entryIndex = 0; entryIndex < section.entryCount; ++entryIndex) {
        const auto& entry = section.entries[entryIndex];
        const Rectangle<float> row = frame.palette.entryBounds(activeSectionIndex, entryIndex);
        const bool hover = row.contains(frame.pointer);
        const auto colours = CanvasChromePalette::control(hover
                ? CanvasChromeControlState::Hovered
                : CanvasChromeControlState::Resting);
        graphics.setColour(colours.surface);
        graphics.fillRoundedRectangle(row, CanvasChromeMetrics::controlCornerRadius);
        graphics.setColour(colours.border);
        graphics.drawRoundedRectangle(
                row,
                CanvasChromeMetrics::controlCornerRadius,
                hover
                        ? CanvasChromeMetrics::activeBorderWidth
                        : CanvasChromeMetrics::restingBorderWidth);

        NodePaletteEntryIconRenderer::paint(
                graphics,
                entry.kind,
                Rectangle<float>(row.getX() + 7.f, row.getY() + 6.f, 34.f, row.getHeight() - 12.f),
                hover);

        graphics.setColour(colours.text);
        graphics.setFont(FontOptions(CanvasChromeMetrics::labelFontSize));
        graphics.drawText(
                String::fromUTF8(entry.label),
                row.withTrimmedLeft(48.f).reduced(0.f, 2.f),
                Justification::centredLeft);
    }
}

}
