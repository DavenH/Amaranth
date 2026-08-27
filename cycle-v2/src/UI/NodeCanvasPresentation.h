#pragma once

#include <JuceHeader.h>

#include "UI/NodeCanvasScene.h"
#include "UI/NodeCanvasGlRenderer.h"
#include "UI/NodeCanvasPresentationPerformanceObserver.h"
#include "UI/NodeCanvasViewport.h"
#include "UI/NodePalette.h"
#include "UI/NodePreviewRenderer.h"
#include "UI/GuideCurveShelf.h"
#include "UI/GuideRelationshipPresentation.h"
#include "UI/SignalProbeDetailView.h"
#include "UI/SignalProbeRail.h"
#include "Graph/GraphCompiler.h"
#include "Runtime/GraphPreviewExecutor.h"

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
    GuideCurveShelfState guideShelfState;
    float dockSplitRatio { 0.5f };
    SignalProbeRailState probeRailState;
    WorkspaceDockFocus dockFocus;
    SignalProbeDetailState probeDetailState;
    UnisonPreviewContext unisonPreviewContext;
};

struct NodePortPresentation {
    Rectangle<float> bounds;
    Point<float> centre;
};

class NodeCanvasPresentation {
public:
    NodeCanvasPresentation(
            NodeCanvasScene& sceneToUse,
            NodePreviewRenderer& previewRendererToUse,
            NodeCanvasPresentationPerformanceObserver* performanceObserver = nullptr);

    void paint(Graphics& graphics, const NodeCanvasPresentationFrame& frame);
    bool renderOpenGL(
            NodeCanvasRenderer& renderer,
            const NodeCanvasPresentationFrame& frame,
            float scaleFactor);

    static NodePortPresentation portPresentation(
            const NodeCanvasViewport& viewport,
            const Node& node,
            const Port& port);
    static UnisonPreviewContext unisonPreviewContextFor(
            const GraphExecutionPlan& plan,
            const String& unisonNodeId,
            UnisonPreviewContext fallback);
    static String canvasStatusText(
            const String& statusMessage,
            const String& hoverText);
    void paintStatus(Graphics& graphics, const NodeCanvasPresentationFrame& frame);
    bool guideShelfNeedsOpenGLPreviewRender() const;
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
    GuideCurveShelf guideCurveShelf;
    SignalProbeDetailView signalProbeDetailView;
    NodeCanvasPresentationPerformanceObserver* performanceObserver;
};

}
