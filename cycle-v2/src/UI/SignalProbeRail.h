#pragma once

#include <JuceHeader.h>

#include "UI/NodeCanvasScene.h"
#include "UI/NodePreviewRenderer.h"
#include "UI/NodeCanvasPresentationPerformanceObserver.h"
#include "UI/SignalProbePreviewTileCache.h"
#include "UI/WorkspaceDock.h"
#include "Graph/GraphRenderSemanticResolver.h"
#include "Runtime/NodeUpdateGraph.h"

namespace CycleV2 {

struct SignalProbeRailState {
    bool expanded { true };
    bool minimized {};
    float expandedHeight { 190.f };
    float horizontalOffset {};
    String selectedProbeId;
    String hoveredProbeId;
    ProbeRefreshMode refreshMode { ProbeRefreshMode::OnGestureCommit };
};

class SignalProbeRail {
public:
    explicit SignalProbeRail(
            NodePreviewRenderer& rendererToUse,
            NodeCanvasPresentationPerformanceObserver* performanceObserverToUse = nullptr) :
            renderer(rendererToUse)
        ,   performanceObserver(performanceObserverToUse) {
    }

    static constexpr float collapsedHeight = WorkspaceDock::collapsedHeight;
    static constexpr float minimumExpandedHeight = WorkspaceDock::minimumExpandedHeight;

    static Rectangle<float> boundsFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& state);
    static Rectangle<float> contentBoundsFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& state);
    static Rectangle<float> resizeHandleFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& state);
    static Rectangle<float> collapseHandleFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& state);
    static Rectangle<float> refreshModeBoundsFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& state);
    static Rectangle<float> minimizeButtonBoundsFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& state);
    static Rectangle<float> tileBoundsFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& state,
            int tileIndex);
    static float maximumHorizontalOffset(Rectangle<float> workspace, int probeCount);
    static int ordinalForProbe(const NodeGraph& graph, const String& probeId);
    static std::vector<String> orderedProbeIds(const NodeGraph& graph);
    static NodeRenderSemantic renderSemanticForProbe(
            const NodeGraph& graph,
            const String& probeId);
    static Point<float> markerCentre(
            const SignalProbe& probe,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene);

    String probeAt(
            Point<float> position,
            Rectangle<float> workspace,
            const NodeGraph& graph,
            const SignalProbeRailState& state) const;
    String markerProbeAt(
            Point<float> position,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene) const;

    void paintCableAnnotations(
            Graphics& graphics,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene,
            Rectangle<float> workspace,
            const SignalProbeRailState& state) const;
    void paintRail(
            Graphics& graphics,
            const NodeGraph& graph,
            const GraphPreviewResult& previews,
            Rectangle<float> workspace,
            const SignalProbeRailState& state,
            const WorkspaceDockFocus& focus);
    void clearPreviewCache() { previewTileCache.clear(); }

private:
    static std::vector<const SignalProbe*> orderedProbes(const NodeGraph& graph);
    static const NodeSceneEdge* anchorFor(
            const SignalProbe& probe,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene);
    static Colour colourForProbe(
            const SignalProbe& probe,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene);
    const GraphPreviewResult::SignalProbePreview* previewFor(
            const GraphPreviewResult& previews,
            const String& probeId) const;
    void paintCachedPreview(
            Graphics& graphics,
            const NodeGraph& graph,
            const SignalProbe& probe,
            const GraphPreviewResult::SignalProbePreview& preview,
            Rectangle<float> previewBounds,
            float physicalScale);

    NodePreviewRenderer& renderer;
    NodeCanvasPresentationPerformanceObserver* performanceObserver;
    SignalProbePreviewTileCache previewTileCache;
};

}
