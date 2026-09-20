#include "UI/WorkspaceDockInteractionController.h"

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
                    probeState.expandedHeight
            });
    const bool hasSpies = true;
    if (hasSpies && handleChromeDown(event, layout)) {
        return true;
    }
    if (handleGuideDown(event, workspace)) {
        return true;
    }
    return hasSpies && handleSpyDown(event, workspace);
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
    return false;
}

bool WorkspaceDockInteractionController::mouseUp() {
    if (resizingHeight) {
        resizingHeight = false;
        settings.getGlobalSetting(AppSettings::GuideSpyDockHeight) =
                roundToInt(probeState.expandedHeight);
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
            probeState.horizontalOffset,
            *this);
}

void WorkspaceDockInteractionController::clearEphemeralState() {
    guideState.verticalOffset = 0.f;
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
            workspace, probeState, guideState);
    const Rectangle<float> spies = spyWorkspace(workspace);
    return {
            guides.getHeight(),
            SignalProbeRail::boundsFor(spies, probeState).getWidth(),
            GuideCurveShelf::maximumVerticalOffset(
                    workspace,
                    probeState,
                    guideState,
                    (int) graph.getGuideCurves().size()),
            SignalProbeRail::maximumHorizontalOffset(
                    spies,
                    (int) graph.getSignalProbes().size() + 1)
    };
}

Rectangle<float> WorkspaceDockInteractionController::spyWorkspace(
        Rectangle<float> workspace) const {
    return GuideCurveShelf::spyWorkspace(
            workspace,
            guideState.minimized,
            probeState.minimized);
}

bool WorkspaceDockInteractionController::handleChromeDown(
        const MouseEvent& event,
        const WorkspaceDockLayout& layout) {
    if (!probeState.expanded && layout.collapseHandle.contains(event.position)) {
        keyboardFocus = { WorkspaceDockFocusTarget::Collapse, {} };
        setDockExpandedFromKeyboard(true);
        return true;
    }
    if (layout.resizeHandle.contains(event.position)) {
        keyboardFocus = {};
        resizingHeight = true;
        resizeStartHeight = probeState.expandedHeight;
        resizeStartY = event.position.y;
        return true;
    }
    return false;
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
    const String probeId = probeRail.probeAt(event.position, spies, graph, probeState);
    if (probeId.isNotEmpty()) {
        keyboardFocus = { WorkspaceDockFocusTarget::SpyTile, probeId };
        probeState.selectedProbeId = probeId;
        if (probeId == DefaultOutputProbeResolver::probeId) {
            probeState.defaultOutputView = probeState.defaultOutputView == PresetPreviewView::Time
                    ? PresetPreviewView::Spectrum
                    : PresetPreviewView::Time;
        } else if (event.getNumberOfClicks() >= 2) {
            callbacks.openProbeDetail(probeId);
        }
        callbacks.repaint();
        return true;
    }
    return false;
}

void WorkspaceDockInteractionController::setDockExpandedFromKeyboard(bool expanded) {
    probeState.expanded = expanded;
    settings.getGlobalSetting(AppSettings::GuideSpyDockExpanded) = expanded;
    if (!expanded && probeDetailState.isOpen()) {
        probeDetailState.close();
        if (callbacks.occlusionChanged) {
            callbacks.occlusionChanged();
        }
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

void WorkspaceDockInteractionController::setSpyShelfMinimizedFromKeyboard(bool minimized) {
    probeState.minimized = minimized;
    settings.getGlobalSetting(AppSettings::SpyShelfMinimized) = minimized;
    keyboardFocus = {
            minimized
                    ? WorkspaceDockFocusTarget::SpyDrawer
                    : WorkspaceDockFocusTarget::SpyMinimize,
            {}
    };
    callbacks.repaint();
}

void WorkspaceDockInteractionController::setProbeRefreshMode(ProbeRefreshMode mode) {
    probeState.refreshMode = mode;
    settings.getGlobalSetting(AppSettings::ProbeEditRefreshPolicy) =
            probeState.refreshMode == ProbeRefreshMode::LiveLatest ? 1 : 0;
    callbacks.repaint();
}

void WorkspaceDockInteractionController::selectSpyFromKeyboard(
        const String& probeId,
        bool openDetail) {
    probeState.selectedProbeId = probeId;
    if (openDetail && probeId == DefaultOutputProbeResolver::probeId) {
        probeState.defaultOutputView = probeState.defaultOutputView == PresetPreviewView::Time
                ? PresetPreviewView::Spectrum
                : PresetPreviewView::Time;
    } else if (openDetail) {
        callbacks.openProbeDetail(probeId);
    }
}

void WorkspaceDockInteractionController::removeSpyFromKeyboard(const String& probeId) {
    if (probeId == DefaultOutputProbeResolver::probeId) {
        return;
    }
    callbacks.applyAuthoringResult(authoring.removeSignalProbe(probeId));
    if (probeDetailState.probeId == probeId) {
        probeDetailState.close();
        if (callbacks.occlusionChanged) {
            callbacks.occlusionChanged();
        }
    }
    probeState.selectedProbeId = {};
}

void WorkspaceDockInteractionController::repaintDockFromKeyboard() {
    callbacks.repaint();
}

}
