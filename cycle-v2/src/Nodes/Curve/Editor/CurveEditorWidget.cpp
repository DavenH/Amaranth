#include "Nodes/Curve/Editor/CurveEditorWidget.h"

#include "Nodes/Curve/Model/CurveNodeModels.h"

namespace CycleV2 {

CurveEditorWidget::CurveEditorWidget(NodeKind nodeKind) :
        kind    (nodeKind)
    ,   controller(createCurvePanelController(nodeKind)) {
    jassert(controller != nullptr);
}

CurveEditorWidget::CurveEditorWidget(bool shouldUseGuideResource) :
        kind    (NodeKind::GenericProcessor)
    ,   guideResource(shouldUseGuideResource)
    ,   controller(createGuideCurvePanelController()) {
    jassert(guideResource && controller != nullptr);
}

CurveEditorWidget::~CurveEditorWidget() = default;

Component* CurveEditorWidget::prepareExpandedPanelComponent(
        const Node& node,
        Rectangle<float> contentBounds) {
    ignoreUnused(contentBounds);

    if (!guideResource && node.kind != kind) {
        return nullptr;
    }

    syncFromNode(node);
    return controller->panelHostComponent();
}

Component* CurveEditorWidget::getExpandedPanelComponentIfCreated() {
    return controller->panelHostComponentIfCreated();
}

void CurveEditorWidget::setDelegate(CurvePanelControllerDelegate* delegate) {
    controller->setDelegate(delegate);
}

void CurveEditorWidget::setControlValues(
        bool enabled,
        float firstValue,
        float secondValue,
        float thirdValue,
        int menuId) {
    controller->setControlValues(enabled, firstValue, secondValue, thirdValue, menuId);
}

void CurveEditorWidget::setEnvelopeBipolar(bool bipolar) {
    if (auto* envelope = dynamic_cast<EnvelopeCurvePanelController*>(controller.get())) {
        envelope->setBipolar(bipolar);
    }
}

void CurveEditorWidget::setEnvelopeLogarithmic(bool shouldUseLogarithmicScale) {
    if (auto* envelope = dynamic_cast<EnvelopeCurvePanelController*>(controller.get())) {
        envelope->setLogarithmic(shouldUseLogarithmicScale);
    }
}

void CurveEditorWidget::setEnvelopeAxisLinks(bool redLinked, bool blueLinked) {
    if (auto* envelope = dynamic_cast<EnvelopeCurvePanelController*>(controller.get())) {
        envelope->setAxisLinks(redLinked, blueLinked);
    }
}

void CurveEditorWidget::fitEnvelopeVerticalRange() {
    if (auto* envelope = dynamic_cast<EnvelopeCurvePanelController*>(controller.get())) {
        envelope->fitVerticalRange();
    }
}

void CurveEditorWidget::resetEnvelopeVerticalRange() {
    if (auto* envelope = dynamic_cast<EnvelopeCurvePanelController*>(controller.get())) {
        envelope->resetVerticalRange();
    }
}

void CurveEditorWidget::setImpulseResponseAudioResource(
        const AudioSampleResource* resource) {
    if (auto* impulseResponse =
            dynamic_cast<ImpulseResponseCurvePanelController*>(controller.get())) {
        impulseResponse->setAudioResource(resource);
    }
}

void CurveEditorWidget::syncFromNode(const Node& node) {
    if (guideResource || node.kind != kind) {
        return;
    }

    controller->syncFromNode(node);
}

void CurveEditorWidget::syncFromGuideResource(const GuideCurveResource& guide) {
    if (!guideResource) {
        return;
    }
    controller->syncFromGuideResource(guide);
    controller->setControlValues(
            guide.enabled,
            guide.noise,
            guide.dcOffset,
            guide.phase,
            0);
}

void CurveEditorWidget::renderExpandedPanelOpenGL(
        const Node& node,
        Rectangle<float> bounds,
        Rectangle<float> clipBounds,
        float scaleFactor) {
    if (!guideResource && node.kind != kind) {
        return;
    }

    controller->render(bounds, clipBounds, scaleFactor);
}

void CurveEditorWidget::renderGuideExpandedPanelOpenGL(
        Rectangle<float> bounds,
        Rectangle<float> clipBounds,
        float scaleFactor) {
    if (!guideResource) {
        return;
    }

    controller->render(bounds, clipBounds, scaleFactor);
}

void CurveEditorWidget::renderGuidePreviewSnapshotOpenGL(
        Rectangle<float> bounds,
        float scaleFactor) {
    if (!guideResource) {
        return;
    }

    controller->renderPreview(bounds, scaleFactor);
}

void CurveEditorWidget::renderPreviewSnapshotOpenGL(
        const Node& node,
        Rectangle<float> bounds,
        float scaleFactor) {
    if (!guideResource && node.kind != kind) {
        return;
    }

    controller->renderPreview(bounds, scaleFactor);
}

bool CurveEditorWidget::paintPreviewSnapshot(Graphics& g, Rectangle<float> bounds) const {
    return controller->paintPreviewSnapshot(g, bounds);
}

bool CurveEditorWidget::paintExpandedSnapshot(Graphics& g, Rectangle<float> bounds) const {
    return controller->paintExpandedSnapshot(g, bounds);
}

void CurveEditorWidget::releaseSharedGlResources() {
    controller->releaseSharedGlResources();
}

int CurveEditorWidget::vertexCountForAutomation() const {
    return controller->vertexCountForAutomation();
}

var CurveEditorWidget::automationState() const {
    return controller->automationState();
}

std::vector<CurvePanelGridLine> CurveEditorWidget::verticalMajorGridLines() const {
    return controller->verticalMajorGridLines();
}

std::vector<CurvePreviewVertex> CurveEditorWidget::previewVertices() {
    return controller->previewVertices();
}

String CurveEditorWidget::serializedMeshState() {
    return controller->serializedMeshState();
}

NodeModelStatePtr CurveEditorWidget::modelPublication() {
    return controller->modelPublication();
}

NodeModelStatePtr CurveEditorWidget::prepareModelPublication(uint64_t currentRevision) {
    return controller->prepareModelPublication(currentRevision);
}

uint64_t CurveEditorWidget::modelRevision() const {
    return controller->modelRevision();
}

uint64_t CurveEditorWidget::contentRevision() const {
    return controller->contentRevision();
}

std::vector<TrimeshVertexParameter> CurveEditorWidget::selectedVertexParameters() const {
    return controller->selectedVertexParameters();
}

bool CurveEditorWidget::setSelectedVertexParameter(const String& parameterId, float normalizedValue) {
    return controller->setSelectedVertexParameter(parameterId, normalizedValue);
}

bool CurveEditorWidget::hasSingleSelectedEnvelopeVertex() {
    auto* envelope = dynamic_cast<EnvelopeCurvePanelController*>(controller.get());
    return envelope != nullptr && envelope->hasSingleSelectedVertex();
}

bool CurveEditorWidget::selectedEnvelopeMarkerState(bool loopMarker) const {
    const auto* envelope = dynamic_cast<const EnvelopeCurvePanelController*>(controller.get());
    return envelope != nullptr && envelope->selectedMarkerState(loopMarker);
}

void CurveEditorWidget::toggleSelectedEnvelopeMarker(bool loopMarker) {
    if (auto* envelope = dynamic_cast<EnvelopeCurvePanelController*>(controller.get())) {
        envelope->toggleSelectedMarker(loopMarker);
    }
}

}
