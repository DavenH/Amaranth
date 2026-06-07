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

void Effect2DWidget::renderExpandedPanelOpenGL(
        const Node& node,
        Rectangle<float> bounds,
        float scaleFactor) {
    if (node.kind != kind) {
        return;
    }

    bridge.syncFromNode(node);
    bridge.renderPanel(bounds, scaleFactor);
}

void Effect2DWidget::releaseSharedGlResources() {
    bridge.releaseSharedGlResources();
}

}
