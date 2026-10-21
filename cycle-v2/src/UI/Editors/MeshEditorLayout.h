#pragma once

#include "JuceHeader.h"

namespace CycleV2 {

// Shared body composition; domain editors retain their own tools and footer.
class MeshEditorLayout {
public:
    MeshEditorLayout(juce::Rectangle<float> content, int dimensionCount);

    juce::Rectangle<float> controls() const;
    juce::Rectangle<float> surface() const;
    juce::Rectangle<float> curve() const;
    static juce::Rectangle<float> staticEditorBounds(juce::Rectangle<float> fullBounds);

    static constexpr float controlFooterHeight = 52.f;

private:
    juce::Rectangle<float> content;
    int dimensionCount;
};

}
