#include "Effect2DWidget.h"

namespace CycleV2 {

Effect2DWidget::Effect2DWidget(NodeKind nodeKind) :
        kind    (nodeKind)
    ,   bridge  (nodeKind) {
}

Effect2DWidget::~Effect2DWidget() = default;

Component* Effect2DWidget::prepareExpandedPanelComponent(
        const Node& node,
        Rectangle<float> contentBounds) {
    ignoreUnused(contentBounds);

    if (node.kind != kind) {
        return nullptr;
    }

    bridge.syncFromNode(node);
    return bridge.getPanelHostComponent();
}

Component* Effect2DWidget::getExpandedPanelComponentIfCreated() {
    return bridge.getPanelHostComponentIfCreated();
}

void Effect2DWidget::setExpandedPanelCallbacks(
        std::function<void()> repaintCallback,
        std::function<void(const MouseCursor&)> cursorCallback) {
    bridge.setPanelHostCallbacks(std::move(repaintCallback), std::move(cursorCallback));
}

void Effect2DWidget::setMeshEditedCallback(std::function<void()> callback) {
    bridge.setMeshEditedCallback(std::move(callback));
}

void Effect2DWidget::setControlValues(
        bool enabled,
        float firstValue,
        float secondValue,
        float thirdValue,
        int menuId) {
    bridge.setControlValues(enabled, firstValue, secondValue, thirdValue, menuId);
}

void Effect2DWidget::setEnvelopeLogarithmic(bool shouldUseLogarithmicScale) {
    bridge.setEnvelopeLogarithmic(shouldUseLogarithmicScale);
}

void Effect2DWidget::setEnvelopeAxisLinks(bool redLinked, bool blueLinked) {
    bridge.setEnvelopeAxisLinks(redLinked, blueLinked);
}

void Effect2DWidget::renderExpandedPanelOpenGL(
        const Node& node,
        Rectangle<float> bounds,
        Rectangle<float> clipBounds,
        float scaleFactor) {
    if (node.kind != kind) {
        return;
    }

    bridge.syncFromNode(node);
    bridge.renderPanel(bounds, clipBounds, scaleFactor);
}

void Effect2DWidget::renderPreviewSnapshotOpenGL(
        const Node& node,
        Rectangle<float> bounds,
        float scaleFactor) {
    if (node.kind != kind) {
        return;
    }

    bridge.syncFromNode(node);
    bridge.renderPreviewSnapshot(bounds, scaleFactor);
}

bool Effect2DWidget::paintPreviewSnapshot(Graphics& g, Rectangle<float> bounds) const {
    return bridge.paintPreviewSnapshot(g, bounds);
}

bool Effect2DWidget::paintExpandedSnapshot(Graphics& g, Rectangle<float> bounds) const {
    return bridge.paintExpandedSnapshot(g, bounds);
}

void Effect2DWidget::releaseSharedGlResources() {
    bridge.releaseSharedGlResources();
}

int Effect2DWidget::vertexCountForAutomation() const {
    return bridge.vertexCountForAutomation();
}

var Effect2DWidget::automationState() const {
    return bridge.automationState();
}

std::vector<Effect2DPanelBridge::PreviewVertex> Effect2DWidget::previewVertices() {
    return bridge.previewVertices();
}

String Effect2DWidget::serializedMeshState() {
    return bridge.serializedMeshState();
}

std::vector<TrimeshVertexParameter> Effect2DWidget::selectedVertexParameters() const {
    return bridge.selectedVertexParameters();
}

bool Effect2DWidget::setSelectedVertexParameter(const String& parameterId, float normalizedValue) {
    return bridge.setSelectedVertexParameter(parameterId, normalizedValue);
}

bool Effect2DWidget::selectedEnvelopeMarkerState(bool loopMarker) const {
    return bridge.selectedEnvelopeMarkerState(loopMarker);
}

void Effect2DWidget::toggleSelectedEnvelopeMarker(bool loopMarker) {
    bridge.toggleSelectedEnvelopeMarker(loopMarker);
}

}
