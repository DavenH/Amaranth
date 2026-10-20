#include <algorithm>

#include "Nodes/Trimesh/Rendering/TrimeshSidePanelRenderer.h"

#include "UI/Editors/PropertyControls.h"

namespace CycleV2 {

void TrimeshSidePanelRenderer::drawMorphPlanePreview(
        Graphics& graphics, Rectangle<float> area,
        const std::array<AxisState, 3>& axes, MorphDimensions dimensions,
        const std::vector<TrimeshCubePreviewVertex>& vertices) {
    if (area.getWidth() < 42.f || area.getHeight() < 42.f) {
        return;
    }
    paintPropertyGroupLabel(graphics, area.reduced(8.f, 5.f).removeFromTop(18.f), "Plane display");
    area = area.withTrimmedTop(22.f).reduced(7.f, 5.f);
    std::array<int, 2> active {};
    int ordinal = 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (dimensions.active(MorphDimensions::axes[(size_t) axis])) {
            active[(size_t) ordinal++] = axis;
        }
    }
    const auto origin = Point<float>(area.getX(), area.getCentreY());
    const auto first = Point<float>(area.getWidth() * 0.5f, area.getHeight() * 0.5f);
    const auto second = Point<float>(area.getWidth() * 0.5f, -area.getHeight() * 0.5f);
    const auto project = [&](float u, float v) { return origin + first * u + second * v; };
    const std::array<Point<float>, 4> corners { project(0.f, 0.f), project(1.f, 0.f),
            project(1.f, 1.f), project(0.f, 1.f) };
    const auto polygon = [](const std::array<Point<float>, 4>& points) {
        Path path;
        path.startNewSubPath(points[0]);
        for (int point = 1; point < 4; ++point) {
            path.lineTo(points[(size_t) point]);
        }
        path.closeSubPath();
        return path;
    };
    const auto plane = polygon(corners);
    graphics.setColour(Colour(0xff232830));
    graphics.fillPath(plane);
    for (int axis = 0; axis < 2; ++axis) {
        const auto colour = axes[(size_t) active[(size_t) axis]].colour;
        const auto end = axis == 0 ? corners[1] : corners[3];
        graphics.setGradientFill(ColourGradient(colour.withAlpha(0.03f), origin,
                colour.withAlpha(0.48f), end, false));
        graphics.fillPath(plane);
        graphics.setColour(colour.withAlpha(0.72f));
        graphics.drawLine(Line<float>(origin.translated(-4.f, 0.f), end.translated(-4.f, 0.f)), 2.6f);
    }
    const auto coordinate = [](const TrimeshCubePreviewVertex& vertex, int axis) {
        return axis == 0 ? vertex.time : axis == 1 ? vertex.red : vertex.blue;
    };
    const int firstBit = MorphDimensions::poleBits[(size_t) active[0]];
    const int secondBit = MorphDimensions::poleBits[(size_t) active[1]];
    const std::array<int, 4> indices { 0, firstBit, firstBit | secondBit, secondBit };
    std::array<Point<float>, 4> authored = corners;
    const bool hasGeometry = vertices.size() >= 8;
    const auto selected = std::find_if(vertices.begin(), vertices.end(),
            [](const auto& vertex) { return vertex.selected; });
    if (hasGeometry) {
        for (int point = 0; point < 4; ++point) {
            const auto& vertex = vertices[(size_t) indices[(size_t) point]];
            authored[(size_t) point] = project(coordinate(vertex, active[0]), coordinate(vertex, active[1]));
        }
        graphics.setColour(Colours::white.withAlpha(0.12f));
        graphics.fillPath(polygon(authored));
        graphics.setColour(Colours::white.withAlpha(0.26f));
        graphics.strokePath(polygon(authored), PathStrokeType(0.8f));
    }
    const float u = jlimit(0.f, 1.f, axes[(size_t) active[0]].value);
    const float v = jlimit(0.f, 1.f, axes[(size_t) active[1]].value);
    const auto cursor = project(u, v);
    graphics.setColour(axes[(size_t) active[0]].colour.withAlpha(0.45f));
    graphics.drawLine(Line<float>(project(0.f, v), cursor), 1.f);
    graphics.setColour(axes[(size_t) active[1]].colour.withAlpha(0.45f));
    graphics.drawLine(Line<float>(project(u, 0.f), cursor), 1.f);
    for (int point = 0; point < 4; ++point) {
        bool highlighted = hasGeometry && selected != vertices.end();
        for (const int axis : active) {
            highlighted &= axes[(size_t) axis].linked || (hasGeometry && selected != vertices.end()
                    && coordinate(vertices[(size_t) indices[(size_t) point]], axis) == coordinate(*selected, axis));
        }
        graphics.setColour(Colours::black);
        graphics.fillEllipse(Rectangle<float>(4.5f, 4.5f).withCentre(authored[(size_t) point]));
        graphics.setColour(highlighted ? Colour(0xffed7c7c) : Colours::white);
        graphics.fillEllipse(Rectangle<float>(1.8f, 1.8f).withCentre(authored[(size_t) point]));
    }
    graphics.setColour(Colours::black.withAlpha(0.7f));
    graphics.fillEllipse(Rectangle<float>(5.f, 5.f).withCentre(cursor));
    graphics.setColour(Colours::white);
    graphics.fillEllipse(Rectangle<float>(2.f, 2.f).withCentre(cursor));
}

}
