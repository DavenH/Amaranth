#pragma once

#include <JuceHeader.h>

#include "../Graph/GraphDocument.h"
#include "../Runtime/GraphPresentationModel.h"
#include "NodeCanvasScene.h"
#include "NodeCanvasViewport.h"
#include "NodeEditorHost.h"

namespace CycleV2 {

struct GuideTileAutomationPresentation {
    juce::String guideId;
    juce::Rectangle<float> bounds;
};

struct GuideDockAutomationPresentation {
    bool expanded { true };
    bool guidesMinimized {};
    bool spiesMinimized {};
    float expandedHeight { 190.f };
    float splitRatio { 0.5f };
    float guideHorizontalOffset {};
    float spyHorizontalOffset {};
    juce::String selectedGuideId;
    juce::String expandedGuideId;
    juce::Rectangle<float> dockBounds;
    juce::Rectangle<float> guideShelfBounds;
    juce::Rectangle<float> spyShelfBounds;
    juce::Rectangle<float> dividerBounds;
    juce::Rectangle<float> collapseBounds;
    juce::Rectangle<float> resizeBounds;
    juce::Rectangle<float> guideMinimizeBounds;
    juce::Rectangle<float> spyMinimizeBounds;
    juce::Rectangle<float> addGuideBounds;
    juce::Rectangle<float> guideEditorBounds;
    std::vector<GuideTileAutomationPresentation> guideTiles;
};

struct NodeCanvasAutomationPresentation {
    juce::String selectedNodeId;
    juce::String expandedNodeId;
    juce::String editStatusMessage;
    int selectedEdgeIndex { -1 };
    double previewVoiceLengthSeconds { 1.0 };
    ProbeRefreshMode probeRefreshMode { ProbeRefreshMode::OnGestureCommit };
    juce::Rectangle<float> probeRefreshModeBounds;
    juce::String probeDetailId;
    size_t probeDetailResolution {};
    size_t probeDetailColumns {};
    size_t probeDetailRows {};
    juce::Rectangle<float> probeDetailBounds;
    juce::Rectangle<float> canvasContentBounds;
    GuideDockAutomationPresentation guideDock;
};

struct NodeCanvasAutomationContext {
    const juce::Component& canvas;
    const GraphDocument& document;
    const GraphPresentationModel& presentation;
    const NodeCanvasViewport& viewport;
    const NodeEditorHost& editorHost;
};

class NodeCanvasAutomationInspector {
public:
    explicit NodeCanvasAutomationInspector(NodeCanvasAutomationContext context);

    juce::var exportState(const NodeCanvasAutomationPresentation& state) const;
    juce::String exportGraphJson() const;
    juce::var inspectNodeControls(const juce::String& nodeId, const NodeCanvasAutomationPresentation& state) const;
    juce::var inspectPointerTargets(const NodeCanvasAutomationPresentation& state) const;
    juce::var captureAudio(size_t frameCount) const;

private:
    NodeCanvasAutomationContext context;
    mutable NodeCanvasScene scene;
};

}
