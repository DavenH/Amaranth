#include "Nodes/Envelope/Editor/EnvelopeMorphControls.h"
#include "UI/Editors/MorphControlLayout.h"
#include "UI/Editors/MorphSpaceLayout.h"

#include "Graph/NodeGraph.h"
#include "Nodes/Trimesh/Rendering/TrimeshSidePanelRenderer.h"
#include "UI/CanvasChromeMetrics.h"
#include "UI/Editors/PropertyControlLookAndFeel.h"
#include "UI/Editors/PropertyControls.h"
#include "UI/EnvelopeToolbarMetrics.h"

using namespace juce;

namespace CycleV2 {

namespace {

const Colour kGroupFill { 0xff151c24 };
const Colour kGroupBorder { 0xff536171 };
constexpr float kActionTopInset = 4.f;
constexpr float kActionBottomInset = 12.f;
constexpr float kActionColumnGap = 28.f;
constexpr float kActionColumnWidths[] {
        EnvelopeToolbarMetrics::scalingSelectorWidth,
        EnvelopeToolbarMetrics::pairedActionWidth
};
constexpr float kActionBarHeight = PropertyControlMetrics::groupLabelHeight
        + EnvelopeToolbarMetrics::controlHeight + kActionTopInset + kActionBottomInset;

void drawSegmentedGroup(
        Graphics& graphics,
        Rectangle<float> bounds) {
    graphics.setColour(kGroupFill);
    graphics.fillRoundedRectangle(bounds, CanvasChromeMetrics::controlCornerRadius);
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

float EnvelopeMorphControls::actionBarHeight() {
    return kActionBarHeight;
}

Rectangle<float> EnvelopeMorphControls::planeGroupLabelBounds(Rectangle<float> controls) const {
    const auto plane = planeBounds(controls);
    return plane.isEmpty() ? plane : plane.reduced(8.f, 5.f).removeFromTop(18.f);
}

Rectangle<float> EnvelopeMorphControls::planeBounds(Rectangle<float> controls) const {
    return MorphControlLayout(controls, dimensionCount).diagram();
}

Rectangle<float> EnvelopeMorphControls::morphInputBounds(Rectangle<float> controls) const {
    const auto diagram = planeBounds(controls);
    if (dimensionCount == 3) {
        return diagram;
    }
    const MorphSpaceLayout space(diagram, MorphDimensions(dimensionCount));
    return dimensionCount == 1 ? space.surface.withSizeKeepingCentre(space.surface.getWidth(), 24.f)
            : space.surface;
}

Rectangle<float> EnvelopeMorphControls::morphGroupLabelBounds(Rectangle<float> controls) const {
    const int firstAxis = dimensionCount == 2 ? 1 : 0;
    auto row = morphRow(controls, firstAxis);
    return row.isEmpty() ? row : row.withY(row.getY() - 12.f).withHeight(10.f);
}

Rectangle<float> EnvelopeMorphControls::morphRow(Rectangle<float> controls, int axis) const {
    return MorphControlLayout(controls, dimensionCount).row(axis);
}

Rectangle<float> EnvelopeMorphControls::morphSliderBounds(
        Rectangle<float> controls,
        int axis) const {
    const MorphControlLayout layout(controls, dimensionCount);
    return layout.row(axis).withRight(layout.rail(axis).getRight());
}

Rectangle<float> EnvelopeMorphControls::keyboardFocusBounds(
        Rectangle<float> controls,
        int axis) const {
    return MorphControlLayout(controls, dimensionCount).keyboardFocus(axis);
}

Rectangle<float> EnvelopeMorphControls::actionBarBounds(Rectangle<float> controls) const {
    const float horizontalInset = controls.getX();
    auto bar = controls.expanded(horizontalInset, 0.f);
    return bar.removeFromBottom(kActionBarHeight);
}

Rectangle<float> EnvelopeMorphControls::actionRow(Rectangle<float> controls) const {
    auto row = actionBarBounds(controls);
    row.removeFromBottom(kActionBottomInset);
    return row.removeFromBottom(EnvelopeToolbarMetrics::controlHeight);
}

Rectangle<float> EnvelopeMorphControls::actionColumnBounds(
        Rectangle<float> controls,
        int column) const {
    auto area = actionRow(controls);
    const float totalWidth = kActionColumnWidths[0]
            + kActionColumnWidths[1]
            + kActionColumnGap;
    area = area.withSizeKeepingCentre(totalWidth, area.getHeight());
    for (int index = 0; index < column; ++index) {
        area.removeFromLeft(kActionColumnWidths[index] + kActionColumnGap);
    }
    return area.removeFromLeft(kActionColumnWidths[column]);
}

Rectangle<float> EnvelopeMorphControls::markerGroupBounds(Rectangle<float> panel) const {
    return Rectangle<float>(EnvelopeToolbarMetrics::pairedActionWidth, EnvelopeToolbarMetrics::controlHeight)
            .withPosition(panel.getRight() - EnvelopeToolbarMetrics::pairedActionWidth - 8.f, panel.getY() + 8.f);
}

Rectangle<float> EnvelopeMorphControls::axisScaleGroupLabelBounds(Rectangle<float> controls) const {
    return actionGroupLabelBounds(controls, 0);
}

Rectangle<float> EnvelopeMorphControls::axisScaleBounds(Rectangle<float> controls) const {
    return actionColumnBounds(controls, 0);
}

Rectangle<float> EnvelopeMorphControls::rangeGroupLabelBounds(Rectangle<float> controls) const {
    return actionGroupLabelBounds(controls, 1);
}

Rectangle<float> EnvelopeMorphControls::rangeGroupBounds(Rectangle<float> controls) const {
    return actionColumnBounds(controls, 1);
}

Rectangle<float> EnvelopeMorphControls::actionGroupLabelBounds(
        Rectangle<float> controls,
        int column) const {
    auto bounds = actionColumnBounds(controls, column);
    bounds.setY(actionBarBounds(controls).getY() + kActionTopInset);
    bounds.setHeight((float) PropertyControlMetrics::groupLabelHeight);
    return bounds;
}

Rectangle<float> EnvelopeMorphControls::axisGroupLabelBounds(
        Rectangle<float> controls) const {
    const int axis = dimensionCount == 2 ? 1 : 0;
    const auto button = axisBounds(controls, axis);
    return button.isEmpty() ? button
            : TrimeshSidePanelRenderer::morphColumnHeaderBounds(button, morphRow(controls, axis));
}

Rectangle<float> EnvelopeMorphControls::axisBounds(Rectangle<float> controls, int axis) const {
    return MorphControlLayout(controls, dimensionCount).axis(axis);
}

Rectangle<float> EnvelopeMorphControls::linkBounds(Rectangle<float> controls, int axis) const {
    return MorphControlLayout(controls, dimensionCount).link(axis);
}

Rectangle<float> EnvelopeMorphControls::vertexBounds(Rectangle<float> controls) const {
    return MorphControlLayout(controls, dimensionCount).vertices();
}

Rectangle<float> EnvelopeMorphControls::vertexParameterRowBounds(
        Rectangle<float> controls,
        const String& parameterId) const {
    return TrimeshSidePanelRenderer::vertexParameterRowBounds(
            vertexBounds(controls),
            TrimeshSidePanelRenderer::vertexParameterSlot(parameterId, 7),
            vertexParameterHeightScale, 7);
}

Colour EnvelopeMorphControls::axisColour(int axis) const {
    static const MorphDimension dimensions[] {
        MorphDimension::Yellow,
        MorphDimension::Red,
        MorphDimension::Blue
    };
    return colourForMorphDimension(dimensions[jlimit(0, 2, axis)]);
}

void EnvelopeMorphControls::draw(
        Graphics& graphics, Rectangle<float> controls, float red, float blue,
        int viewAxis, bool redLinked, bool blueLinked, float yellow, bool yellowLinked,
        const std::vector<TrimeshCubePreviewVertex>& vertices) const {
    const std::array<TrimeshSidePanelRenderer::AxisState, 3> axes {
            TrimeshSidePanelRenderer::AxisState { "Yellow", "Y", axisColour(0), yellow, viewAxis == 0, yellowLinked },
            TrimeshSidePanelRenderer::AxisState { "Red", "R", axisColour(1), red, viewAxis == 1, redLinked },
            TrimeshSidePanelRenderer::AxisState { "Blue", "B", axisColour(2), blue, viewAxis == 2, blueLinked }
    };
    TrimeshSidePanelRenderer::drawMorphControls(graphics,
            controls, axes, dimensionCount,
            MorphDimensions::Plane::RedBlue, vertices);
}

void EnvelopeMorphControls::drawActions(Graphics& graphics, Rectangle<float> controls) const {
    drawActionGroups(graphics, controls);
    paintPropertyGroupLabel(graphics, axisScaleGroupLabelBounds(controls), "Scaling");
    paintPropertyGroupLabel(graphics, rangeGroupLabelBounds(controls), "Zoom");
}

void EnvelopeMorphControls::drawActionGroups(
        Graphics& graphics,
        Rectangle<float> controls) const {
    auto bar = actionBarBounds(controls);
    graphics.setColour(Colour(0xff0f151c).withAlpha(0.72f));
    graphics.fillRect(bar);
    graphics.setColour(kGroupBorder.withAlpha(0.26f));
    graphics.fillRect(bar.removeFromTop(1.f));
    graphics.fillRect(bar.removeFromBottom(1.f));

    drawSegmentedGroup(graphics, rangeGroupBounds(controls));
}

}
