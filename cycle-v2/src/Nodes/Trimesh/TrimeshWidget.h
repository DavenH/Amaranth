#pragma once

#include "TrimeshPanelBridge.h"
#include "TrimeshRenderProfile.h"
#include "TrimeshSidePanelRenderer.h"
#include "TrimeshSliceRenderer2D.h"
#include "TrimeshSurfaceRenderer.h"

#include <JuceHeader.h>

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace CycleV2 {

enum class TrimeshExpandedHitRegionKind {
    MorphControl,
    PrimaryAxis,
    VertexParameter
};

struct TrimeshExpandedHitRegion {
    TrimeshExpandedHitRegionKind kind;
    juce::Rectangle<float> bounds;
    juce::String parameterId;
    juce::String axisValue;
};

class TrimeshWidget {
public:
    void syncFromNode(const Node& node);
    void setDisplayDomain(PortDomain domain);
    void setRenderProfile(TrimeshRenderProfile profile);

    void paintCompact(
            juce::Graphics& g,
            const Node& node,
            juce::Rectangle<float> area,
            float zoom,
            PortDomain domain);
    void paintCompact(
            juce::Graphics& g,
            const Node& node,
            juce::Rectangle<float> area,
            float zoom,
            const TrimeshRenderProfile& profile);

    void paintExpanded(
            juce::Graphics& g,
            const Node& node,
            juce::Rectangle<float> content);
    void renderExpandedPanelsOpenGL(
            const Node& node,
            juce::Rectangle<float> content,
            float scaleFactor);
    juce::Component* prepareExpandedPanel3DComponent(
            const Node& node,
            juce::Rectangle<float> content);
    juce::Component* getExpandedPanel3DComponentIfCreated();
    juce::Component* prepareExpandedPanel2DComponent(
            const Node& node,
            juce::Rectangle<float> content);
    juce::Component* getExpandedPanel2DComponentIfCreated();
    void releaseSharedGlResources();
    int resolvedSelectedVertexIndexForNode(const Node& node);
    void setExpandedPanelCallbacks(
            std::function<void()> repaintCallback,
            std::function<void(const juce::MouseCursor&)> cursorCallback,
            std::function<void(juce::Point<float>)> hoverCallback);
    static juce::Rectangle<float> expandedGridPanelContentBounds(juce::Rectangle<float> content);
    static juce::Rectangle<float> expandedWavePanelContentBounds(juce::Rectangle<float> content);
    static juce::Colour surfaceColourForDomain(float value, PortDomain domain);
    static juce::Colour surfaceColourForProfile(float value, const TrimeshRenderProfile& profile);

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
    std::vector<TrimeshExpandedHitRegion> expandedControlHitRegions(juce::Rectangle<float> content) const;

private:
    struct CachedHeatmap {
        juce::Image image;
        size_t valueCount {};
        int rows {};
        int columns {};
        uint64_t revision {};
        PortDomain domain { PortDomain::ControlSignal };
        RenderScalePolicy scalePolicy { RenderScalePolicy::Unipolar };
    };

    static juce::Rectangle<float> meshPreviewContentArea(juce::Rectangle<float> area);
    static void drawPanelFrame(
            juce::Graphics& g,
            juce::Rectangle<float> area,
            const juce::String& title,
            bool fillBody = true);
    static juce::Rectangle<float> morphPanelBounds(juce::Rectangle<float> content);
    static juce::Rectangle<float> morphRailBounds(juce::Rectangle<float> morphArea, int axisIndex);
    static juce::Rectangle<float> primaryAxisBounds(juce::Rectangle<float> morphArea, int axisIndex);
    static juce::String primaryAxisValue(int axis);
    static juce::Rectangle<float> vertexParameterPanelBounds(juce::Rectangle<float> content);
    static juce::Rectangle<float> vertexParameterRowBounds(juce::Rectangle<float> parameterArea, int parameterIndex);
    static juce::Rectangle<float> vertexParameterRailBounds(juce::Rectangle<float> parameterRow);
    static juce::String vertexParameterId(int parameterIndex);
    static juce::Rectangle<float> waveshapeContentBounds(juce::Rectangle<float> content);

    TrimeshPanelBridge bridge;
    CachedHeatmap compactHeatmap;
    TrimeshRenderProfile displayProfile { TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal) };
};

}
