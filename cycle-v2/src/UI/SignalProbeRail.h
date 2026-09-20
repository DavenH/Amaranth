#pragma once

#include <JuceHeader.h>

#include "UI/NodeCanvasScene.h"
#include "UI/NodePreviewRenderer.h"
#include "UI/NodeCanvasPresentationPerformanceObserver.h"
#include "UI/SignalProbePreviewTileCache.h"
#include "UI/WorkspaceDock.h"
#include "Graph/DefaultOutputProbeResolver.h"
#include "Graph/PresetPresentation.h"
#include "Runtime/GraphPresentationFacts.h"
#include "Runtime/GraphPresentationSnapshot.h"
#include "Runtime/PresentationRefreshPolicy.h"

namespace CycleV2 {

struct SignalProbeRailState {
    bool expanded { true };
    bool minimized {};
    float expandedHeight { 190.f };
    float horizontalOffset {};
    String selectedProbeId;
    String hoveredProbeId;
    ProbeRefreshMode refreshMode { ProbeRefreshMode::OnGestureCommit };
    PresetPreviewView defaultOutputView { PresetPreviewView::Spectrum };
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
    static Rectangle<float> minimizeButtonBoundsFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& state);
    static Rectangle<float> tileBoundsFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& state,
            int tileIndex);
    static Rectangle<float> scrollAreaFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& state,
            int probeCount);
    static float maximumHorizontalOffset(Rectangle<float> workspace, int probeCount);
    static int ordinalForProbe(const NodeGraph& graph, const String& probeId);
    static std::vector<String> orderedProbeIds(const NodeGraph& graph);
    static Point<float> markerCentre(
            const SignalProbe& probe,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene);
    static float cableAnnotationDiameter(float zoom);

    static String probeAt(
            Point<float> position,
            Rectangle<float> workspace,
            const NodeGraph& graph,
            const SignalProbeRailState& state);
    String markerProbeAt(
            Point<float> position,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene) const;

    void paintCableAnnotations(
            Graphics& graphics,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene,
            const GraphPresentationFacts& facts,
            Rectangle<float> workspace,
            const SignalProbeRailState& state,
            float zoom) const;
    void paintRail(
            Graphics& graphics,
            const NodeGraph& graph,
            const GraphPresentationSnapshot& snapshot,
            const GraphPresentationFacts& facts,
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
            const NodeCanvasSceneSnapshot& scene,
            const GraphPresentationFacts& facts);
    void paintCachedPreview(
            Graphics& graphics,
            const NodeGraph& graph,
            const SignalProbe& probe,
            const GraphPreviewResult::SignalProbePreview& preview,
            const GraphPresentationFacts& facts,
            Rectangle<float> previewBounds,
            float physicalScale);

    NodePreviewRenderer& renderer;
    NodeCanvasPresentationPerformanceObserver* performanceObserver;
    SignalProbePreviewTileCache previewTileCache;
};

}
