#pragma once

#include <JuceHeader.h>

#include <map>
#include <memory>

#include "SignalProbeRail.h"
#include "../Graph/NodeGraph.h"
#include "../Nodes/Effect2D/Effect2DWidget.h"

namespace CycleV2 {

struct GuideCurveShelfState {
    bool minimized {};
    float horizontalOffset {};
    String selectedGuideId;
};

class GuideCurveShelf {
public:
    static constexpr float minimizedWidth = 28.f;

    static Rectangle<float> guideWorkspace(
            Rectangle<float> workspace,
            float splitRatio,
            bool guidesMinimized = false,
            bool spiesMinimized = false);
    static Rectangle<float> spyWorkspace(
            Rectangle<float> workspace,
            float splitRatio,
            bool guidesMinimized = false,
            bool spiesMinimized = false);
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
private:
    Effect2DWidget& previewFor(const GuideCurveResource& guide) const;

    mutable std::map<String, std::unique_ptr<Effect2DWidget>> previews;
};

}
