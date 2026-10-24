#include "Nodes/Envelope/Editor/EnvelopeMorphControls.h"
#include "UI/Editors/MorphControlLayout.h"
#include "UI/Editors/MorphSpaceLayout.h"

#include "Graph/NodeGraph.h"
#include "Nodes/Trimesh/Rendering/TrimeshSidePanelRenderer.h"
#include "UI/EnvelopeToolbarMetrics.h"

using namespace juce;

namespace CycleV2 {

namespace {

constexpr float kOverlayGroupGap = 16.f;

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
    return space.inputBounds();
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

Rectangle<float> EnvelopeMorphControls::markerGroupBounds(Rectangle<float> panel) const {
    return Rectangle<float>(EnvelopeToolbarMetrics::pairedActionWidth, EnvelopeToolbarMetrics::controlHeight)
            .withPosition(panel.getRight() - EnvelopeToolbarMetrics::pairedActionWidth - 8.f, panel.getY() + 8.f);
}

Rectangle<float> EnvelopeMorphControls::axisScaleBounds(Rectangle<float> panel) const {
    const auto zoom = rangeGroupBounds(panel);
    return zoom.withWidth(EnvelopeToolbarMetrics::scalingSelectorWidth)
            .withX(zoom.getX() - kOverlayGroupGap - EnvelopeToolbarMetrics::scalingSelectorWidth);
}

Rectangle<float> EnvelopeMorphControls::rangeGroupBounds(Rectangle<float> panel) const {
    const auto markers = markerGroupBounds(panel);
    return markers.translated(-markers.getWidth() - kOverlayGroupGap, 0.f);
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

}
