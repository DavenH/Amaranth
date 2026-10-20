#pragma once

#include <JuceHeader.h>
#include <Curve/Mesh/MorphDimensions.h>

namespace CycleV2 {

class MorphControlLayout {
public:
    MorphControlLayout(juce::Rectangle<float> bounds, int count);
    juce::Rectangle<float> diagram() const;
    juce::Rectangle<float> row(int axisIndex) const;
    juce::Rectangle<float> rail(int axisIndex) const;
    juce::Rectangle<float> axis(int axisIndex) const;
    juce::Rectangle<float> link(int axisIndex) const;
    juce::Rectangle<float> vertices() const;

private:
    juce::Rectangle<float> morphColumn() const;
    juce::Rectangle<float> area;
    MorphDimensions dimensions;
};

}
