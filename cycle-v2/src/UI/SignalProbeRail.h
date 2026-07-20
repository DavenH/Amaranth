#pragma once

#include <JuceHeader.h>

#include "NodeCanvasScene.h"
#include "NodePreviewRenderer.h"
#include "../Runtime/NodeUpdateGraph.h"

namespace CycleV2 {

struct SignalProbeRailState {
    bool expanded { true };
    float expandedHeight { 190.f };
    float horizontalOffset {};
    String selectedProbeId;
    String hoveredProbeId;
    ProbeRefreshMode refreshMode { ProbeRefreshMode::OnGestureCommit };
};

class SignalProbeRail {
public:
    explicit SignalProbeRail(NodePreviewRenderer& rendererToUse) : renderer(rendererToUse) {}

    static constexpr float collapsedHeight = 28.f;
    static constexpr float minimumExpandedHeight = 120.f;

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
    static Rectangle<float> tileBoundsFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& state,
            int tileIndex);
    static float maximumHorizontalOffset(Rectangle<float> workspace, int probeCount);

    String probeAt(
            Point<float> position,
            Rectangle<float> workspace,
            const NodeGraph& graph,
            const SignalProbeRailState& state) const;
    String closeProbeAt(
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
            const SignalProbeRailState& state);

private:
    static std::vector<const SignalProbe*> orderedProbes(const NodeGraph& graph);
    static const NodeSceneEdge* anchorFor(
            const SignalProbe& probe,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene);
    static Point<float> markerCentre(
            const SignalProbe& probe,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene);
    static Colour colourForProbe(
            const SignalProbe& probe,
            const NodeGraph& graph,
            const NodeCanvasSceneSnapshot& scene);
    static Rectangle<float> closeBounds(Rectangle<float> tile);
    const GraphPreviewResult::SignalProbePreview* previewFor(
            const GraphPreviewResult& previews,
            const String& probeId) const;

    NodePreviewRenderer& renderer;
};

}
