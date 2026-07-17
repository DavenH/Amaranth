#pragma once

#include <JuceHeader.h>

#include <optional>

#include "NodeCanvasAutomationInspector.h"
#include "NodeCanvasAuthoring.h"
#include "NodePreviewResources.h"

namespace CycleV2 {

struct NodeCanvasOpenGLDiagnosticsState {
    bool canvasAttached {};
    juce::String expandedNodeId;
};

struct NodeCanvasAutomationControllerContext {
    const juce::Component& canvas;
    const GraphDocument& document;
    const GraphPresentationModel& presentation;
    const NodeCanvasViewport& viewport;
    NodeCanvasAuthoring& authoring;
    const NodeEditorHost& editorHost;
    const NodePreviewResources& previewResources;
};

class NodeCanvasAutomationController {
public:
    explicit NodeCanvasAutomationController(NodeCanvasAutomationControllerContext context);

    NodeCanvasAuthoringResult openEditor(const juce::String& nodeId);
    NodeCanvasAuthoringResult addNode(const juce::String& kindId, juce::Point<float> position);
    NodeCanvasAuthoringResult moveNode(const juce::String& nodeId, juce::Point<float> position);
    NodeCanvasAuthoringResult connectPorts(
            const juce::String& sourceNodeId,
            const juce::String& sourcePortId,
            const juce::String& destNodeId,
            const juce::String& destPortId);
    NodeCanvasAuthoringResult deleteNode(const juce::String& nodeId);
    NodeCanvasAuthoringResult deleteEdge(int edgeIndex);
    NodeCanvasAuthoringResult setNodeParameter(
            const juce::String& nodeId,
            const juce::String& parameterId,
            const juce::String& label,
            const juce::String& value);
    NodeCanvasAuthoringResult setMorph(const juce::String& nodeId, const juce::String& axis, float value);
    NodeCanvasAuthoringResult setPrimaryAxis(const juce::String& nodeId, const juce::String& axis);
    NodeCanvasAuthoringResult toggleLink(const juce::String& nodeId, const juce::String& axis);
    NodeCanvasAuthoringResult selectVertex(const juce::String& nodeId, int vertexIndex);
    NodeCanvasAuthoringResult setVertexParameter(
            const juce::String& nodeId,
            const juce::String& parameterId,
            float value);
    bool getNodeParameter(
            const juce::String& nodeId,
            const juce::String& parameterId,
            juce::String& value) const;

    juce::var exportState(const NodeCanvasAutomationPresentation& state) const;
    juce::String exportGraphXml() const;
    juce::var inspectNodeControls(
            const juce::String& nodeId,
            const NodeCanvasAutomationPresentation& state) const;
    juce::var inspectPointerTargets(const NodeCanvasAutomationPresentation& state) const;
    juce::var inspectOpenGLDiagnostics(const NodeCanvasOpenGLDiagnosticsState& state) const;
    juce::var captureAudio(size_t frameCount) const;

    static std::optional<NodeKind> parseNodeKind(const juce::String& id);

private:
    NodeCanvasAutomationControllerContext context;
    NodeCanvasAutomationInspector inspector;
};

}
