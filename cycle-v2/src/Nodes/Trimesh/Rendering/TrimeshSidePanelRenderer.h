#pragma once

#include "Nodes/Trimesh/Model/TrimeshNodeModel.h"

#include <JuceHeader.h>
#include <Curve/Mesh/MorphDimensions.h>

#include <array>
#include <vector>

namespace CycleV2 {

class TrimeshSidePanelRenderer {
public:
    enum class GuideControls {
        Visible,
        Hidden
    };

    struct AxisState {
        juce::String label;
        juce::String shortLabel;
        juce::Colour colour;
        float value {};
        bool primary {};
        bool linked {};
    };

    static void drawSidePanel(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::array<AxisState, 3>& axes,
            const std::vector<TrimeshCubePreviewVertex>& cubeVertices,
            const std::vector<TrimeshVertexParameter>& parameters,
            const std::array<juce::String, 6>& guideAttachmentLabels,
            bool showOutputScale,
            PortDomain outputDomain,
            float outputScale,
            int dimensionCount = 3,
            const juce::String& feedbackParameter = {});
    static void drawMorphControls(
            juce::Graphics& g, juce::Rectangle<float> area,
            const std::array<AxisState, 3>& axes, int dimensionCount,
            MorphDimensions::Plane plane = MorphDimensions::Plane::RedBlue,
            const std::vector<TrimeshCubePreviewVertex>& vertices = {});
    static void drawMorphCubePreview(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::array<AxisState, 3>& axes,
            const std::vector<TrimeshCubePreviewVertex>& cubeVertices);
    static void drawReducedMorphPreview(
            juce::Graphics& graphics, juce::Rectangle<float> area,
            const std::array<AxisState, 3>& axes, MorphDimensions dimensions,
            const std::vector<TrimeshCubePreviewVertex>& vertices);
    static int vertexParameterSlot(const juce::String& id, int slotCount);
    static void drawVertexParameters(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const std::vector<TrimeshVertexParameter>& parameters,
            const std::array<juce::String, 6>& guideAttachmentLabels,
            float heightScale = 1.f,
            GuideControls guideControls = GuideControls::Visible,
            int stableSlotCount = 0,
            const juce::String& feedbackParameter = {});
    static void drawMorphColumnHeaders(
            juce::Graphics& g,
            juce::Rectangle<float> firstRow,
            juce::Rectangle<float> axisButton,
            juce::Rectangle<float> linkButton);

    static juce::Rectangle<float> morphCubeBounds(
            juce::Rectangle<float> sideArea,
            bool showOutputScale = false);
    static juce::Rectangle<float> morphRailBounds(
            juce::Rectangle<float> sideArea,
            int axisIndex,
            bool showOutputScale = false);
    static juce::Rectangle<float> morphMarkerBounds(
            juce::Rectangle<float> rail,
            float value);
    static juce::Rectangle<float> morphColumnHeaderBounds(
            juce::Rectangle<float> button,
            juce::Rectangle<float> firstRow);
    static juce::Rectangle<float> primaryAxisBounds(
            juce::Rectangle<float> sideArea,
            int axisIndex,
            bool showOutputScale = false);
    static juce::Rectangle<float> linkToggleBounds(
            juce::Rectangle<float> sideArea,
            int axisIndex,
            bool showOutputScale = false);
    static juce::Rectangle<float> keyboardFocusBounds(
            juce::Rectangle<float> sideArea,
            int axisIndex,
            bool showOutputScale = false,
            int dimensionCount = 3);
    static juce::Rectangle<float> outputScaleRowBounds(juce::Rectangle<float> sideArea, int dimensionCount = 3);
    static juce::Rectangle<float> outputScaleRailBounds(juce::Rectangle<float> sideArea, int dimensionCount = 3);
    static juce::Rectangle<float> vertexParameterPanelBounds(
            juce::Rectangle<float> sideArea,
            bool showOutputScale = false);
    static juce::Rectangle<float> vertexParameterRowBounds(
            juce::Rectangle<float> parameterArea,
            int parameterIndex,
            float heightScale = 1.f,
            int parameterCount = 6);
    static juce::String propertyTooltip(const std::vector<TrimeshVertexParameter>& parameters,
            const juce::String& id);
    static bool showsGuideControlsForParameter(const juce::String& parameterId);
    static juce::Rectangle<float> vertexParameterRailBounds(
            juce::Rectangle<float> parameterRow,
            GuideControls guideControls = GuideControls::Visible);
    static juce::Rectangle<float> vertexParameterGuideGainBounds(
            juce::Rectangle<float> parameterRow);
    static juce::Rectangle<float> vertexParameterGuideBounds(juce::Rectangle<float> parameterRow);

private:

    static void drawVertexParameter(
            juce::Graphics& graphics, juce::Rectangle<float> row,
            const TrimeshVertexParameter& parameter,
            const std::array<juce::String, 6>& guideAttachmentLabels, GuideControls guideControls,
            const juce::String& feedbackParameter);
};

}
