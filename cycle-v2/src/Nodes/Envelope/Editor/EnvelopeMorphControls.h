#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

using juce::Colour;
using juce::Graphics;
using juce::Rectangle;

class EnvelopeMorphControls {
public:
    static constexpr float controlsHeight = 286.f;
    static constexpr float vertexParameterHeightScale = 1.15f;

    Rectangle<float> planeGroupLabelBounds(Rectangle<float> controls) const;
    Rectangle<float> planeBounds(Rectangle<float> controls) const;
    Rectangle<float> railColumn(Rectangle<float> controls) const;
    Rectangle<float> purposeGroupLabelBounds(Rectangle<float> controls) const;
    Rectangle<float> purposeSelectorBounds(Rectangle<float> controls) const;
    Rectangle<float> morphGroupLabelBounds(Rectangle<float> controls) const;
    Rectangle<float> morphRow(Rectangle<float> controls, int axis) const;
    Rectangle<float> actionBarBounds(Rectangle<float> controls) const;
    Rectangle<float> actionRow(Rectangle<float> controls) const;
    Rectangle<float> markerGroupLabelBounds(Rectangle<float> controls) const;
    Rectangle<float> markerGroupBounds(Rectangle<float> controls) const;
    Rectangle<float> axisScaleGroupLabelBounds(Rectangle<float> controls) const;
    Rectangle<float> axisScaleBounds(Rectangle<float> controls) const;
    Rectangle<float> rangeGroupLabelBounds(Rectangle<float> controls) const;
    Rectangle<float> rangeGroupBounds(Rectangle<float> controls) const;
    Rectangle<float> axisGroupLabelBounds(Rectangle<float> controls) const;
    Rectangle<float> linkGroupLabelBounds(Rectangle<float> controls) const;
    Rectangle<float> axisBounds(Rectangle<float> controls, int axis) const;
    Rectangle<float> linkBounds(Rectangle<float> controls, int axis) const;
    Rectangle<float> vertexBounds(Rectangle<float> controls) const;
    Rectangle<float> vertexParameterRowBounds(
            Rectangle<float> controls,
            int parameterIndex) const;

    void draw(
            Graphics& graphics,
            Rectangle<float> controls,
            float red,
            float blue,
            int viewAxis,
            bool redLinked,
            bool blueLinked,
            bool loopSelected,
            bool sustainSelected) const;

private:
    Rectangle<float> squareColumn(Rectangle<float> controls) const;
    Rectangle<float> actionColumnBounds(Rectangle<float> controls, int column) const;
    Colour axisColour(int axis) const;
    void drawPlane(
            Graphics& graphics,
            Rectangle<float> controls,
            float red,
            float blue) const;
    void drawActionGroups(
            Graphics& graphics,
            Rectangle<float> controls,
            bool loopSelected,
            bool sustainSelected) const;
};

}
