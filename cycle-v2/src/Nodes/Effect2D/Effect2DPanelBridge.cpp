#include "Effect2DPanelBridge.h"

#include <UI/Panels/Panel.h>

namespace CycleV2 {

namespace {

bool isEffect2DNode(NodeKind kind) {
    return kind == NodeKind::Envelope
        || kind == NodeKind::GuideCurve
        || kind == NodeKind::ImpulseResponse
        || kind == NodeKind::Waveshaper;
}

}

Effect2DPanelBridge::Effect2DPanelBridge(NodeKind nodeKind) :
        kind    (nodeKind)
    ,   adapter (nodeKind == NodeKind::Envelope
                ? decltype(adapter)(std::in_place_type<EnvelopePanelAdapter>)
                : decltype(adapter)(std::in_place_type<FlatCurvePanelAdapter>, nodeKind)) {
    jassert(isEffect2DNode(kind));

    if (auto* envelope = envelopeAdapter()) {
        panel = createEnvelopeCurvePanel(
                &environment.services().getRepo(), environment.services(), envelope->mesh());
    } else {
        panel = createFlatCurvePanel(kind, &environment.services().getRepo(), adapterMesh());
    }
    host = std::make_unique<CurvePanelHost>(
            panel->hostedPanel(),
            [this](Component* component) {
                panel->initWithHost(component);
                panel->stopUpdates();
            },
            [this](bool resetView) { panel->updateZoomBounds(resetView); },
            [this] {
                applyPanelSettings();
                panel->refreshRasterizer();
            },
            [this] { return notifyMeshEdited(); },
            [this] { synchronizeModelSelection(); });

    if (kind != NodeKind::Envelope) {
        initialiseMesh();
    }
}

Effect2DPanelBridge::~Effect2DPanelBridge() {
    releaseSharedGlResources();
}

void Effect2DPanelBridge::syncFromNode(const Node& node) {
    if (node.kind != kind) {
        return;
    }
    const bool didSync = std::visit([&node](auto& typedAdapter) {
        return typedAdapter.syncFromNode(node);
    }, adapter);
    if (!didSync) {
        return;
    }

    std::visit([this](const auto& typedAdapter) {
        lastSyncedNodeId = typedAdapter.lastNodeId();
        lastSyncedModelSnapshot = typedAdapter.lastModelSnapshot();
        lastSyncedMeshState = typedAdapter.lastMeshState();
    }, adapter);
    publicationRevision = jmax<uint64_t>(1, (uint64_t) parameterValueForNode(
            node, CurveNodeModelCodec::revisionParameterId(), "1").getLargeIntValue());
    applyPanelSettings();
    panel->refreshRasterizer();
}

Component* Effect2DPanelBridge::getPanelHostComponent() {
    initialiseMesh();
    return host->component();
}

Component* Effect2DPanelBridge::getPanelHostComponentIfCreated() {
    return host->componentIfCreated();
}

void Effect2DPanelBridge::setPanelHostCallbacks(
        std::function<void()> repaintCallback,
        std::function<void(const MouseCursor&)> cursorCallback) {
    host->setCallbacks(std::move(repaintCallback), std::move(cursorCallback));
}

void Effect2DPanelBridge::setMeshEditedCallback(std::function<void()> callback) {
    meshEditedCallback = std::move(callback);
}

void Effect2DPanelBridge::setControlValues(
        bool enabled,
        float firstValue,
        float secondValue,
        float thirdValue,
        int menuId) {
    const ControlState next { enabled, firstValue, secondValue, thirdValue, menuId };
    const bool changed = controls.enabled != next.enabled
            || controls.first != next.first
            || controls.second != next.second
            || controls.third != next.third
            || controls.menuId != next.menuId;
    controls = next;
    if (auto* envelope = envelopeAdapter()) {
        envelope->setMorph(firstValue, secondValue);
    }
    if (changed) {
        ++publicationRevision;
    }
    applyPanelSettings();
}

void Effect2DPanelBridge::setEnvelopeLogarithmic(bool shouldUseLogarithmicScale) {
    auto* envelope = envelopeAdapter();
    if (envelope == nullptr) {
        return;
    }
    if (envelope->logarithmic() != shouldUseLogarithmicScale) {
        ++publicationRevision;
    }
    envelope->setLogarithmic(shouldUseLogarithmicScale);

    if (auto* typedPanel = envelopePanel()) {
        typedPanel->setEnvelopeLogarithmic(shouldUseLogarithmicScale);
    }
}

void Effect2DPanelBridge::setEnvelopeAxisLinks(bool redLinked, bool blueLinked) {
    auto* envelope = envelopeAdapter();
    if (envelope == nullptr) {
        return;
    }
    if (envelope->redLinked() != redLinked || envelope->blueLinked() != blueLinked) {
        ++publicationRevision;
    }
    envelope->setAxisLinks(redLinked, blueLinked);
    envelopePanel()->setEnvelopeAxisLinks(redLinked, blueLinked);
}

void Effect2DPanelBridge::renderPanel(
        Rectangle<float> bounds,
        Rectangle<float> clipBounds,
        float scaleFactor) {
    host->render(bounds, clipBounds, scaleFactor);
}

void Effect2DPanelBridge::renderPreviewSnapshot(Rectangle<float> bounds, float scaleFactor) {
    host->renderPreview(bounds, scaleFactor);
}

bool Effect2DPanelBridge::paintExpandedSnapshot(Graphics& g, Rectangle<float> bounds) const {
    return host->paintExpandedSnapshot(g, bounds);
}

bool Effect2DPanelBridge::paintPreviewSnapshot(Graphics& g, Rectangle<float> bounds) const {
    return host->paintPreviewSnapshot(g, bounds);
}

void Effect2DPanelBridge::initialiseSharedGlResources() {
    host->component();
}

void Effect2DPanelBridge::releaseSharedGlResources() {
    host->releaseSharedGlResources();
}

void Effect2DPanelBridge::applyPanelSettings() {
    if (panel != nullptr) {
        panel->setControlValues(
                controls.enabled,
                controls.first,
                controls.second,
                controls.third,
                controls.menuId);
        if (const auto* envelope = envelopeAdapter()) {
            envelopePanel()->setEnvelopeLogarithmic(envelope->logarithmic());
            envelopePanel()->setEnvelopeAxisLinks(envelope->redLinked(), envelope->blueLinked());
        }
    }
}

int Effect2DPanelBridge::vertexCountForAutomation() const {
    return adapterMesh().getNumVerts();
}

var Effect2DPanelBridge::automationState() const {
    var state = panel != nullptr ? panel->automationState() : var(new DynamicObject());
    if (auto* object = state.getDynamicObject()) {
        object->setProperty("modelRevision", (int64) publicationRevision);
        object->setProperty("domainModel", kind == NodeKind::Envelope ? "envelope" : "flatCurve");
    }
    return state;
}

std::vector<Effect2DPanelBridge::PreviewVertex> Effect2DPanelBridge::previewVertices() {
    return std::visit([](auto& typedAdapter) {
        return typedAdapter.previewVertices();
    }, adapter);
}

String Effect2DPanelBridge::serializedMeshState() {
    return std::visit([](auto& typedAdapter) {
        return typedAdapter.serializedMeshState();
    }, adapter);
}

String Effect2DPanelBridge::serializedModelSnapshot() {
    Vertex* selectedVertex = flatPanel() != nullptr ? flatPanel()->selectedFlatVertexForModel() : nullptr;
    VertCube* selectedCube = envelopePanel() != nullptr
            ? envelopePanel()->selectedEnvelopeCubeForModel()
            : nullptr;
    const String snapshot = flatAdapter() != nullptr
            ? flatAdapter()->serializedModelSnapshot(selectedVertex, publicationRevision)
            : envelopeAdapter()->serializedModelSnapshot(selectedCube, publicationRevision);
    if (snapshot.isEmpty() && panel != nullptr) {
        panel->clearInteractionState();
        panel->refreshRasterizer();
    }
    lastSyncedModelSnapshot = snapshot;
    return lastSyncedModelSnapshot;
}

String Effect2DPanelBridge::prepareModelPublication(uint64_t currentRevision) {
    publicationRevision = jmax(publicationRevision, currentRevision + 1);
    return serializedModelSnapshot();
}

std::vector<TrimeshVertexParameter> Effect2DPanelBridge::selectedVertexParameters() const {
    return panel != nullptr ? panel->selectedVertexParameters() : std::vector<TrimeshVertexParameter>();
}

bool Effect2DPanelBridge::setSelectedVertexParameter(const String& parameterId, float normalizedValue) {
    if (panel == nullptr || !panel->setSelectedVertexParameter(parameterId, normalizedValue)) {
        return false;
    }

    notifyMeshEdited();
    return true;
}

bool Effect2DPanelBridge::selectedEnvelopeMarkerState(bool loopMarker) const {
    return envelopePanel() != nullptr && envelopePanel()->selectedEnvelopeMarkerState(loopMarker);
}

void Effect2DPanelBridge::toggleSelectedEnvelopeMarker(bool loopMarker) {
    if (envelopePanel() == nullptr) {
        return;
    }

    envelopePanel()->toggleSelectedEnvelopeMarker(loopMarker);
    notifyMeshEdited();
}

void Effect2DPanelBridge::initialiseMesh() {
    std::visit([](auto& typedAdapter) {
        typedAdapter.initialiseDefaultMesh();
    }, adapter);

    panel->refreshRasterizer();

    if (kind == NodeKind::Envelope) {
        panel->updateZoomBounds(true);
    }
}

bool Effect2DPanelBridge::notifyMeshEdited() {
    if (meshEditedCallback == nullptr) {
        return false;
    }

    const bool changed = std::visit([](auto& typedAdapter) {
        return typedAdapter.registerMeshEdit();
    }, adapter);
    if (!changed) {
        return false;
    }

    std::visit([this](const auto& typedAdapter) {
        lastSyncedMeshState = typedAdapter.lastMeshState();
    }, adapter);
    ++publicationRevision;
    meshEditedCallback();
    return true;
}

void Effect2DPanelBridge::synchronizeModelSelection() {
    if (panel == nullptr) {
        return;
    }
    if (auto* flat = flatAdapter()) {
        flat->serializedModelSnapshot(
                flatPanel()->selectedFlatVertexForModel(), publicationRevision);
    } else {
        envelopeAdapter()->serializedModelSnapshot(
                envelopePanel()->selectedEnvelopeCubeForModel(), publicationRevision);
    }
}

Mesh& Effect2DPanelBridge::adapterMesh() {
    return std::visit([](auto& typedAdapter) -> Mesh& {
        return typedAdapter.mesh();
    }, adapter);
}

const Mesh& Effect2DPanelBridge::adapterMesh() const {
    return std::visit([](const auto& typedAdapter) -> const Mesh& {
        return typedAdapter.mesh();
    }, adapter);
}

FlatCurvePanelAdapter* Effect2DPanelBridge::flatAdapter() {
    return std::get_if<FlatCurvePanelAdapter>(&adapter);
}

const FlatCurvePanelAdapter* Effect2DPanelBridge::flatAdapter() const {
    return std::get_if<FlatCurvePanelAdapter>(&adapter);
}

EnvelopePanelAdapter* Effect2DPanelBridge::envelopeAdapter() {
    return std::get_if<EnvelopePanelAdapter>(&adapter);
}

const EnvelopePanelAdapter* Effect2DPanelBridge::envelopeAdapter() const {
    return std::get_if<EnvelopePanelAdapter>(&adapter);
}

FlatCurvePanelContract* Effect2DPanelBridge::flatPanel() {
    return dynamic_cast<FlatCurvePanelContract*>(panel.get());
}

const FlatCurvePanelContract* Effect2DPanelBridge::flatPanel() const {
    return dynamic_cast<const FlatCurvePanelContract*>(panel.get());
}

EnvelopeCurvePanelContract* Effect2DPanelBridge::envelopePanel() {
    return dynamic_cast<EnvelopeCurvePanelContract*>(panel.get());
}

const EnvelopeCurvePanelContract* Effect2DPanelBridge::envelopePanel() const {
    return dynamic_cast<const EnvelopeCurvePanelContract*>(panel.get());
}

}
