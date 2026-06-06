#include "TrimeshSidePanelRenderer.h"

namespace CycleV2 {

namespace {

const Colour kText      { 0xffe2e8ef };
const Colour kMutedText { 0xff8793a1 };

Point<float> projectCubePoint(
        Rectangle<float> area,
        const std::array<float, 3>& values,
        float size) {
    const Point<float> centre = area.getCentre();
    const Point<float> left(centre.x - size, centre.y - size * 0.28f);
    const Point<float> right(centre.x + size, centre.y - size * 0.28f);
    const Point<float> back(centre.x, centre.y + size * 0.98f);
    const Point<float> top(centre.x, centre.y - size * 0.86f);
    const Point<float> base(
            left.x + (right.x - left.x) * jlimit(0.f, 1.f, values[0]),
            back.y + (top.y - back.y) * jlimit(0.f, 1.f, values[2]));

    return base.translated(
            (jlimit(0.f, 1.f, values[1]) - 0.5f) * size * 0.72f,
            -(jlimit(0.f, 1.f, values[1]) - 0.5f) * size * 0.34f);
}

}

void TrimeshSidePanelRenderer::drawMorphCubePreview(
        Graphics& g,
        Rectangle<float> area,
        const std::array<float, 3>& values,
        int selectedVertexIndex) {
    if (area.getWidth() < 42.f || area.getHeight() < 42.f) {
        return;
    }

    area = area.reduced(3.f);
    g.setColour(Colour(0xff05070a).withAlpha(0.42f));
    g.fillRoundedRectangle(area, 4.f);

    const Point<float> centre = area.getCentre();
    const float size = jmin(area.getWidth(), area.getHeight()) * 0.30f;
    const Point<float> top(centre.x, centre.y - size * 0.86f);
    const Point<float> left(centre.x - size, centre.y - size * 0.28f);
    const Point<float> right(centre.x + size, centre.y - size * 0.28f);
    const Point<float> bottom(centre.x, centre.y + size * 0.34f);
    const Point<float> back(centre.x, centre.y + size * 0.98f);

    Path leftFace;
    leftFace.startNewSubPath(top);
    leftFace.lineTo(left);
    leftFace.lineTo(bottom);
    leftFace.lineTo(centre);
    leftFace.closeSubPath();

    Path rightFace;
    rightFace.startNewSubPath(top);
    rightFace.lineTo(right);
    rightFace.lineTo(bottom);
    rightFace.lineTo(centre);
    rightFace.closeSubPath();

    Path lowerFace;
    lowerFace.startNewSubPath(left);
    lowerFace.lineTo(bottom);
    lowerFace.lineTo(back);
    lowerFace.lineTo(centre.x - size * 0.12f, centre.y + size * 0.08f);
    lowerFace.closeSubPath();

    g.setColour(Colour(0xff7e8794).withAlpha(0.16f));
    g.fillPath(leftFace);
    g.setColour(Colour(0xffd8e0ea).withAlpha(0.11f));
    g.fillPath(rightFace);
    g.setColour(Colour(0xff42505f).withAlpha(0.15f));
    g.fillPath(lowerFace);
    g.setColour(Colour(0xffbac5d0).withAlpha(0.26f));
    g.strokePath(leftFace, PathStrokeType(1.f));
    g.strokePath(rightFace, PathStrokeType(1.f));
    g.strokePath(lowerFace, PathStrokeType(1.f));

    const Point<float> yellowAxis(left.x, back.y);
    const Point<float> redAxis(right.x, back.y);
    const Point<float> blueAxis(centre.x, top.y);
    const Point<float> point(
            yellowAxis.x + (redAxis.x - yellowAxis.x) * jlimit(0.f, 1.f, values[0]),
            back.y + (blueAxis.y - back.y) * jlimit(0.f, 1.f, values[2]));
    const Point<float> lifted = point.translated(
            (jlimit(0.f, 1.f, values[1]) - 0.5f) * size * 0.72f,
            -(jlimit(0.f, 1.f, values[1]) - 0.5f) * size * 0.34f);

    g.setColour(Colour(0xffe0c247).withAlpha(0.68f));
    g.drawLine(yellowAxis.x, yellowAxis.y, redAxis.x, redAxis.y, 1.4f);
    g.setColour(Colour(0xffd65a5a).withAlpha(0.68f));
    g.drawLine(left.x, left.y, right.x, right.y, 1.2f);
    g.setColour(Colour(0xff5f91e8).withAlpha(0.70f));
    g.drawLine(centre.x, back.y, centre.x, top.y, 1.2f);

    for (int i = 0; i < 8; ++i) {
        const std::array<float, 3> corner {
                (i & 1) != 0 ? 1.f : 0.f,
                (i & 2) != 0 ? 1.f : 0.f,
                (i & 4) != 0 ? 1.f : 0.f
        };
        const Point<float> cornerPoint = projectCubePoint(area, corner, size);
        const bool selected = selectedVertexIndex == i;
        const float radius = selected ? 4.2f : 2.4f;

        g.setColour(Colour(0xff05070a).withAlpha(selected ? 0.88f : 0.74f));
        g.fillEllipse(Rectangle<float>((radius + 1.4f) * 2.f, (radius + 1.4f) * 2.f).withCentre(cornerPoint));
        g.setColour(selected ? Colour(0xffffd13d).withAlpha(0.96f) : Colour(0xffdce6ee).withAlpha(0.66f));
        g.fillEllipse(Rectangle<float>(radius * 2.f, radius * 2.f).withCentre(cornerPoint));
    }

    g.setColour(Colour(0xffffd13d).withAlpha(0.94f));
    g.fillEllipse(Rectangle<float>(7.f, 7.f).withCentre(lifted));
    g.setColour(Colour(0xff05070a).withAlpha(0.72f));
    g.drawEllipse(Rectangle<float>(10.f, 10.f).withCentre(lifted), 1.2f);
}

void TrimeshSidePanelRenderer::drawVertexParameters(
        Graphics& g,
        Rectangle<float> area,
        const std::vector<TrimeshVertexParameter>& parameters) {
    if (area.getHeight() < 28.f) {
        return;
    }

    area = area.withHeight(jmin(area.getHeight(), 140.f));
    g.setColour(Colour(0xff0a0f13).withAlpha(0.58f));
    g.fillRoundedRectangle(area, 5.f);
    g.setColour(kText);
    g.setFont(FontOptions(11.f, Font::bold));
    g.drawText("Selected Vertex", area.reduced(10.f, 8.f).removeFromTop(15.f), Justification::centredLeft);

    for (int i = 0; i < (int) parameters.size(); ++i) {
        const auto& parameter = parameters[(size_t) i];
        const auto row = vertexParameterRowBounds(area, i);
        auto labelBox = row;
        auto valueBox = row;
        const auto rail = vertexParameterRailBounds(row);

        g.setColour(kMutedText);
        g.setFont(FontOptions(10.f, Font::bold));
        g.drawText(parameter.label, labelBox.removeFromLeft(70.f), Justification::centredLeft);

        const float range = parameter.maximum - parameter.minimum;
        const float normalized = range > 0.f
                ? jlimit(0.f, 1.f, (parameter.value - parameter.minimum) / range)
                : 0.f;
        valueBox = valueBox.removeFromRight(44.f);

        g.setColour(Colour(0xff273342).withAlpha(0.84f));
        g.fillRoundedRectangle(rail, 2.5f);
        g.setColour(Colour(0xffc5d1dc).withAlpha(0.86f));
        g.fillRoundedRectangle(rail.withWidth(rail.getWidth() * normalized), 2.5f);

        g.setColour(kText.withAlpha(0.88f));
        g.setFont(FontOptions(9.5f));
        g.drawText(String(parameter.value, 2), valueBox, Justification::centredRight);
    }
}

Rectangle<float> TrimeshSidePanelRenderer::vertexParameterRowBounds(
        Rectangle<float> parameterArea,
        int parameterIndex) {
    auto rows = parameterArea.reduced(10.f, 28.f);
    rows.translate(0.f, (float) parameterIndex * 32.f);
    return rows.removeFromTop(28.f);
}

Rectangle<float> TrimeshSidePanelRenderer::vertexParameterRailBounds(Rectangle<float> parameterRow) {
    parameterRow.removeFromLeft(70.f);
    parameterRow.removeFromRight(44.f);
    return parameterRow.withTrimmedRight(8.f)
            .withSizeKeepingCentre(jmax(4.f, parameterRow.getWidth() - 8.f), 5.f);
}

}
