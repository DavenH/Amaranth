#pragma once

#include <JuceHeader.h>

#include <map>
#include <memory>

#include "SignalProbeRail.h"
#include "WorkspaceDock.h"
#include "../Graph/NodeGraph.h"
#include "../Nodes/Effect2D/Effect2DWidget.h"

namespace CycleV2 {

struct GuideCurveShelfState {
    bool minimized {};
    float horizontalOffset {};
    String selectedGuideId;
    String hoveredGuideId;
};

class GuideCurveShelf {
public:
    static constexpr float minimizedWidth = WorkspaceDock::drawerWidth;

    static Colour colourForGuide(const GuideCurveResource& guide);

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
    static Rectangle<float> tileBoundsFor(
            Rectangle<float> workspace,
            const SignalProbeRailState& dockState,
            float splitRatio,
            const GuideCurveShelfState& state,
            int tileIndex);
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
    bool needsOpenGLPreviewRender() const;
    bool renderOpenGL(
            const NodeGraph& graph,
            Rectangle<float> workspace,
            Rectangle<float> captureWorkspace,
            const SignalProbeRailState& dockState,
            float splitRatio,
            const GuideCurveShelfState& state,
            float scaleFactor);

private:
    struct Preview {
        std::unique_ptr<Effect2DWidget> widget;
        NodeModelStatePtr model;
        bool enabled {};
        float noise {};
        float dcOffset {};
        float phase {};
        bool needsOpenGLRender {};
    };

    Preview& previewFor(const GuideCurveResource& guide) const;

    mutable std::map<String, Preview> previews;
};

}
