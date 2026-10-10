#include "UI/NodeCanvasGuideEditorCoordinator.h"

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphDocument.h"
#include "Nodes/Curve/Editor/CurveEditorWidget.h"

#include <utility>

namespace CycleV2 {

NodeCanvasGuideEditorCoordinator::NodeCanvasGuideEditorCoordinator(
        Component& targetOwner,
        GraphDocument& targetDocument,
        GraphCommandDispatcher& targetCommands,
        CurveExpandedEditorDelegate& targetDelegate,
        NodeCanvasGuideEditorCallbacks targetCallbacks) :
        owner(targetOwner)
    ,   document(targetDocument)
    ,   commands(targetCommands)
    ,   delegate(targetDelegate)
    ,   callbacks(std::move(targetCallbacks)) {
}

void NodeCanvasGuideEditorCoordinator::ensureEditor() {
    if (editor != nullptr) {
        return;
    }

    widget = std::make_unique<CurveEditorWidget>(true);
    editor = std::make_unique<GuideCurveEditorComponent>(*widget);
    editor->setDelegate(&delegate);
    editor->setHeatmapActions(callbacks.setHeatmap, callbacks.clearHeatmap);
    editor->setTitle("Guide Curve");
    owner.addAndMakeVisible(*editor);
}

void NodeCanvasGuideEditorCoordinator::open(
        const String& guideId,
        Rectangle<float> availableBounds) {
    const GuideCurveResource* guide = document.graph().findGuideCurve(guideId);
    if (guide == nullptr) {
        return;
    }

    ensureEditor();
    expandedGuideId = guideId;
    editor->setGuideResource(
            *guide,
            document.graph().guideHeatmapAsset(guide->heatmapAssetId));
    layout(availableBounds);
    editor->setVisible(true);
    editor->toFront(false);
    callbacks.notifyOcclusionChanged();
}

void NodeCanvasGuideEditorCoordinator::close() {
    if (transactionBaseRevision.has_value()) {
        commands.cancelTransientEdit();
        transactionBaseRevision.reset();
        callbacks.refreshCompiledState();
    }
    expandedGuideId = {};
    if (editor != nullptr) {
        editor->setVisible(false);
    }
    callbacks.notifyOcclusionChanged();
    callbacks.requestCanvasRepaint();
}

void NodeCanvasGuideEditorCoordinator::rebind() {
    if (!isOpen()) {
        return;
    }
    const GuideCurveResource* guide = document.graph().findGuideCurve(expandedGuideId);
    if (guide == nullptr) {
        close();
        return;
    }
    editor->setGuideResource(
            *guide,
            document.graph().guideHeatmapAsset(guide->heatmapAssetId));
}

void NodeCanvasGuideEditorCoordinator::layout(Rectangle<float> availableBounds) {
    if (isOpen()) {
        editor->setBounds(
                GuideCurveEditorComponent::preferredHostBounds(availableBounds).toNearestInt());
    }
}

void NodeCanvasGuideEditorCoordinator::renderOpenGL(float scaleFactor) {
    if (isOpen()) {
        editor->renderOpenGL(scaleFactor);
    }
}

void NodeCanvasGuideEditorCoordinator::releaseOpenGLResources() {
    if (widget != nullptr) {
        widget->releaseSharedGlResources();
    }
}

void NodeCanvasGuideEditorCoordinator::repaint() {
    if (editor != nullptr) {
        editor->repaint();
    }
}

bool NodeCanvasGuideEditorCoordinator::publishCurveState(
        NodeModelStatePtr model,
        const std::vector<NodeParameter>& controls,
        ProbeRefreshMode refreshMode) {
    if (expandedGuideId.isEmpty()) {
        return false;
    }
    const GuideCurveResource* guide = document.graph().findGuideCurve(expandedGuideId);
    if (guide == nullptr) {
        return false;
    }

    const GraphEditResult result = commands.publishGuideCurveState({
            expandedGuideId,
            transactionBaseRevision.value_or(guide->revision),
            std::move(model),
            controls
    });
    if (!result.succeeded()) {
        return false;
    }
    if (PresentationRefreshPolicy::schedulesDownstreamDuringMovement(refreshMode)) {
        callbacks.scheduleCompiledStateRefresh();
    }
    callbacks.requestCanvasRepaint();
    return true;
}

void NodeCanvasGuideEditorCoordinator::beginTransaction() {
    const GuideCurveResource* guide = document.graph().findGuideCurve(expandedGuideId);
    if (guide == nullptr || transactionBaseRevision.has_value()) {
        return;
    }
    transactionBaseRevision = guide->revision;
    commands.beginTransientEdit();
}

void NodeCanvasGuideEditorCoordinator::commitTransaction() {
    if (!transactionBaseRevision.has_value()) {
        return;
    }
    commands.commitTransientEdit();
    transactionBaseRevision.reset();
    callbacks.refreshCompiledState();
}

bool NodeCanvasGuideEditorCoordinator::isOpen() const {
    return editor != nullptr && editor->isVisible();
}

Rectangle<float> NodeCanvasGuideEditorCoordinator::bounds() const {
    return isOpen() ? editor->getBounds().toFloat() : Rectangle<float> {};
}

var NodeCanvasGuideEditorCoordinator::automationState() const {
    return isOpen() ? editor->automationState() : var();
}

std::vector<std::pair<String, Rectangle<float>>>
NodeCanvasGuideEditorCoordinator::automationPointerTargets() const {
    return isOpen()
            ? editor->automationPointerTargets()
            : std::vector<std::pair<String, Rectangle<float>>> {};
}

}
