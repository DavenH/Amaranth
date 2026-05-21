#pragma once

#include "TrimeshNodeModel.h"

#include <JuceHeader.h>

#include <cstdint>

namespace CycleV2 {

class TrimeshWidget {
public:
    void syncFromNode(const Node& node);

    void paintCompact(
            juce::Graphics& g,
            const Node& node,
            juce::Rectangle<float> area,
            float zoom);

    void paintExpanded(
            juce::Graphics& g,
            const Node& node,
            juce::Rectangle<float> content);

    bool findMorphControlAt(
            juce::Rectangle<float> content,
            juce::Point<float> position,
            juce::String& parameterId,
            float& value) const;
    bool morphValueForParameterAt(
            juce::Rectangle<float> content,
            const juce::String& parameterId,
            juce::Point<float> position,
            float& value) const;
    bool findPrimaryAxisAt(
            juce::Rectangle<float> content,
            juce::Point<float> position,
            juce::String& axisValue) const;

private:
    struct CachedHeatmap {
        juce::Image image;
        size_t valueCount {};
        int rows {};
        int columns {};
        uint64_t revision {};
    };

    static juce::Colour meshSurfaceColour(float value);
    static juce::Rectangle<float> meshPreviewContentArea(juce::Rectangle<float> area);
    static void drawPanelFrame(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const juce::String& title);
    static void drawMeshHeatmap(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const TrimeshRenderData& renderData,
            bool drawGrid);
    static void drawTrace(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::vector<float>& values,
            juce::Colour colour);
    static void drawVertexParameters(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::vector<TrimeshVertexParameter>& parameters);
    static juce::Rectangle<float> morphPanelBounds(juce::Rectangle<float> content);
    static juce::Rectangle<float> morphRailBounds(juce::Rectangle<float> morphArea, int axisIndex);
    static juce::Rectangle<float> primaryAxisBounds(juce::Rectangle<float> morphArea, int axisIndex);
    static juce::String primaryAxisValue(int axis);

    juce::Image createHeatmapImage(const TrimeshRenderData& renderData) const;

    TrimeshNodeModel model;
    CachedHeatmap compactHeatmap;
};

}
