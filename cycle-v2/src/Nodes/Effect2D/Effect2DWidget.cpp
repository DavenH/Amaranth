#include "Effect2DWidget.h"

namespace CycleV2 {

Effect2DWidget::Effect2DWidget(NodeKind nodeKind) :
        kind    (nodeKind)
    ,   controller(createCurvePanelController(nodeKind)) {
    jassert(controller != nullptr);
}

Effect2DWidget::~Effect2DWidget() = default;

Component* Effect2DWidget::prepareExpandedPanelComponent(
        const Node& node,
        Rectangle<float> contentBounds) {
    ignoreUnused(contentBounds);

    if (node.kind != kind) {
        return nullptr;
    }

    controller->syncFromNode(node);
    return controller->panelHostComponent();
}

Component* Effect2DWidget::getExpandedPanelComponentIfCreated() {
    return controller->panelHostComponentIfCreated();
}

void Effect2DWidget::setDelegate(CurvePanelControllerDelegate* delegate) {
    controller->setDelegate(delegate);
}

void Effect2DWidget::setControlValues(
        bool enabled,
        float firstValue,
        float secondValue,
        float thirdValue,
        int menuId) {
    controller->setControlValues(enabled, firstValue, secondValue, thirdValue, menuId);
}

void Effect2DWidget::setEnvelopeLogarithmic(bool shouldUseLogarithmicScale) {
    if (auto* envelope = dynamic_cast<EnvelopeCurvePanelController*>(controller.get())) {
        envelope->setLogarithmic(shouldUseLogarithmicScale);
    }
}

void Effect2DWidget::setEnvelopeAxisLinks(bool redLinked, bool blueLinked) {
    if (auto* envelope = dynamic_cast<EnvelopeCurvePanelController*>(controller.get())) {
        envelope->setAxisLinks(redLinked, blueLinked);
    }
}

void Effect2DWidget::syncFromNode(const Node& node) {
    if (node.kind == kind) {
        controller->syncFromNode(node);
    }
}

void Effect2DWidget::renderExpandedPanelOpenGL(
        const Node& node,
        Rectangle<float> bounds,
        Rectangle<float> clipBounds,
        float scaleFactor) {
    if (node.kind != kind) {
        return;
    }

    controller->render(bounds, clipBounds, scaleFactor);
}

void Effect2DWidget::renderPreviewSnapshotOpenGL(
        const Node& node,
        Rectangle<float> bounds,
        float scaleFactor) {
    if (node.kind != kind) {
        return;
    }

    controller->renderPreview(bounds, scaleFactor);
}

bool Effect2DWidget::paintPreviewSnapshot(Graphics& g, Rectangle<float> bounds) const {
    return controller->paintPreviewSnapshot(g, bounds);
}

bool Effect2DWidget::paintExpandedSnapshot(Graphics& g, Rectangle<float> bounds) const {
    return controller->paintExpandedSnapshot(g, bounds);
}

void Effect2DWidget::releaseSharedGlResources() {
    controller->releaseSharedGlResources();
}

int Effect2DWidget::vertexCountForAutomation() const {
    return controller->vertexCountForAutomation();
}

var Effect2DWidget::automationState() const {
    return controller->automationState();
}

std::vector<CurvePreviewVertex> Effect2DWidget::previewVertices() {
    return controller->previewVertices();
}

String Effect2DWidget::serializedMeshState() {
    return controller->serializedMeshState();
}

String Effect2DWidget::serializedModelSnapshot() {
    return controller->serializedModelSnapshot();
}

String Effect2DWidget::prepareModelPublication(uint64_t currentRevision) {
    return controller->prepareModelPublication(currentRevision);
}

uint64_t Effect2DWidget::modelRevision() const {
    return controller->modelRevision();
}

uint64_t Effect2DWidget::contentRevision() const {
    return controller->contentRevision();
}

std::vector<TrimeshVertexParameter> Effect2DWidget::selectedVertexParameters() const {
    return controller->selectedVertexParameters();
}

bool Effect2DWidget::setSelectedVertexParameter(const String& parameterId, float normalizedValue) {
    return controller->setSelectedVertexParameter(parameterId, normalizedValue);
}

bool Effect2DWidget::selectedEnvelopeMarkerState(bool loopMarker) const {
    const auto* envelope = dynamic_cast<const EnvelopeCurvePanelController*>(controller.get());
    return envelope != nullptr && envelope->selectedMarkerState(loopMarker);
}

void Effect2DWidget::toggleSelectedEnvelopeMarker(bool loopMarker) {
    if (auto* envelope = dynamic_cast<EnvelopeCurvePanelController*>(controller.get())) {
        envelope->toggleSelectedMarker(loopMarker);
    }
}

}
