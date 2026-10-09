#pragma once

#include <JuceHeader.h>

#include <functional>
#include <utility>

#include <App/Settings.h>

#include "UI/GuideCurveShelf.h"
#include "UI/WorkspaceDockKeyboardNavigation.h"
#include "Graph/GraphCommandDispatcher.h"

namespace CycleV2 {

struct WorkspaceDockInteractionCallbacks {
    std::function<void(const String&)> openGuideEditor;
    std::function<void(const String&)> requestGuideDeletion;
    std::function<void()> repaint;
};

class WorkspaceDockInteractionController final :
        private WorkspaceDockKeyboardDelegate {
public:
    WorkspaceDockInteractionController(
            GraphCommandDispatcher& commands,
            const NodeGraph& graph,
            Settings& settings,
            SignalProbeCanvasState& probeState,
            GuideCurveShelfState& guideState,
            String& statusMessage,
            WorkspaceDockInteractionCallbacks callbacks);

    bool mouseDown(const MouseEvent& event, Rectangle<float> workspace);
    bool keyPressed(const KeyPress& key, Rectangle<float> workspace);
    void createGuide(Rectangle<float> workspace);

    const WorkspaceDockFocus& focus() const { return keyboardFocus; }
    void clearFocus() { keyboardFocus = {}; }
    void setFocus(WorkspaceDockFocus focusToUse) { keyboardFocus = std::move(focusToUse); }
    void clearEphemeralState();
    void setProbeRefreshMode(ProbeRefreshMode mode);

private:
    WorkspaceDockKeyboardModel keyboardModel() const;
    WorkspaceDockKeyboardLayout keyboardLayout(Rectangle<float> workspace) const;
    bool handleGuideDown(const MouseEvent& event, Rectangle<float> workspace);
    bool handleGuideControlsDown(const MouseEvent& event, Rectangle<float> workspace);
    bool handleGuideTileDown(const MouseEvent& event, Rectangle<float> workspace);

    void setGuideShelfMinimizedFromKeyboard(bool minimized) override;
    String createGuideFromKeyboard() override;
    void selectGuideFromKeyboard(const String& guideId, bool openEditor) override;
    void removeGuideFromKeyboard(const String& guideId) override;
    void repaintDockFromKeyboard() override;

    GraphCommandDispatcher& commands;
    const NodeGraph& graph;
    Settings& settings;
    SignalProbeCanvasState& probeState;
    GuideCurveShelfState& guideState;
    String& statusMessage;
    WorkspaceDockInteractionCallbacks callbacks;
    WorkspaceDockFocus keyboardFocus;
    Rectangle<float> workspaceBounds;
};

}
