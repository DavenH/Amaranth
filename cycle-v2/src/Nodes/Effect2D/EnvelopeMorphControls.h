#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

using juce::Colour;
using juce::Graphics;
using juce::Rectangle;

class EnvelopeMorphControls {
public:
    static constexpr float controlsHeight = 246.f;
    static constexpr float vertexParameterHeightScale = 1.15f;

    Rectangle<float> planeBounds(Rectangle<float> controls) const;
    Rectangle<float> railColumn(Rectangle<float> controls) const;
    Rectangle<float> modeRow(Rectangle<float> controls) const;
    Rectangle<float> morphRow(Rectangle<float> controls, int axis) const;
    Rectangle<float> actionRow(Rectangle<float> controls) const;
    Rectangle<float> vertexModeLabelBounds(Rectangle<float> controls) const;
    Rectangle<float> vertexModeGroupBounds(Rectangle<float> controls) const;
    Rectangle<float> logarithmicBounds(Rectangle<float> controls) const;
    Rectangle<float> rangeGroupBounds(Rectangle<float> controls) const;
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
