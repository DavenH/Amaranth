#pragma once

#include <JuceHeader.h>

#include "NodeCanvasScene.h"
#include "NodeCanvasGlRenderer.h"
#include "NodeCanvasViewport.h"
#include "NodePalette.h"
#include "NodePreviewRenderer.h"
#include "SignalProbeRail.h"
#include "../Graph/GraphCompiler.h"
#include "../Runtime/GraphPreviewExecutor.h"

namespace CycleV2 {

struct PendingConnectionPresentation {
    PortAddress source;
    Point<float> pointer;
};

struct SnapGuidePresentation {
    bool hasX {};
    bool hasY {};
    float worldX {};
    float worldY {};
};

struct NodeCanvasPresentationFrame {
    const NodeGraph& graph;
    const GraphCompileResult& compileResult;
    const GraphPreviewResult& previewResult;
    const NodeCanvasViewport& viewport;
    const NodePalette& palette;
    Rectangle<float> canvasBounds;
    Rectangle<float> canvasOcclusion;
    Point<float> pointer;
    String selectedNodeId;
    String statusMessage;
    String hoverText;
    std::optional<PendingConnectionPresentation> pendingConnection;
    SnapGuidePresentation snapGuides;
    uint64_t presentationRevision {};
    uint64_t documentRevision {};
    int selectedEdgeIndex { -1 };
    int spliceTargetEdgeIndex { -1 };
    bool openGLUnderlay { true };
    Rectangle<float> workspaceBounds;
    SignalProbeRailState probeRailState;
};

struct NodePortPresentation {
    Rectangle<float> bounds;
    Point<float> centre;
};

class NodeCanvasPresentation {
public:
    NodeCanvasPresentation(
            NodeCanvasScene& sceneToUse,
            NodePreviewRenderer& previewRendererToUse);

    void paint(Graphics& graphics, const NodeCanvasPresentationFrame& frame);
    void renderOpenGL(
            NodeCanvasRenderer& renderer,
            const NodeCanvasPresentationFrame& frame,
            float scaleFactor);

    static NodePortPresentation portPresentation(
            const NodeCanvasViewport& viewport,
            const Node& node,
            const Port& port);
    SignalProbeRail& probeRail() { return signalProbeRail; }

private:
    void paintGrid(Graphics& graphics, const NodeCanvasPresentationFrame& frame);
    void paintContent(Graphics& graphics, const NodeCanvasPresentationFrame& frame);
    void paintEdges(Graphics& graphics, const NodeCanvasPresentationFrame& frame);
    void paintPendingConnection(Graphics& graphics, const NodeCanvasPresentationFrame& frame);
    void paintSnapGuides(Graphics& graphics, const NodeCanvasPresentationFrame& frame);
    void paintNodes(Graphics& graphics, const NodeCanvasPresentationFrame& frame);
    void paintNode(
            Graphics& graphics,
            const NodeCanvasPresentationFrame& frame,
            const Node& node);
    void paintMiniMap(Graphics& graphics, const NodeCanvasPresentationFrame& frame);
    void paintLegend(Graphics& graphics, const NodeCanvasPresentationFrame& frame);
    void paintHoverConsole(Graphics& graphics, const NodeCanvasPresentationFrame& frame);
    void paintPalette(Graphics& graphics, const NodeCanvasPresentationFrame& frame);

    void renderOpenGLEffectPreviews(
            const NodeCanvasPresentationFrame& frame,
            float scaleFactor);

    const NodePreviewResult* previewFor(
            const GraphPreviewResult& previews,
            const String& nodeId) const;
    TrimeshRenderProfile profileFor(const NodeCanvasPresentationFrame& frame, const Node& node) const;

    NodeCanvasScene& scene;
    NodePreviewRenderer& previewRenderer;
    SignalProbeRail signalProbeRail;
};

}
