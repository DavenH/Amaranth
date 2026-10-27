#pragma once

#include <JuceHeader.h>
#include <Curve/Mesh/MorphDimensions.h>

#include <array>

namespace CycleV2 {

// Presentation geometry shared by diagram painting and pole-link hit targets.
class MorphSpaceLayout {
public:
    MorphSpaceLayout(juce::Rectangle<float> diagram, MorphDimensions dimensions);
    juce::Point<float> project(float red, float blue, float yellow) const;
    juce::Rectangle<float> inputBounds() const;
    juce::Point<float> reducedCoordinates(juce::Point<float> position) const;
    juce::Point<float> cubeFacePoint(int heldAxis, float first, float second) const;
    int cubeFaceAt(juce::Point<float> position) const;
    static std::array<int, 2> cubeFaceAxes(int heldAxis);
    std::array<float, 3> dragCubeFace(int heldAxis, juce::Point<float> delta,
            std::array<float, 3> initial) const;
    static juce::Rectangle<float> lockBadge(juce::Rectangle<float> target);

    juce::Rectangle<float> surface;
    std::array<juce::Point<float>, 8> corners;
    std::array<juce::Line<float>, 3> rails;
    std::array<juce::Rectangle<float>, 3> labels;
    std::array<juce::Rectangle<float>, 3> locks;

private:
    juce::Point<float> cubeFaceCoordinates(int heldAxis, juce::Point<float> delta) const;
    void placeLock(int axis, juce::Point<float> outward);
    MorphDimensions dimensions;
    juce::Point<float> origin;
    float scale {};
    std::array<int, 2> planeAxes {};
};

}
