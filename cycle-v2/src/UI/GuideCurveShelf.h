#pragma once

#include <JuceHeader.h>

#include "SignalProbeRail.h"
#include "../Graph/NodeGraph.h"

namespace CycleV2 {

struct GuideCurveShelfState {
    bool minimized {};
    float horizontalOffset {};
    String selectedGuideId;
};

class GuideCurveShelf {
public:
    static constexpr float minimizedWidth = 28.f;

    static Rectangle<float> guideWorkspace(Rectangle<float> workspace, float splitRatio);
    static Rectangle<float> spyWorkspace(Rectangle<float> workspace, float splitRatio);
    static Rectangle<float> boundsFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& dockState,
            float splitRatio,
            const GuideCurveShelfState& state);
    static Rectangle<float> addButtonBounds(
            Rectangle<float> workspace,
            const SignalProbeRailState& dockState,
            float splitRatio,
            const GuideCurveShelfState& state);
    static Rectangle<float> minimizeButtonBounds(
            Rectangle<float> workspace,
            const SignalProbeRailState& dockState,
            float splitRatio,
            const GuideCurveShelfState& state);
    static String guideAt(
            Point<float> position,
            const NodeGraph& graph,
            Rectangle<float> workspace,
            const SignalProbeRailState& dockState,
            float splitRatio,
            const GuideCurveShelfState& state);
    static float maximumHorizontalOffset(
            Rectangle<float> workspace,
            const SignalProbeRailState& dockState,
            float splitRatio,
            const GuideCurveShelfState& state,
            int guideCount);

    void paint(
            Graphics& graphics,
            const NodeGraph& graph,
            Rectangle<float> workspace,
            const SignalProbeRailState& dockState,
            float splitRatio,
            const GuideCurveShelfState& state) const;
};

}
