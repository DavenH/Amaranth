#pragma once

#include <JuceHeader.h>

#include <vector>

#include "UI/NodeCanvasScene.h"
#include "UI/NodeCanvasViewport.h"
#include "UI/NodePreviewRenderer.h"
#include "UI/NodeCanvasPresentationPerformanceObserver.h"
#include "UI/SignalProbePreviewTileCache.h"
#include "Graph/DefaultOutputProbeResolver.h"
#include "Graph/PresetPresentation.h"
#include "Runtime/GraphPresentationFacts.h"
#include "Runtime/GraphPresentationSnapshot.h"
#include "Runtime/PresentationRefreshPolicy.h"

namespace CycleV2 {

struct SignalProbeCanvasState {
    String selectedProbeId;
    std::vector<String> selectedProbeIds;
    String hoveredProbeId;
    ProbeRefreshMode refreshMode { ProbeRefreshMode::OnGestureCommit };
    PresetPreviewView defaultOutputView { PresetPreviewView::Spectrum };
    bool outputSpyVisible { true };
    std::optional<Point<float>> outputCanvasPosition;
    std::vector<std::pair<String, Point<float>>> draggedCardWorldPositions;
    Point<float> draggedScreenOffset;

    bool isSelected(const String& probeId) const;
};

class SignalProbeCanvas {
public:
    explicit SignalProbeCanvas(
            NodePreviewRenderer& rendererToUse,
            NodeCanvasPresentationPerformanceObserver* performanceObserverToUse = nullptr) :
            renderer(rendererToUse)
        ,   performanceObserver(performanceObserverToUse) {
    }

    static int ordinalForProbe(const NodeGraph& graph, const String& probeId);
    static std::vector<String> orderedProbeIds(const NodeGraph& graph);
    static Point<float> markerCentre(
            const SignalProbe& probe,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene);
    static juce::Path tetherPath(
            const juce::Path& cable,
            float attachmentFraction,
            juce::Point<float> target);
    static float cableAnnotationDiameter(float zoom);
    static Rectangle<float> cardBoundsFor(
            const String& probeId,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene,
            const NodeCanvasViewport& viewport,
            const SignalProbeCanvasState& state);
    static String cardAt(
            Point<float> position,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene,
            const NodeCanvasViewport& viewport,
            const SignalProbeCanvasState& state);

    String markerProbeAt(
            Point<float> position,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene) const;

    void paintCableAnnotations(
            Graphics& graphics,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene,
            const GraphPresentationFacts& facts,
            const NodeCanvasViewport& viewport,
            const SignalProbeCanvasState& state,
            float zoom) const;
    void paintCards(
            Graphics& graphics,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene,
            const NodeCanvasViewport& viewport,
            const GraphPresentationSnapshot& snapshot,
            const GraphPresentationFacts& facts,
            const SignalProbeCanvasState& state);
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
