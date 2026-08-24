#include "WorkspaceDockInteractionController.h"

#include <utility>

namespace CycleV2 {

WorkspaceDockInteractionController::WorkspaceDockInteractionController(
        GraphCommandDispatcher& commandsToUse,
        NodeCanvasAuthoring& authoringToUse,
        const NodeGraph& graphToUse,
        Settings& settingsToUse,
        SignalProbeRail& probeRailToUse,
        SignalProbeRailState& probeStateToUse,
        GuideCurveShelfState& guideStateToUse,
        SignalProbeDetailState& probeDetailStateToUse,
        float& splitRatioToUse,
        String& statusMessageToUse,
        WorkspaceDockInteractionCallbacks callbacksToUse) :
        commands(commandsToUse)
    ,   authoring(authoringToUse)
    ,   graph(graphToUse)
    ,   settings(settingsToUse)
    ,   probeRail(probeRailToUse)
    ,   probeState(probeStateToUse)
    ,   guideState(guideStateToUse)
    ,   probeDetailState(probeDetailStateToUse)
    ,   splitRatio(splitRatioToUse)
    ,   statusMessage(statusMessageToUse)
    ,   callbacks(std::move(callbacksToUse)) {
}

bool WorkspaceDockInteractionController::mouseDown(
        const MouseEvent& event,
        Rectangle<float> workspace) {
    workspaceBounds = workspace;
    const WorkspaceDockLayout layout = WorkspaceDock::layout(
            workspace,
            {
                    probeState.expanded,
                    guideState.minimized,
                    probeState.minimized,
                    probeState.expandedHeight,
                    splitRatio
            });
    if (handleChromeDown(event, layout)) {
        return true;
    }
    if (handleGuideDown(event, workspace)) {
        return true;
    }
    return handleSpyDown(event, workspace);
}

bool WorkspaceDockInteractionController::mouseDrag(
        const MouseEvent& event,
        Rectangle<float> workspace) {
    workspaceBounds = workspace;
    if (resizingHeight) {
        probeState.expandedHeight = jlimit(
                SignalProbeRail::minimumExpandedHeight,
                workspace.getHeight() * 0.4f,
                resizeStartHeight + resizeStartY - event.position.y);
        callbacks.resized();
        return true;
    }
    if (!resizingSplit) {
        return false;
    }

    splitRatio = WorkspaceDock::clampedSplitRatio(
            workspace,
            (event.position.x - workspace.getX()) / workspace.getWidth());
    callbacks.repaint();
    return true;
}

bool WorkspaceDockInteractionController::mouseUp() {
    if (resizingHeight) {
        resizingHeight = false;
        settings.getGlobalSetting(AppSettings::GuideSpyDockHeight) =
                roundToInt(probeState.expandedHeight);
        return true;
    }
    if (!resizingSplit) {
        return false;
    }

    resizingSplit = false;
    settings.getGlobalSetting(AppSettings::GuideDockSplitPercent) = roundToInt(splitRatio * 100.f);
    return true;
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
            guideState.horizontalOffset,
            probeState.horizontalOffset,
            *this);
}

void WorkspaceDockInteractionController::clearEphemeralState() {
    guideState.horizontalOffset = 0.f;
    guideState.selectedGuideId = {};
    guideState.hoveredGuideId = {};
    probeState.horizontalOffset = 0.f;
    probeState.selectedProbeId = {};
    probeState.hoveredProbeId = {};
    keyboardFocus = {};
}

WorkspaceDockKeyboardModel WorkspaceDockInteractionController::keyboardModel() const {
    WorkspaceDockKeyboardModel model;
    model.expanded = probeState.expanded;
    model.guidesMinimized = guideState.minimized;
    model.spiesMinimized = probeState.minimized;
    for (const auto& guide : graph.getGuideCurves()) {
        model.guideIds.push_back(guide.id);
    }
    model.spyIds = SignalProbeRail::orderedProbeIds(graph);
    return model;
}

WorkspaceDockKeyboardLayout WorkspaceDockInteractionController::keyboardLayout(
        Rectangle<float> workspace) const {
    const Rectangle<float> guides = GuideCurveShelf::boundsFor(
            workspace, probeState, splitRatio, guideState);
    const Rectangle<float> spies = spyWorkspace(workspace);
    return {
            guides.getWidth(),
            SignalProbeRail::boundsFor(spies, probeState).getWidth(),
            GuideCurveShelf::maximumHorizontalOffset(
                    workspace,
                    probeState,
                    splitRatio,
                    guideState,
                    (int) graph.getGuideCurves().size()),
            SignalProbeRail::maximumHorizontalOffset(
                    spies,
                    (int) graph.getSignalProbes().size())
    };
}

Rectangle<float> WorkspaceDockInteractionController::spyWorkspace(
        Rectangle<float> workspace) const {
    return GuideCurveShelf::spyWorkspace(
            workspace,
            splitRatio,
            guideState.minimized,
            probeState.minimized);
}

bool WorkspaceDockInteractionController::handleChromeDown(
        const MouseEvent& event,
        const WorkspaceDockLayout& layout) {
    if (!probeState.expanded && layout.dock.contains(event.position)) {
        keyboardFocus = { WorkspaceDockFocusTarget::Collapse, {} };
        setDockExpandedFromKeyboard(true);
        return true;
    }
    if (layout.collapseHandle.contains(event.position)) {
        keyboardFocus = { WorkspaceDockFocusTarget::Collapse, {} };
        setDockExpandedFromKeyboard(false);
        return true;
    }
    if (layout.resizeHandle.contains(event.position)) {
        keyboardFocus = {};
        resizingHeight = true;
        resizeStartHeight = probeState.expandedHeight;
        resizeStartY = event.position.y;
        return true;
    }
    if (!layout.divider.contains(event.position)) {
        return false;
    }

    keyboardFocus = {};
    resizingSplit = true;
    return true;
}

bool WorkspaceDockInteractionController::handleGuideDown(
        const MouseEvent& event,
        Rectangle<float> workspace) {
    if (handleGuideControlsDown(event, workspace)) {
        return true;
    }
    return handleGuideTileDown(event, workspace);
}

bool WorkspaceDockInteractionController::handleGuideControlsDown(
        const MouseEvent& event,
        Rectangle<float> workspace) {
    const Rectangle<float> shelf = GuideCurveShelf::boundsFor(
            workspace, probeState, splitRatio, guideState);
    if (guideState.minimized && shelf.contains(event.position)) {
        keyboardFocus = { WorkspaceDockFocusTarget::GuideDrawer, {} };
        setGuideShelfMinimizedFromKeyboard(false);
        return true;
    }
    if (GuideCurveShelf::addButtonBounds(
                workspace, probeState, splitRatio, guideState).contains(event.position)) {
        keyboardFocus = { WorkspaceDockFocusTarget::GuideAdd, {} };
        const String guideId = createGuideFromKeyboard();
        if (guideId.isNotEmpty()) {
            keyboardFocus = { WorkspaceDockFocusTarget::GuideTile, guideId };
        }
        callbacks.repaint();
        return true;
    }
    if (GuideCurveShelf::minimizeButtonBounds(
                workspace, probeState, splitRatio, guideState).contains(event.position)) {
        keyboardFocus = { WorkspaceDockFocusTarget::GuideMinimize, {} };
        setGuideShelfMinimizedFromKeyboard(true);
        return true;
    }
    return false;
}

bool WorkspaceDockInteractionController::handleGuideTileDown(
        const MouseEvent& event,
        Rectangle<float> workspace) {
    const Rectangle<float> shelf = GuideCurveShelf::boundsFor(
            workspace, probeState, splitRatio, guideState);
    const String guideId = GuideCurveShelf::guideAt(
            event.position, graph, workspace, probeState, splitRatio, guideState);
    if (guideId.isNotEmpty()) {
        keyboardFocus = { WorkspaceDockFocusTarget::GuideTile, guideId };
        guideState.selectedGuideId = guideId;
        if (event.getNumberOfClicks() >= 2) {
            callbacks.openGuideEditor(guideId);
        }
        callbacks.repaint();
        return true;
    }
    if (!shelf.contains(event.position)) {
        return false;
    }

    keyboardFocus = {};
    callbacks.repaint();
    return true;
}

bool WorkspaceDockInteractionController::handleSpyDown(
        const MouseEvent& event,
        Rectangle<float> workspace) {
    if (handleSpyControlsDown(event, workspace)) {
        return true;
    }
    return handleSpyTileDown(event, workspace);
}

bool WorkspaceDockInteractionController::handleSpyControlsDown(
        const MouseEvent& event,
        Rectangle<float> workspace) {
    const Rectangle<float> spies = spyWorkspace(workspace);
    const Rectangle<float> shelf = SignalProbeRail::boundsFor(spies, probeState);
    if (probeState.minimized && shelf.contains(event.position)) {
        keyboardFocus = { WorkspaceDockFocusTarget::SpyDrawer, {} };
        setSpyShelfMinimizedFromKeyboard(false);
        return true;
    }
    if (probeRail.refreshModeBoundsFor(spies, probeState).contains(event.position)) {
        keyboardFocus = { WorkspaceDockFocusTarget::SpyRefresh, {} };
        toggleSpyRefreshFromKeyboard();
        callbacks.repaint();
        return true;
    }
    if (probeRail.minimizeButtonBoundsFor(spies, probeState).contains(event.position)) {
        keyboardFocus = { WorkspaceDockFocusTarget::SpyMinimize, {} };
        setSpyShelfMinimizedFromKeyboard(true);
        return true;
    }
    return false;
}

bool WorkspaceDockInteractionController::handleSpyTileDown(
        const MouseEvent& event,
        Rectangle<float> workspace) {
    const Rectangle<float> spies = spyWorkspace(workspace);
    const Rectangle<float> shelf = SignalProbeRail::boundsFor(spies, probeState);
    const String closeProbe = probeRail.closeProbeAt(event.position, spies, graph, probeState);
    if (closeProbe.isNotEmpty()) {
        keyboardFocus = { WorkspaceDockFocusTarget::SpyRemove, closeProbe };
        removeSpyFromKeyboard(closeProbe);
        return true;
    }
    const String probeId = probeRail.probeAt(event.position, spies, graph, probeState);
    if (probeId.isNotEmpty()) {
        keyboardFocus = { WorkspaceDockFocusTarget::SpyTile, probeId };
        probeState.selectedProbeId = probeId;
        if (event.getNumberOfClicks() >= 2) {
            callbacks.openProbeDetail(probeId);
        }
        callbacks.repaint();
        return true;
    }
    if (!shelf.contains(event.position)) {
        return false;
    }

    keyboardFocus = {};
    probeState.selectedProbeId = {};
    callbacks.repaint();
    return true;
}

void WorkspaceDockInteractionController::setDockExpandedFromKeyboard(bool expanded) {
    probeState.expanded = expanded;
    settings.getGlobalSetting(AppSettings::GuideSpyDockExpanded) = expanded;
    if (!expanded && probeDetailState.isOpen()) {
        probeDetailState.close();
        callbacks.overlayChanged();
    }
    callbacks.resized();
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
    if (minimized && probeState.minimized) {
        keyboardFocus = { WorkspaceDockFocusTarget::Collapse, {} };
        setDockExpandedFromKeyboard(false);
    }
    callbacks.repaint();
}

String WorkspaceDockInteractionController::createGuideFromKeyboard() {
    const GraphEditResult result = commands.createGuideCurve();
    if (!result.succeeded()) {
        return {};
    }

    guideState.selectedGuideId = result.nodeId;
    guideState.horizontalOffset = keyboardLayout(workspaceBounds).maximumGuideOffset;
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

void WorkspaceDockInteractionController::setSpyShelfMinimizedFromKeyboard(bool minimized) {
    probeState.minimized = minimized;
    settings.getGlobalSetting(AppSettings::SpyShelfMinimized) = minimized;
    keyboardFocus = {
            minimized
                    ? WorkspaceDockFocusTarget::SpyDrawer
                    : WorkspaceDockFocusTarget::SpyMinimize,
            {}
    };
    if (minimized && guideState.minimized) {
        keyboardFocus = { WorkspaceDockFocusTarget::Collapse, {} };
        setDockExpandedFromKeyboard(false);
    }
    callbacks.repaint();
}

void WorkspaceDockInteractionController::toggleSpyRefreshFromKeyboard() {
    probeState.refreshMode = probeState.refreshMode == ProbeRefreshMode::LiveLatest
            ? ProbeRefreshMode::OnGestureCommit
            : ProbeRefreshMode::LiveLatest;
    settings.getGlobalSetting(AppSettings::ProbeEditRefreshPolicy) =
            probeState.refreshMode == ProbeRefreshMode::LiveLatest ? 1 : 0;
}

void WorkspaceDockInteractionController::selectSpyFromKeyboard(
        const String& probeId,
        bool openDetail) {
    probeState.selectedProbeId = probeId;
    if (openDetail) {
        callbacks.openProbeDetail(probeId);
    }
}

void WorkspaceDockInteractionController::removeSpyFromKeyboard(const String& probeId) {
    callbacks.applyAuthoringResult(authoring.removeSignalProbe(probeId));
    if (probeDetailState.probeId == probeId) {
        probeDetailState.close();
        callbacks.overlayChanged();
    }
    probeState.selectedProbeId = {};
}

void WorkspaceDockInteractionController::repaintDockFromKeyboard() {
    callbacks.repaint();
}

}
