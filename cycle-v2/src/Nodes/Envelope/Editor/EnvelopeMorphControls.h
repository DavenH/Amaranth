#pragma once

#include <JuceHeader.h>
#include "Nodes/Trimesh/Model/TrimeshNodeModel.h"

namespace CycleV2 {

using juce::Colour;
using juce::Graphics;
using juce::Rectangle;

class EnvelopeMorphControls {
public:
    void setDimensionCount(int count) { dimensionCount = count; }
    int getDimensionCount() const { return dimensionCount; }
    static constexpr float vertexParameterHeightScale = 1.f;

    Rectangle<float> planeGroupLabelBounds(Rectangle<float> controls) const;
    Rectangle<float> planeBounds(Rectangle<float> controls) const;
    Rectangle<float> morphInputBounds(Rectangle<float> controls) const;
    Rectangle<float> morphGroupLabelBounds(Rectangle<float> controls) const;
    Rectangle<float> morphRow(Rectangle<float> controls, int axis) const;
    Rectangle<float> morphSliderBounds(Rectangle<float> controls, int axis) const;
    Rectangle<float> keyboardFocusBounds(Rectangle<float> controls, int axis) const;
    Rectangle<float> markerGroupBounds(Rectangle<float> panel) const;
    Rectangle<float> axisScaleBounds(Rectangle<float> panel) const;
    Rectangle<float> rangeGroupBounds(Rectangle<float> panel) const;
    Rectangle<float> axisGroupLabelBounds(Rectangle<float> controls) const;
    Rectangle<float> axisBounds(Rectangle<float> controls, int axis) const;
    Rectangle<float> linkBounds(Rectangle<float> controls, int axis) const;
    Rectangle<float> vertexBounds(Rectangle<float> controls) const;
    Rectangle<float> vertexParameterRowBounds(
            Rectangle<float> controls,
            const juce::String& parameterId) const;

    void draw(
            Graphics& graphics,
            Rectangle<float> controls,
            float red,
            float blue,
            int viewAxis,
            bool redLinked,
            bool blueLinked,
            float yellow = 0.f,
            bool yellowLinked = true,
            const std::vector<TrimeshCubePreviewVertex>& vertices = {}) const;

private:
    int dimensionCount { 2 };
    Colour axisColour(int axis) const;
};

}
