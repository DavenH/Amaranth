#pragma once

#include <JuceHeader.h>

namespace CycleV2 {

class NodeCanvasGlRenderer {
public:
    NodeCanvasGlRenderer() = default;

    void initialize();
    void shutdown();
    void renderBackground(
            int width,
            int height,
            float renderingScale,
            float zoom,
            juce::Point<float> pan);

private:
    static void setColour(juce::Colour colour);
    static void fillRect(juce::Rectangle<float> bounds);
    static void drawLine(juce::Point<float> start, juce::Point<float> end);
    static void drawGridLines(
            juce::Rectangle<float> bounds,
            juce::Point<float> pan,
            float step,
            juce::Colour colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeCanvasGlRenderer)
};

}
