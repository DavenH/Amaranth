#include "UI/WorkspaceDockInteractionController.h"

#include <utility>

namespace CycleV2 {

WorkspaceDockInteractionController::WorkspaceDockInteractionController(
        GraphCommandDispatcher& commandsToUse,
        const NodeGraph& graphToUse,
        Settings& settingsToUse,
        SignalProbeCanvasState& probeStateToUse,
        GuideCurveShelfState& guideStateToUse,
        String& statusMessageToUse,
        WorkspaceDockInteractionCallbacks callbacksToUse) :
        commands(commandsToUse)
    ,   graph(graphToUse)
    ,   settings(settingsToUse)
    ,   probeState(probeStateToUse)
    ,   guideState(guideStateToUse)
    ,   statusMessage(statusMessageToUse)
    ,   callbacks(std::move(callbacksToUse)) {
}

bool WorkspaceDockInteractionController::mouseDown(
        const MouseEvent& event,
        Rectangle<float> workspace) {
    workspaceBounds = workspace;
    if (handleGuideDown(event, workspace)) {
        return true;
    }
    return false;
}

bool WorkspaceDockInteractionController::keyPressed(
        const KeyPress& key,
        Rectangle<float> workspace) {
    workspaceBounds = workspace;
    return WorkspaceDockKeyboardNavigation::keyPressed(
            key,
            keyboardModel(),
            keyboardLayout(workspace),
            keyboardFocus,
            guideState.verticalOffset,
            *this);
}

void WorkspaceDockInteractionController::clearEphemeralState() {
    guideState.verticalOffset = 0.f;
    guideState.selectedGuideId = {};
    guideState.hoveredGuideId = {};
    probeState.selectedProbeId = {};
    probeState.hoveredProbeId = {};
    keyboardFocus = {};
}

WorkspaceDockKeyboardModel WorkspaceDockInteractionController::keyboardModel() const {
    WorkspaceDockKeyboardModel model;
    model.guidesMinimized = guideState.minimized;
    for (const auto& guide : graph.getGuideCurves()) {
        model.guideIds.push_back(guide.id);
    }
    return model;
}

WorkspaceDockKeyboardLayout WorkspaceDockInteractionController::keyboardLayout(
        Rectangle<float> workspace) const {
    const Rectangle<float> guides = GuideCurveShelf::boundsFor(
            workspace, probeState, guideState);
    return {
            guides.getHeight(),
            GuideCurveShelf::maximumVerticalOffset(
                    workspace, probeState, guideState,
                    (int) graph.getGuideCurves().size())
    };
}

bool WorkspaceDockInteractionController::handleGuideDown(
        const MouseEvent& event,
        Rectangle<float> workspace) {
    if (handleGuideControlsDown(event, workspace)) {
        return true;
    }
    return handleGuideTileDown(event, workspace);
}

void WorkspaceDockInteractionController::createGuide(Rectangle<float> workspace) {
    workspaceBounds = workspace;
    const String guideId = createGuideFromKeyboard();
    if (guideId.isNotEmpty()) {
        keyboardFocus = { WorkspaceDockFocusTarget::GuideTile, guideId };
    }
    callbacks.repaint();
}

bool WorkspaceDockInteractionController::handleGuideControlsDown(
        const MouseEvent& event,
        Rectangle<float> workspace) {
    const Rectangle<float> shelf = GuideCurveShelf::boundsFor(
            workspace, probeState, guideState);
    if (guideState.minimized && shelf.contains(event.position)) {
        keyboardFocus = { WorkspaceDockFocusTarget::GuideDrawer, {} };
        setGuideShelfMinimizedFromKeyboard(false);
        return true;
    }
    if (GuideCurveShelf::addButtonBounds(
                workspace, probeState, guideState).contains(event.position)) {
        keyboardFocus = { WorkspaceDockFocusTarget::GuideAdd, {} };
        const String guideId = createGuideFromKeyboard();
        if (guideId.isNotEmpty()) {
            keyboardFocus = { WorkspaceDockFocusTarget::GuideTile, guideId };
        }
        callbacks.repaint();
        return true;
    }
    if (GuideCurveShelf::minimizeButtonBounds(
                workspace, probeState, guideState).contains(event.position)) {
        keyboardFocus = { WorkspaceDockFocusTarget::GuideMinimize, {} };
        setGuideShelfMinimizedFromKeyboard(true);
        return true;
    }
    return false;
}

bool WorkspaceDockInteractionController::handleGuideTileDown(
        const MouseEvent& event,
        Rectangle<float> workspace) {
    const String guideToDelete = GuideCurveShelf::guideDeleteAt(
            event.position, graph, workspace, probeState, guideState);
    if (guideToDelete.isNotEmpty()) {
        callbacks.requestGuideDeletion(guideToDelete);
        return true;
    }
    const String guideId = GuideCurveShelf::guideAt(
            event.position, graph, workspace, probeState, guideState);
    if (guideId.isNotEmpty()) {
        keyboardFocus = { WorkspaceDockFocusTarget::GuideTile, guideId };
        guideState.selectedGuideId = guideId;
        if (event.getNumberOfClicks() >= 2) {
            callbacks.openGuideEditor(guideId);
        }
        callbacks.repaint();
        return true;
    }
    return false;
}

void WorkspaceDockInteractionController::setGuideShelfMinimizedFromKeyboard(bool minimized) {
    guideState.minimized = minimized;
    settings.getGlobalSetting(AppSettings::GuideShelfMinimized) = minimized;
    keyboardFocus = {
            minimized
                    ? WorkspaceDockFocusTarget::GuideDrawer
                    : WorkspaceDockFocusTarget::GuideMinimize,
            {}
    };
    callbacks.repaint();
}

String WorkspaceDockInteractionController::createGuideFromKeyboard() {
    const GraphEditResult result = commands.createGuideCurve();
    if (!result.succeeded()) {
        return {};
    }

    guideState.selectedGuideId = result.nodeId;
    guideState.verticalOffset = keyboardLayout(workspaceBounds).maximumGuideOffset;
    statusMessage = "Guide Curve created";
    return result.nodeId;
}

void WorkspaceDockInteractionController::selectGuideFromKeyboard(
        const String& guideId,
        bool openEditor) {
    guideState.selectedGuideId = guideId;
    if (openEditor) {
        callbacks.openGuideEditor(guideId);
    }
}

void WorkspaceDockInteractionController::removeGuideFromKeyboard(const String& guideId) {
    callbacks.requestGuideDeletion(guideId);
}

void WorkspaceDockInteractionController::setProbeRefreshMode(ProbeRefreshMode mode) {
    probeState.refreshMode = mode;
    settings.getGlobalSetting(AppSettings::ProbeEditRefreshPolicy) =
            probeState.refreshMode == ProbeRefreshMode::LiveLatest ? 1 : 0;
    callbacks.repaint();
}

void WorkspaceDockInteractionController::repaintDockFromKeyboard() {
    callbacks.repaint();
}

}
