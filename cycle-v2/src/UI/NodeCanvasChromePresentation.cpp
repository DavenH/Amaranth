#include "NodeCanvasPresentation.h"

#include "NodePaletteEntryIconRenderer.h"
#include "NodePaletteIconRenderer.h"

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };

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
    const Rectangle<float> map(
            frame.canvasBounds.getRight() - 180.f,
            frame.canvasBounds.getY() + 18.f,
            154.f,
            92.f);
    graphics.setColour(Colour(0xaa0b0e13));
    graphics.fillRoundedRectangle(map, 7.f);
    graphics.setColour(Colour(0xff354050));
    graphics.drawRoundedRectangle(map, 7.f, 1.f);

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
        graphics.setColour(Colour(0xff778596).withAlpha(0.62f));
        graphics.fillRoundedRectangle(project(node.bounds), 2.f);
    }

    const Point<float> pan = frame.viewport.getPan();
    const float zoom = frame.viewport.getZoom();
    const Rectangle<float> viewportWorld(
            -pan.x / zoom,
            -pan.y / zoom,
            frame.canvasBounds.getWidth() / zoom,
            frame.canvasBounds.getHeight() / zoom);
    const Rectangle<float> viewportInMap = project(viewportWorld).getIntersection(projectedBounds);
    graphics.setColour(Colour(0xff35d6d2).withAlpha(0.24f));
    graphics.fillRoundedRectangle(viewportInMap, 3.f);
    graphics.setColour(Colour(0xff35d6d2).withAlpha(0.85f));
    graphics.drawRoundedRectangle(viewportInMap, 3.f, 1.f);

    if (frame.statusMessage.isNotEmpty()) {
        const Rectangle<float> status(map.getX(), map.getBottom() + 8.f, map.getWidth(), 24.f);
        graphics.setColour(Colour(0xaa0b0e13));
        graphics.fillRoundedRectangle(status, 5.f);
        graphics.setColour(kMutedText);
        graphics.setFont(FontOptions(10.f));
        graphics.drawText(frame.statusMessage, status.reduced(8.f, 0.f), Justification::centredLeft);
    }
}

void NodeCanvasPresentation::paintLegend(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    struct LegendEntry {
        PortDomain domain;
        const char* label;
        bool attachment;
    };
    const LegendEntry entries[] = {
            { PortDomain::TimeSignal, "Time", false },
            { PortDomain::SpectralMagnitudeSignal, "Magnitude", false },
            { PortDomain::SpectralPhaseSignal, "Phase", false },
            { PortDomain::EnvelopeSignal, "Envelope", false },
            { PortDomain::EnvelopeSignal, "Attachment", true },
            { PortDomain::ControlSignal, "Universal", false }
    };
    const Rectangle<float> legend(
            frame.canvasBounds.getRight() - 134.f,
            frame.canvasBounds.getBottom() - 174.f,
            116.f,
            138.f);
    graphics.setColour(Colour(0xaa0b0e13));
    graphics.fillRoundedRectangle(legend, 5.f);
    graphics.setColour(Colour(0xff354050));
    graphics.drawRoundedRectangle(legend, 5.f, 1.f);
    graphics.setFont(FontOptions(9.f));

    float y = legend.getY() + 17.f;
    for (const auto& entry : entries) {
        const float x = legend.getX() + 12.f;
        Path line;
        line.startNewSubPath(x, y);
        line.lineTo(x + 17.f, y);
        graphics.setColour(colourForDomain(entry.domain).withAlpha(0.90f));

        if (entry.attachment) {
            Path dashed;
            PathStrokeType stroke(2.f, PathStrokeType::curved, PathStrokeType::rounded);
            Array<float> dashes { 5.f, 4.f };
            stroke.createDashedStroke(dashed, line, dashes.getRawDataPointer(), dashes.size());
            graphics.strokePath(dashed, stroke);
        } else {
            graphics.strokePath(line, PathStrokeType(2.f));
        }

        graphics.setColour(kMutedText);
        graphics.drawText(
                entry.label,
                Rectangle<float>(x + 24.f, y - 10.f, 76.f, 20.f),
                Justification::centredLeft);
        y += 20.f;
    }
}

void NodeCanvasPresentation::paintHoverConsole(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    if (frame.hoverText.isEmpty()) {
        return;
    }

    const Rectangle<float> console(
            frame.canvasBounds.getX() + 18.f,
            frame.canvasBounds.getBottom() - 42.f,
            jmin(560.f, frame.canvasBounds.getWidth() - 220.f),
            24.f);
    if (console.getWidth() < 180.f) {
        return;
    }

    const Rectangle<float> textBounds = console.reduced(10.f, 1.f);
    graphics.setFont(FontOptions(10.f));
    graphics.setColour(Colour(0xff0b0e13).withAlpha(0.76f));
    graphics.drawText(frame.hoverText, textBounds.translated(0.f, 1.f), Justification::centredLeft);
    graphics.setColour(kMutedText);
    graphics.drawText(frame.hoverText, textBounds, Justification::centredLeft);
}

void NodeCanvasPresentation::paintPalette(
        Graphics& graphics,
        const NodeCanvasPresentationFrame& frame) {
    const int activeSectionIndex = frame.palette.activeSection();
    for (int sectionIndex = 0; sectionIndex < frame.palette.sectionCount(); ++sectionIndex) {
        const auto& section = frame.palette.section(sectionIndex);
        const bool active = sectionIndex == activeSectionIndex;
        const Rectangle<float> button = frame.palette.groupBounds(sectionIndex);
        graphics.setColour(Colour(active ? 0xff1d2631 : 0xff151b24).withAlpha(active ? 0.94f : 0.82f));
        graphics.fillRoundedRectangle(button, 7.f);
        graphics.setColour(Colour(active ? 0xff8290a2 : 0xff435061).withAlpha(active ? 0.86f : 0.72f));
        graphics.drawRoundedRectangle(button, 7.f, active ? 1.6f : 1.f);

        Rectangle<float> content = button;
        const Rectangle<float> label = content.removeFromBottom(18.f);
        NodePaletteIconRenderer::paint(
                graphics,
                section.icon,
                content.reduced(8.f, 4.f),
                active);
        graphics.setFont(FontOptions(9.4f, Font::bold));
        graphics.setColour(kText.withAlpha(active ? 0.92f : 0.70f));
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
        graphics.setColour(Colour(hover ? 0xff202935 : 0xff161d26).withAlpha(hover ? 0.94f : 0.82f));
        graphics.fillRoundedRectangle(row, 6.f);
        graphics.setColour(Colour(0xff8290a2).withAlpha(hover ? 0.76f : 0.42f));
        graphics.drawRoundedRectangle(row, 6.f, hover ? 1.4f : 1.f);

        NodePaletteEntryIconRenderer::paint(
                graphics,
                entry.kind,
                Rectangle<float>(row.getX() + 7.f, row.getY() + 6.f, 34.f, row.getHeight() - 12.f),
                hover);

        graphics.setColour(kText.withAlpha(hover ? 0.96f : 0.82f));
        graphics.setFont(FontOptions(11.2f, Font::bold));
        graphics.drawText(
                String::fromUTF8(entry.label),
                row.withTrimmedLeft(48.f).reduced(0.f, 2.f),
                Justification::centredLeft);
    }
}

}
