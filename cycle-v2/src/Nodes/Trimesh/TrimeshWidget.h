#pragma once

#include "TrimeshPanelBridge.h"
#include "TrimeshPanelHostDelegate.h"
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
    LinkToggle,
    VertexParameter,
    VertexGuideAttachment
};

struct TrimeshExpandedHitRegion {
    TrimeshExpandedHitRegionKind kind;
    juce::Rectangle<float> bounds;
    juce::String parameterId;
    juce::String axisValue;
};

struct TrimeshPanelRenderStats {
    int sampleCount {};
    int interceptCount {};
    float minimum {};
    float maximum {};
    float centreSample {};
    double absoluteSum {};
    std::vector<juce::Point<float>> intercepts;
};

class TrimeshWidget {
public:
    void syncFromNode(const Node& node);
    void setDisplayDomain(PortDomain domain);
    void setRenderProfile(TrimeshRenderProfile profile);
    void setGuideAttachmentLabels(std::array<juce::String, 6> labels);

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
    juce::Component* getExpandedPanel3DComponentIfCreated() const;
    juce::Component* prepareExpandedPanel2DComponent(
            const Node& node,
            juce::Rectangle<float> content);
    juce::Component* getExpandedPanel2DComponentIfCreated() const;
    void releaseSharedGlResources();
    int resolvedSelectedVertexIndexForNode(const Node& node);
    void setExpandedPanelHostDelegate(TrimeshPanelHostDelegate* delegate);
    void clearExpandedPanelHostDelegate(TrimeshPanelHostDelegate* delegate);
    void setMeshEditedCallback(std::function<void(TrimeshMeshEditEvent)> callback);
    juce::String currentMeshState();
    bool setVertexParameter(int vertexIndex, const juce::String& parameterId, float value);
    std::vector<TrimeshVertexParameter> vertexParametersForIndex(int vertexIndex);
    int selectedVertexIndexForPanel();
    std::vector<TrimeshVertexMarker> vertexMarkers();
    const TrimeshRenderData& renderDataForAutomation() const;
    TrimeshPanelRenderStats panelRenderStatsForAutomation() const;
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
    bool findLinkToggleAt(
            juce::Rectangle<float> content,
            juce::Point<float> position,
            juce::String& axisValue) const;
    bool findVertexParameterAt(
            juce::Rectangle<float> content,
            juce::Point<float> position,
            juce::String& parameterId,
            float& value) const;
    bool findVertexGuideAttachmentAt(
            juce::Rectangle<float> content,
            juce::Point<float> position,
            juce::String& parameterId) const;
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
    static std::vector<TrimeshExpandedHitRegion> expandedControlHitRegions(juce::Rectangle<float> content);

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
    static juce::Rectangle<float> linkToggleBounds(juce::Rectangle<float> morphArea, int axisIndex);
    static juce::String primaryAxisValue(int axis);
    static juce::Rectangle<float> vertexParameterPanelBounds(juce::Rectangle<float> content);
    static juce::Rectangle<float> vertexParameterRowBounds(juce::Rectangle<float> parameterArea, int parameterIndex);
    static juce::Rectangle<float> vertexParameterRailBounds(juce::Rectangle<float> parameterRow);
    static juce::String vertexParameterId(int parameterIndex);
    static juce::Rectangle<float> waveshapeContentBounds(juce::Rectangle<float> content);

    TrimeshPanelBridge bridge;
    CachedHeatmap compactHeatmap;
    TrimeshRenderProfile displayProfile { TrimeshRenderProfile::fromDomain(PortDomain::TimeSignal) };
    std::array<juce::String, 6> guideAttachmentLabels;
};

}
