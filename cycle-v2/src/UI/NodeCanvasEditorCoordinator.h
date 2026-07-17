#pragma once

#include <JuceHeader.h>

#include "NodeEditorHost.h"
#include "NodePreviewRenderer.h"
#include "NodePreviewResources.h"
#include "TransformCompactEditor.h"
#include "VoiceContextCompactEditor.h"

namespace CycleV2 {

class GraphCommandDispatcher;
class GraphDocument;

struct NodeCanvasEditorState {
    String& expandedNodeId;
};

enum class ExpandedEditorClickKind {
    Unclaimed,
    Captured,
    Close,
    VoiceContextEdit,
    TransformMode
};

struct ExpandedEditorClick {
    ExpandedEditorClickKind kind { ExpandedEditorClickKind::Unclaimed };
    std::optional<VoiceContextEdit> voiceContextEdit;
    std::optional<CycleV2::TransformMode> transformMode;
};

class NodeCanvasEditorCoordinator final {
public:
    NodeCanvasEditorCoordinator(
            Component& owner,
            GraphDocument& document,
            NodeEditorCommandService& commands,
            NodeEditorPresentation& presentation,
            NodeEditorResources& editorResources,
            NodeCanvasEditorState state);
    ~NodeCanvasEditorCoordinator();

    NodePreviewResources& previewResources() { return resources; }
    NodePreviewRenderer& previewRenderer() { return renderer; }
    NodeEditorHost& host() { return editorHost; }

    static Rectangle<float> boundsFor(const Node* node, Rectangle<float> canvasBounds);
    static bool blocksCanvas(const Node* node);
    static ExpandedEditorClick routeClick(
            const Node* node,
            Rectangle<float> canvasBounds,
            Point<float> position);

    void updateHost(const Node* node);
    void renderOpenGL(float scaleFactor);
    void syncEffectNodes(const NodeGraph& graph);
    void releaseOpenGLResources();
    void clearPreviewCache();
    void close();
    void detach();
    std::array<String, 6> trimeshGuideLabelsFor(const Node& node);

private:
    Component& owner;
    GraphDocument& document;
    NodeEditorPresentation& presentation;
    NodeCanvasEditorState state;
    NodePreviewResources resources;
    NodePreviewRenderer renderer;
    NodeEditorHost editorHost;
};

}
