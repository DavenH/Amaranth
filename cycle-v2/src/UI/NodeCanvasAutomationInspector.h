#pragma once

#include <JuceHeader.h>

#include "../Graph/GraphDocument.h"
#include "../Runtime/GraphPresentationModel.h"
#include "NodeCanvasScene.h"
#include "NodeCanvasViewport.h"
#include "NodeEditorHost.h"

namespace CycleV2 {

struct NodeCanvasAutomationPresentation {
    juce::String selectedNodeId;
    juce::String expandedNodeId;
    juce::String editStatusMessage;
    int selectedEdgeIndex { -1 };
    ProbeRefreshMode probeRefreshMode { ProbeRefreshMode::OnGestureCommit };
    juce::Rectangle<float> probeRefreshModeBounds;
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
