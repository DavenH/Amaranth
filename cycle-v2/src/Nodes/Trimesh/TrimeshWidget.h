#pragma once

#include "TrimeshNodeModel.h"

#include <JuceHeader.h>

#include <array>
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
    bool findVertexParameterAt(
            juce::Rectangle<float> content,
            juce::Point<float> position,
            juce::String& parameterId,
            float& value) const;
    bool vertexParameterValueForParameterAt(
            juce::Rectangle<float> content,
            const juce::String& parameterId,
            juce::Point<float> position,
            float& value) const;
    bool findVertexSelectionAt(
            const Node& node,
            juce::Rectangle<float> content,
            juce::Point<float> position,
            int& vertexIndex);

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
    static void drawEditorGrid(
            juce::Graphics& g,
            juce::Rectangle<float> area);
    static void drawTraceFill(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::vector<float>& values);
    static void drawVertexMarkers(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::vector<TrimeshVertexMarker>& markers);
    static void drawMorphCubePreview(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::array<float, 3>& values);
    static void drawVertexParameters(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::vector<TrimeshVertexParameter>& parameters);
    static juce::Rectangle<float> morphPanelBounds(juce::Rectangle<float> content);
    static juce::Rectangle<float> morphRailBounds(juce::Rectangle<float> morphArea, int axisIndex);
    static juce::Rectangle<float> primaryAxisBounds(juce::Rectangle<float> morphArea, int axisIndex);
    static juce::String primaryAxisValue(int axis);
    static juce::Rectangle<float> vertexParameterPanelBounds(juce::Rectangle<float> content);
    static juce::Rectangle<float> vertexParameterRowBounds(juce::Rectangle<float> parameterArea, int parameterIndex);
    static juce::Rectangle<float> vertexParameterRailBounds(juce::Rectangle<float> parameterRow);
    static juce::String vertexParameterId(int parameterIndex);
    static juce::Rectangle<float> waveshapeContentBounds(juce::Rectangle<float> content);

    juce::Image createHeatmapImage(const TrimeshRenderData& renderData) const;

    TrimeshNodeModel model;
    CachedHeatmap compactHeatmap;
};

}
