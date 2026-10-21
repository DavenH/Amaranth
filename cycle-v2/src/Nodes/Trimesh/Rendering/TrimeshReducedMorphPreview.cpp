#include <algorithm>

#include "Nodes/Trimesh/Rendering/TrimeshSidePanelRenderer.h"
#include "UI/Editors/PropertyControls.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"

namespace CycleV2 {

namespace {

using AxisStates = std::array<TrimeshSidePanelRenderer::AxisState, 3>;

float coordinate(const TrimeshCubePreviewVertex& vertex, int axis) {
    return axis == 0 ? vertex.time : axis == 1 ? vertex.red : vertex.blue;
}

Path polygon(const std::array<Point<float>, 4>& points) {
    Path path;
    path.startNewSubPath(points.front());
    for (size_t point = 1; point < points.size(); ++point) {
        path.lineTo(points[point]);
    }
    path.closeSubPath();
    return path;
}

void drawAxisLabel(Graphics& graphics, Rectangle<float> bounds, const TrimeshSidePanelRenderer::AxisState& axis) {
    graphics.setColour(axis.colour);
    graphics.setFont(FontOptions(9.f));
    graphics.drawText(axis.shortLabel, bounds, Justification::centred);
}

bool highlighted(const TrimeshCubePreviewVertex& vertex, const TrimeshCubePreviewVertex* selected,
        const AxisStates& axes, MorphDimensions dimensions) {
    if (selected == nullptr) {
        return false;
    }
    for (size_t axis = 0; axis < axes.size(); ++axis) {
        if (dimensions.active(MorphDimensions::axes[axis]) && !axes[axis].linked
                && coordinate(vertex, (int) axis) != coordinate(*selected, (int) axis)) {
            return false;
        }
    }
    return true;
}

void drawLinePreview(Graphics& graphics, Rectangle<float> area, const AxisStates& axes,
        const std::vector<TrimeshCubePreviewVertex>& vertices, const TrimeshCubePreviewVertex* selected) {
    const Line<float> line(area.getX(), area.getCentreY(), area.getRight(), area.getCentreY());
    graphics.setColour(axes[0].colour.withAlpha(0.3f));
    graphics.drawLine(line, 1.f);
    graphics.setColour(axes[0].colour.withAlpha(0.8f));
    graphics.fillRect(Rectangle<float>(line.getStartX() - 7.f, line.getStartY() + 7.f,
            line.getLength() + 14.f, 3.f));
    const auto endpoint = [&](int index) {
        return vertices.size() >= 8 ? line.getPointAlongLineProportionally(vertices[(size_t) index].time)
                : index == 0 ? line.getStart() : line.getEnd();
    };
    graphics.setColour(selected != nullptr && axes[0].linked ? Colour(0xffed7c7c) : Colours::white);
    graphics.drawLine(Line<float>(endpoint(0), endpoint(4)), 1.2f);
    for (const int corner : { 0, 4 }) {
        paintMorphPreviewVertex(graphics, endpoint(corner), vertices.size() >= 8
                && highlighted(vertices[(size_t) corner], selected, axes, MorphDimensions(1)));
    }
    const auto cursor = line.getPointAlongLineProportionally(jlimit(0.f, 1.f, axes[0].value));
    graphics.setColour(Colours::white.withAlpha(0.7f));
    graphics.drawVerticalLine(roundToInt(cursor.x), cursor.y - 7.f, cursor.y + 11.f);
    paintMorphPreviewCursor(graphics, cursor);
    drawAxisLabel(graphics, { area.getRight() + 8.f, line.getStartY() + 3.f, 10.f, 12.f }, axes[0]);
}

void drawPlanePreview(Graphics& graphics, Rectangle<float> area, const AxisStates& axes,
        MorphDimensions dimensions, const std::vector<TrimeshCubePreviewVertex>& vertices,
        const TrimeshCubePreviewVertex* selected) {
    std::array<int, 2> active {};
    int ordinal = 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (dimensions.active(MorphDimensions::axes[(size_t) axis])) {
            active[(size_t) ordinal++] = axis;
        }
    }
    const auto origin = area.getBottomLeft();
    const auto project = [&](float u, float v) {
        return Point<float>(area.getX() + area.getWidth() * u, area.getBottom() - area.getHeight() * v);
    };
    const std::array<Point<float>, 4> corners { project(0.f, 0.f), project(1.f, 0.f),
            project(1.f, 1.f), project(0.f, 1.f) };
    const auto plane = polygon(corners);
    graphics.setColour(Colour(0xff232830));
    graphics.fillPath(plane);
    for (int axis = 0; axis < 2; ++axis) {
        const auto colour = axes[(size_t) active[(size_t) axis]].colour;
        graphics.setGradientFill(ColourGradient(colour.withAlpha(0.03f), origin,
                colour.withAlpha(0.48f), axis == 0 ? corners[1] : corners[3], false));
        graphics.fillPath(plane);
        graphics.setColour(colour.withAlpha(0.8f));
        graphics.fillRect(axis == 0
                ? Rectangle<float>(area.getX() - 9.f, area.getBottom() + 6.f, area.getWidth() + 15.f, 3.f)
                : Rectangle<float>(area.getX() - 9.f, area.getY() - 6.f, 3.f, area.getHeight() + 15.f));
    }
    std::array<Point<float>, 4> authored = corners;
    const int first = MorphDimensions::poleBits[(size_t) active[0]];
    const int second = MorphDimensions::poleBits[(size_t) active[1]];
    const std::array<int, 4> indices { 0, first, first | second, second };
    if (vertices.size() >= 8) {
        for (size_t point = 0; point < authored.size(); ++point) {
            const auto& vertex = vertices[(size_t) indices[point]];
            authored[point] = project(coordinate(vertex, active[0]), coordinate(vertex, active[1]));
        }
        graphics.setColour(Colours::white.withAlpha(0.12f));
        graphics.fillPath(polygon(authored));
        graphics.setColour(Colours::white.withAlpha(0.26f));
        graphics.strokePath(polygon(authored), PathStrokeType(0.8f));
    }
    const auto cursor = project(jlimit(0.f, 1.f, axes[(size_t) active[0]].value),
            jlimit(0.f, 1.f, axes[(size_t) active[1]].value));
    graphics.setColour(axes[(size_t) active[0]].colour.withAlpha(0.55f));
    graphics.drawLine(cursor.x, area.getBottom() + 10.f, cursor.x, cursor.y, 1.f);
    graphics.setColour(axes[(size_t) active[1]].colour.withAlpha(0.55f));
    graphics.drawLine(area.getX() - 10.f, cursor.y, cursor.x, cursor.y, 1.f);
    for (size_t point = 0; point < authored.size(); ++point) {
        paintMorphPreviewVertex(graphics, authored[point], vertices.size() >= 8
                && highlighted(vertices[(size_t) indices[point]], selected, axes, dimensions));
    }
    paintMorphPreviewCursor(graphics, cursor);
    drawAxisLabel(graphics, { area.getRight() + 8.f, area.getBottom() + 1.f, 10.f, 12.f }, axes[(size_t) active[0]]);
    drawAxisLabel(graphics, { area.getX() - 24.f, area.getY() - 12.f, 10.f, 12.f }, axes[(size_t) active[1]]);
}

}

void TrimeshSidePanelRenderer::drawReducedMorphPreview(
        Graphics& graphics, Rectangle<float> area, const AxisStates& axes,
        MorphDimensions dimensions, const std::vector<TrimeshCubePreviewVertex>& vertices) {
    if (area.getWidth() < 42.f || area.getHeight() < 42.f) {
        return;
    }
    paintPropertyGroupLabel(graphics, area.reduced(8.f, 5.f).removeFromTop(18.f),
            "Morph space");
    area = area.withTrimmedTop(22.f).reduced(12.f, 10.f);
    const float side = jmin(area.getWidth(), area.getHeight());
    area = area.withSizeKeepingCentre(side, side);
    const auto selected = std::find_if(vertices.begin(), vertices.end(),
            [](const auto& vertex) { return vertex.selected; });
    const auto* selection = selected != vertices.end() ? &*selected : nullptr;
    if (dimensions.axisCount() == 1) {
        drawLinePreview(graphics, area, axes, vertices, selection);
    } else {
        drawPlanePreview(graphics, area, axes, dimensions, vertices, selection);
    }
}

}
