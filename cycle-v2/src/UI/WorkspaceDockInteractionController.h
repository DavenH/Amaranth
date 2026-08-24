#pragma once

#include <JuceHeader.h>

#include <functional>
#include <utility>

#include <App/Settings.h>

#include "GuideCurveShelf.h"
#include "NodeCanvasAuthoring.h"
#include "SignalProbeDetailView.h"
#include "WorkspaceDockKeyboardNavigation.h"
#include "../Graph/GraphCommandDispatcher.h"

namespace CycleV2 {

struct WorkspaceDockInteractionCallbacks {
    std::function<void(const String&)> openGuideEditor;
    std::function<void(const String&)> openProbeDetail;
    std::function<void(const NodeCanvasAuthoringResult&)> applyAuthoringResult;
    std::function<void()> repaint;
    std::function<void()> resized;
    std::function<void()> overlayChanged;
};

class WorkspaceDockInteractionController final :
        private WorkspaceDockKeyboardDelegate {
public:
    WorkspaceDockInteractionController(
            GraphCommandDispatcher& commands,
            NodeCanvasAuthoring& authoring,
            const NodeGraph& graph,
            Settings& settings,
            SignalProbeRail& probeRail,
            SignalProbeRailState& probeState,
            GuideCurveShelfState& guideState,
            SignalProbeDetailState& probeDetailState,
            float& splitRatio,
            String& statusMessage,
            WorkspaceDockInteractionCallbacks callbacks);

    bool mouseDown(const MouseEvent& event, Rectangle<float> workspace);
    bool mouseDrag(const MouseEvent& event, Rectangle<float> workspace);
    bool mouseUp();
    bool keyPressed(const KeyPress& key, Rectangle<float> workspace);

    const WorkspaceDockFocus& focus() const { return keyboardFocus; }
    void clearFocus() { keyboardFocus = {}; }
    void setFocus(WorkspaceDockFocus focusToUse) { keyboardFocus = std::move(focusToUse); }
    void clearEphemeralState();

private:
    WorkspaceDockKeyboardModel keyboardModel() const;
    WorkspaceDockKeyboardLayout keyboardLayout(Rectangle<float> workspace) const;
    Rectangle<float> spyWorkspace(Rectangle<float> workspace) const;
    bool handleChromeDown(
            const MouseEvent& event,
            const WorkspaceDockLayout& layout);
    bool handleGuideDown(const MouseEvent& event, Rectangle<float> workspace);
    bool handleGuideControlsDown(const MouseEvent& event, Rectangle<float> workspace);
    bool handleGuideTileDown(const MouseEvent& event, Rectangle<float> workspace);
    bool handleSpyDown(const MouseEvent& event, Rectangle<float> workspace);
    bool handleSpyControlsDown(const MouseEvent& event, Rectangle<float> workspace);
    bool handleSpyTileDown(const MouseEvent& event, Rectangle<float> workspace);

    void setDockExpandedFromKeyboard(bool expanded) override;
    void setGuideShelfMinimizedFromKeyboard(bool minimized) override;
    String createGuideFromKeyboard() override;
    void selectGuideFromKeyboard(const String& guideId, bool openEditor) override;
    void setSpyShelfMinimizedFromKeyboard(bool minimized) override;
    void toggleSpyRefreshFromKeyboard() override;
    void selectSpyFromKeyboard(const String& probeId, bool openDetail) override;
    void removeSpyFromKeyboard(const String& probeId) override;
    void repaintDockFromKeyboard() override;

    GraphCommandDispatcher& commands;
    NodeCanvasAuthoring& authoring;
    const NodeGraph& graph;
    Settings& settings;
    SignalProbeRail& probeRail;
    SignalProbeRailState& probeState;
    GuideCurveShelfState& guideState;
    SignalProbeDetailState& probeDetailState;
    float& splitRatio;
    String& statusMessage;
    WorkspaceDockInteractionCallbacks callbacks;
    WorkspaceDockFocus keyboardFocus;
    Rectangle<float> workspaceBounds;
    bool resizingHeight {};
    bool resizingSplit {};
    float resizeStartHeight {};
    float resizeStartY {};
};

}
