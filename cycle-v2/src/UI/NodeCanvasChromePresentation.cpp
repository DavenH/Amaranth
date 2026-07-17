#include "NodeCanvasPresentation.h"

#include "../Graph/GraphNodeFactory.h"

#include <cstring>

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };

float fastSin(float value) {
    return (float) dsp::FastMathApproximations::sin((double) value);
}

void paintPaletteGroupIcon(
        Graphics& graphics,
        const char* title,
        Rectangle<float> area,
        bool active) {
    const float stroke = active ? 2.1f : 1.65f;
    const Colour fg = Colour(0xffd2d9e2).withAlpha(active ? 0.96f : 0.76f);
    const Colour dim = kMutedText.withAlpha(active ? 0.68f : 0.44f);
    area = area.reduced(4.f);

    graphics.setColour(fg);

    if (std::strcmp(title, "Context") == 0) {
        const float y = area.getCentreY();
        const float x0 = area.getX() + area.getWidth() * 0.18f;
        const float x1 = area.getRight() - area.getWidth() * 0.18f;
        graphics.drawLine(x0, y, x1, y, stroke);

        for (int i = 0; i < 4; ++i) {
            const float t = (float) i / 3.f;
            const float x = x0 + (x1 - x0) * t;
            Rectangle<float> dot(x - 2.2f, y - 2.2f, 4.4f, 4.4f);
            graphics.fillEllipse(dot);
        }

        graphics.setColour(dim);
        graphics.drawEllipse(area.withSizeKeepingCentre(23.f, 23.f), 1.1f);
        return;
    }

    if (std::strcmp(title, "Transform") == 0) {
        Path square;
        const Rectangle<float> left(
                area.getX(),
                area.getY() + area.getHeight() * 0.22f,
                area.getWidth() * 0.34f,
                area.getHeight() * 0.56f);
        const Rectangle<float> right(
                area.getX() + area.getWidth() * 0.40f,
                area.getY() + area.getHeight() * 0.14f,
                area.getWidth() * 0.60f,
                area.getHeight() * 0.72f);
        square.startNewSubPath(left.getX(), left.getBottom());
        square.lineTo(left.getX(), left.getY());
        square.lineTo(left.getCentreX(), left.getY());
        square.lineTo(left.getCentreX(), left.getBottom());
        square.lineTo(left.getRight(), left.getBottom());
        graphics.strokePath(square, PathStrokeType(stroke, PathStrokeType::mitered, PathStrokeType::rounded));

        Path wave;
        for (int i = 0; i < 32; ++i) {
            const float t = (float) i / 31.f;
            const Point<float> p(
                    right.getX() + t * right.getWidth(),
                    right.getCentreY() + fastSin(t * MathConstants<float>::twoPi * 1.35f) * right.getHeight() * 0.30f);
            if (i == 0) {
                wave.startNewSubPath(p);
            } else {
                wave.lineTo(p);
            }
        }

        graphics.setColour(dim);
        graphics.drawLine(left.getRight() + 3.f, left.getCentreY(), right.getX() - 3.f, right.getCentreY(), 1.1f);
        graphics.setColour(fg);
        graphics.strokePath(wave, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::rounded));
        return;
    }

    if (std::strcmp(title, "Math") == 0) {
        const Point<float> left(area.getX() + area.getWidth() * 0.34f, area.getCentreY());
        const Point<float> right(area.getX() + area.getWidth() * 0.68f, area.getCentreY());
        const float r = 6.f;
        graphics.drawLine(left.x - r, left.y, left.x + r, left.y, stroke);
        graphics.drawLine(left.x, left.y - r, left.x, left.y + r, stroke);
        graphics.drawLine(right.x - r, right.y - r, right.x + r, right.y + r, stroke);
        graphics.drawLine(right.x + r, right.y - r, right.x - r, right.y + r, stroke);
        return;
    }

    if (std::strcmp(title, "Source") == 0) {
        Path wave;
        Rectangle<float> waveArea = area.withTrimmedBottom(area.getHeight() * 0.45f);
        for (int i = 0; i < 20; ++i) {
            const float t = (float) i / 19.f;
            const Point<float> p(
                    waveArea.getX() + t * waveArea.getWidth(),
                    waveArea.getCentreY()
                            + fastSin(t * MathConstants<float>::twoPi * 1.2f)
                                    * waveArea.getHeight() * 0.28f);
            if (i == 0) {
                wave.startNewSubPath(p);
            } else {
                wave.lineTo(p);
            }
        }

        graphics.strokePath(wave, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::rounded));
        graphics.setColour(dim);
        Rectangle<float> image(area.getX() + 5.f, area.getCentreY() + 3.f, 10.f, 8.f);
        Rectangle<float> mesh(area.getRight() - 17.f, area.getCentreY() + 2.f, 12.f, 10.f);
        graphics.drawRect(image, 1.f);
        graphics.drawRoundedRectangle(mesh, 2.f, 1.f);
        return;
    }

    if (std::strcmp(title, "Control") == 0) {
        Rectangle<float> graph = area.reduced(3.f, 6.f);
        Path env;
        env.startNewSubPath(graph.getX(), graph.getBottom());
        env.lineTo(graph.getX() + graph.getWidth() * 0.22f, graph.getY());
        env.lineTo(graph.getX() + graph.getWidth() * 0.58f, graph.getY());
        env.lineTo(graph.getRight(), graph.getBottom());
        graphics.strokePath(env, PathStrokeType(stroke, PathStrokeType::curved, PathStrokeType::rounded));
        graphics.setColour(dim);
        graphics.drawLine(graph.getX(), graph.getBottom(), graph.getRight(), graph.getBottom(), 1.f);
        return;
    }

    if (std::strcmp(title, "FX") == 0) {
        Rectangle<float> arcArea = area.reduced(3.f);
        for (int i = 0; i < 3; ++i) {
            const float inset = (float) i * 5.f;
            graphics.setColour(fg.withAlpha(active ? 0.88f - (float) i * 0.18f : 0.62f - (float) i * 0.13f));
            graphics.drawEllipse(arcArea.reduced(inset), jmax(0.8f, stroke - (float) i * 0.25f));
        }

        graphics.setColour(dim);
        graphics.drawLine(area.getX() + 5.f, area.getCentreY(), area.getRight() - 5.f, area.getCentreY(), 1.f);
        return;
    }

    if (std::strcmp(title, "Channel") == 0) {
        const float left = area.getX() + 5.f;
        const float centre = area.getCentreX();
        const float right = area.getRight() - 5.f;
        const float y = area.getCentreY();
        graphics.drawLine(left, y, centre, y, stroke);
        graphics.drawLine(centre, y, right, y - 7.f, stroke);
        graphics.drawLine(centre, y, right, y + 7.f, stroke);
        graphics.setColour(dim);
        graphics.drawLine(left, y - 8.f, centre, y - 8.f, 1.f);
        graphics.drawLine(left, y + 8.f, centre, y + 8.f, 1.f);
    }
}

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
        paintPaletteGroupIcon(graphics, section.title, content.reduced(8.f, 4.f), active);
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

        Node previewNode = GraphNodeFactory().createNode(entry.kind, {}, {});
        const PortDomain domain = previewNode.outputs.empty()
                ? PortDomain::ControlSignal
                : previewNode.outputs.front().domain;
        previewRenderer.paint(graphics, {
                previewNode,
                nullptr,
                Rectangle<float>(row.getX() + 7.f, row.getY() + 8.f, 39.f, row.getHeight() - 16.f),
                TrimeshRenderProfile::fromDomain(domain),
                frame.viewport.getZoom(),
                false
        });

        graphics.setColour(kText.withAlpha(hover ? 0.96f : 0.82f));
        graphics.setFont(FontOptions(11.2f, Font::bold));
        graphics.drawText(
                String::fromUTF8(entry.label),
                row.withTrimmedLeft(53.f).reduced(0.f, 2.f),
                Justification::centredLeft);
    }
}

}
