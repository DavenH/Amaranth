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
    void renderCable(
            const juce::Path& path,
            juce::Point<float> source,
            juce::Point<float> dest,
            juce::Colour colour,
            float cableScale,
            bool selected,
            bool attachment,
            bool invalid);
    void renderNodeShell(
            juce::Rectangle<float> bounds,
            float headerHeight,
            float cornerRadius,
            juce::Colour bodyColour,
            juce::Colour headerColour);

private:
    struct LineSegment {
        juce::Point<float> start;
        juce::Point<float> end;
    };

    static void setColour(juce::Colour colour);
    static void fillRect(juce::Rectangle<float> bounds);
    static void drawLine(juce::Point<float> start, juce::Point<float> end);
    static void drawCircle(juce::Point<float> centre, float radius, juce::Colour colour, bool filled);
    static void fillRoundedRect(juce::Rectangle<float> bounds, float radius);
    static void fillCornerFan(juce::Point<float> centre, float radius, float startAngle, float endAngle);
    static void drawThickSegment(juce::Point<float> start, juce::Point<float> end, float width);
    static std::vector<LineSegment> flattenPath(const juce::Path& path);
    static void drawSegments(const std::vector<LineSegment>& segments, juce::Colour colour, float width);
    static void drawDashedSegments(
            const std::vector<LineSegment>& segments,
            juce::Colour colour,
            float width,
            float dashLength,
            float gapLength);
    static void drawGridLines(
            juce::Rectangle<float> bounds,
            juce::Point<float> pan,
            float step,
            juce::Colour colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeCanvasGlRenderer)
};

}
