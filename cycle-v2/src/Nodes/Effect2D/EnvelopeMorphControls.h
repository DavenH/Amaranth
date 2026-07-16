#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

using juce::Colour;
using juce::Graphics;
using juce::Rectangle;

class EnvelopeMorphControls {
public:
    static constexpr float controlsHeight = 246.f;

    Rectangle<float> planeBounds(Rectangle<float> controls) const;
    Rectangle<float> railColumn(Rectangle<float> controls) const;
    Rectangle<float> morphRow(Rectangle<float> controls, int axis) const;
    Rectangle<float> axisBounds(Rectangle<float> controls, int axis) const;
    Rectangle<float> linkBounds(Rectangle<float> controls, int axis) const;
    Rectangle<float> vertexBounds(Rectangle<float> controls) const;

    void draw(
            Graphics& graphics,
            Rectangle<float> controls,
            float red,
            float blue,
            int viewAxis,
            bool redLinked,
            bool blueLinked) const;

private:
    Rectangle<float> squareColumn(Rectangle<float> controls) const;
    Colour axisColour(int axis) const;
    void drawPlane(
            Graphics& graphics,
            Rectangle<float> controls,
            float red,
            float blue) const;
};

}
