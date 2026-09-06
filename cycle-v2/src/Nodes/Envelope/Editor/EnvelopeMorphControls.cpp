#include "Nodes/Envelope/Editor/EnvelopeMorphControls.h"

#include "Graph/NodeGraph.h"
#include "Nodes/Trimesh/Rendering/TrimeshSidePanelRenderer.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/PropertyControls.h"

using namespace juce;

namespace CycleV2 {

namespace {

const Colour kText { 0xffe2e8ef };
const Colour kGroupFill { 0xff151c24 };
const Colour kGroupBorder { 0xff536171 };
const Colour kSelectedFill { 0xff2b415a };
constexpr float kPurposeLabelY = 0.f;
constexpr float kPurposeControlY = 18.f;
constexpr float kMorphLabelY = 53.f;
constexpr float kMorphRowsY = 71.f;
constexpr float kActionLabelY = 176.f;
constexpr float kActionControlY = 194.f;
constexpr float kActionColumnGap = 8.f;
constexpr float kActionColumnWidths[] { 92.f, 96.f, 80.f };

Path segmentedHighlight(Rectangle<float> bounds, int selectedSegment) {
    auto selected = bounds;
    selected.setWidth(bounds.getWidth() * 0.5f);
    if (selectedSegment == 1) {
        selected.setPosition(bounds.getCentreX(), bounds.getY());
    }
    Path result;
    result.addRoundedRectangle(
            selected.getX(), selected.getY(), selected.getWidth(), selected.getHeight(),
            5.f, 5.f,
            selectedSegment == 0, selectedSegment == 1,
            selectedSegment == 0, selectedSegment == 1);
    return result;
}

void drawSegmentedGroup(
        Graphics& graphics,
        Rectangle<float> bounds,
        int selectedSegment) {
    graphics.setColour(kGroupFill);
    graphics.fillRoundedRectangle(bounds, CanvasChromeMetrics::controlCornerRadius);
    if (selectedSegment >= 0) {
        graphics.setColour(kSelectedFill);
        graphics.fillPath(segmentedHighlight(bounds, selectedSegment));
    }
    graphics.setColour(kGroupBorder.withAlpha(0.74f));
    graphics.drawVerticalLine(
            roundToInt(bounds.getCentreX()), bounds.getY() + 3.f, bounds.getBottom() - 3.f);
    graphics.setColour(kGroupBorder.withAlpha(0.82f));
    graphics.drawRoundedRectangle(
            bounds,
            CanvasChromeMetrics::controlCornerRadius,
            CanvasChromeMetrics::restingBorderWidth);
}

}

Rectangle<float> EnvelopeMorphControls::squareColumn(Rectangle<float> controls) const {
    controls.reduce(12.f, 8.f);
    return controls.removeFromLeft(178.f);
}

Rectangle<float> EnvelopeMorphControls::planeBounds(Rectangle<float> controls) const {
    auto bounds = squareColumn(controls);
    bounds.removeFromTop(23.f);
    const float size = jmin(bounds.getWidth() - 20.f, bounds.getHeight() - 8.f);
    return Rectangle<float>(size, size).withCentre(bounds.getCentre());
}

Rectangle<float> EnvelopeMorphControls::railColumn(Rectangle<float> controls) const {
    controls.reduce(12.f, 8.f);
    controls.removeFromLeft(200.f);
    return controls.removeFromLeft(328.f);
}

Rectangle<float> EnvelopeMorphControls::purposeGroupLabelBounds(Rectangle<float> controls) const {
    auto column = railColumn(controls);
    return {
            column.getX(),
            column.getY() + kPurposeLabelY,
            152.f,
            (float) PropertyControlMetrics::groupLabelHeight
    };
}

Rectangle<float> EnvelopeMorphControls::purposeSelectorBounds(Rectangle<float> controls) const {
    auto column = railColumn(controls);
    return { column.getX(), column.getY() + kPurposeControlY, 152.f, 30.f };
}

Rectangle<float> EnvelopeMorphControls::morphGroupLabelBounds(Rectangle<float> controls) const {
    auto column = railColumn(controls);
    column.removeFromRight(58.f);
    return {
            column.getX(),
            column.getY() + kMorphLabelY,
            column.getWidth(),
            (float) PropertyControlMetrics::groupLabelHeight
    };
}

Rectangle<float> EnvelopeMorphControls::morphRow(Rectangle<float> controls, int axis) const {
    auto row = railColumn(controls);
    row.removeFromTop(kMorphRowsY + 35.f * static_cast<float>(axis));
    return row.removeFromTop(32.f);
}

Rectangle<float> EnvelopeMorphControls::actionRow(Rectangle<float> controls) const {
    auto row = railColumn(controls);
    row.removeFromTop(kActionControlY);
    return row.removeFromTop(28.f);
}

Rectangle<float> EnvelopeMorphControls::actionColumnBounds(
        Rectangle<float> controls,
        int column) const {
    auto area = railColumn(controls);
    const float totalWidth = kActionColumnWidths[0]
            + kActionColumnWidths[1]
            + kActionColumnWidths[2]
            + 2.f * kActionColumnGap;
    area = area.withSizeKeepingCentre(totalWidth, area.getHeight());
    for (int index = 0; index < column; ++index) {
        area.removeFromLeft(kActionColumnWidths[index] + kActionColumnGap);
    }
    return area.removeFromLeft(kActionColumnWidths[column]);
}

Rectangle<float> EnvelopeMorphControls::markerGroupLabelBounds(Rectangle<float> controls) const {
    auto column = actionColumnBounds(controls, 0);
    return { column.getX(), column.getY() + kActionLabelY,
             column.getWidth(), (float) PropertyControlMetrics::groupLabelHeight };
}

Rectangle<float> EnvelopeMorphControls::markerGroupBounds(Rectangle<float> controls) const {
    auto column = actionColumnBounds(controls, 0);
    return { column.getX(), column.getY() + kActionControlY, column.getWidth(), 28.f };
}

Rectangle<float> EnvelopeMorphControls::axisScaleGroupLabelBounds(Rectangle<float> controls) const {
    auto column = actionColumnBounds(controls, 1);
    return { column.getX(), column.getY() + kActionLabelY,
             column.getWidth(), (float) PropertyControlMetrics::groupLabelHeight };
}

Rectangle<float> EnvelopeMorphControls::axisScaleBounds(Rectangle<float> controls) const {
    auto column = actionColumnBounds(controls, 1);
    return { column.getX(), column.getY() + kActionControlY, column.getWidth(), 28.f };
}

Rectangle<float> EnvelopeMorphControls::rangeGroupLabelBounds(Rectangle<float> controls) const {
    auto column = actionColumnBounds(controls, 2);
    return { column.getX(), column.getY() + kActionLabelY,
             column.getWidth(), (float) PropertyControlMetrics::groupLabelHeight };
}

Rectangle<float> EnvelopeMorphControls::rangeGroupBounds(Rectangle<float> controls) const {
    auto column = actionColumnBounds(controls, 2);
    return { column.getX(), column.getY() + kActionControlY, column.getWidth(), 28.f };
}

Rectangle<float> EnvelopeMorphControls::axisBounds(Rectangle<float> controls, int axis) const {
    const auto row = morphRow(controls, axis);
    return { row.getRight() - 52.f, row.getCentreY() - 10.f, 20.f, 20.f };
}

Rectangle<float> EnvelopeMorphControls::linkBounds(Rectangle<float> controls, int axis) const {
    const auto row = morphRow(controls, axis);
    return { row.getRight() - 26.f, row.getCentreY() - 10.f, 20.f, 20.f };
}

Rectangle<float> EnvelopeMorphControls::vertexBounds(Rectangle<float> controls) const {
    controls.reduce(12.f, 8.f);
    controls.removeFromLeft(546.f);
    return controls;
}

Rectangle<float> EnvelopeMorphControls::vertexParameterRowBounds(
        Rectangle<float> controls,
        int parameterIndex) const {
    return TrimeshSidePanelRenderer::vertexParameterRowBounds(
            vertexBounds(controls),
            parameterIndex,
            vertexParameterHeightScale);
}

Colour EnvelopeMorphControls::axisColour(int axis) const {
    static const MorphDimension dimensions[] {
        MorphDimension::Yellow,
        MorphDimension::Red,
        MorphDimension::Blue
    };
    return colourForMorphDimension(dimensions[jlimit(0, 2, axis)]);
}

void EnvelopeMorphControls::drawPlane(
        Graphics& graphics,
        Rectangle<float> controls,
        float red,
        float blue) const {
    auto column = squareColumn(controls);
    auto header = column.removeFromTop(23.f);
    const auto square = planeBounds(controls);
    const Point<float> cursor {
        square.getX() + square.getWidth() * red,
        square.getBottom() - square.getHeight() * blue
    };
    const auto cursorBounds = Rectangle<float>(8.f, 8.f).withCentre(cursor);

    paintPropertyGroupLabel(graphics, header, "Morph plane");
    graphics.setColour(Colour(0xff5f91e8).withAlpha(0.20f));
    graphics.fillRect(square);
    graphics.setGradientFill(ColourGradient(
            Colour(0x00d65a5a),
            square.getCentreX(),
            square.getY(),
            Colour(0x66d65a5a),
            square.getCentreX(),
            square.getBottom(),
            false));
    graphics.fillRect(square);
    graphics.setColour(kText.withAlpha(0.3f));
    graphics.drawRect(square, 1.f);
    graphics.setColour(Colours::black.withAlpha(0.75f));
    graphics.fillEllipse(cursorBounds);
    graphics.setColour(kText);
    graphics.drawEllipse(cursorBounds, 1.2f);
}

void EnvelopeMorphControls::draw(
        Graphics& graphics,
        Rectangle<float> controls,
        float red,
        float blue,
        int viewAxis,
        bool redLinked,
        bool blueLinked,
        bool loopSelected,
        bool sustainSelected) const {
    const bool linked[] { true, redLinked, blueLinked };
    drawPlane(graphics, controls, red, blue);
    drawActionGroups(graphics, controls, loopSelected, sustainSelected);
    paintPropertyGroupLabel(
            graphics,
            purposeGroupLabelBounds(controls),
            "Envelope purpose");
    paintPropertyGroupLabel(
            graphics,
            morphGroupLabelBounds(controls),
            "Morph position");
    paintPropertyGroupLabel(
            graphics,
            markerGroupLabelBounds(controls),
            "Envelope markers");
    paintPropertyGroupLabel(
            graphics,
            axisScaleGroupLabelBounds(controls),
            "Axis scale");
    paintPropertyGroupLabel(
            graphics,
            rangeGroupLabelBounds(controls),
            "Vertical range");
    TrimeshSidePanelRenderer::drawMorphColumnHeaders(
            graphics,
            morphRow(controls, 0),
            axisBounds(controls, 0),
            linkBounds(controls, 0));

    for (int axis = 0; axis < 3; ++axis) {
        const auto axisArea = axisBounds(controls, axis);
        const auto linkArea = linkBounds(controls, axis);
        const auto colour = axisColour(axis);
        graphics.setColour(colour.withAlpha(axis == viewAxis ? 0.45f : 0.06f));
        graphics.fillRoundedRectangle(axisArea, CanvasChromeMetrics::controlCornerRadius);
        graphics.setColour(colour.withAlpha(axis == viewAxis ? 1.f : 0.32f));
        graphics.drawRoundedRectangle(
                axisArea,
                CanvasChromeMetrics::controlCornerRadius,
                axis == viewAxis
                        ? CanvasChromeMetrics::activeBorderWidth
                        : CanvasChromeMetrics::restingBorderWidth);
        graphics.setColour(colour.withAlpha(linked[axis] ? 0.9f : 0.25f));
        graphics.drawRoundedRectangle(
                linkArea,
                CanvasChromeMetrics::controlCornerRadius,
                linked[axis]
                        ? CanvasChromeMetrics::activeBorderWidth
                        : CanvasChromeMetrics::restingBorderWidth);
    }
}

void EnvelopeMorphControls::drawActionGroups(
        Graphics& graphics,
        Rectangle<float> controls,
        bool loopSelected,
        bool sustainSelected) const {
    int selectedMarker = -1;
    if (loopSelected) {
        selectedMarker = 0;
    } else if (sustainSelected) {
        selectedMarker = 1;
    }
    drawSegmentedGroup(graphics, markerGroupBounds(controls), selectedMarker);
    drawSegmentedGroup(graphics, rangeGroupBounds(controls), -1);
}

}
