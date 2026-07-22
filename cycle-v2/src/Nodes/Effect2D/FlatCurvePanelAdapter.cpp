#include "FlatCurvePanelAdapter.h"

#include "Effect2DMeshState.h"

#include <Curve/Mesh/Vertex.h>

#include <algorithm>

namespace CycleV2 {

namespace {

constexpr float kGuidePadding = 0.05f;
constexpr float kIrPadding = 0.0625f;
constexpr float kWaveshaperPadding = 0.125f;

}

FlatCurvePanelAdapter::FlatCurvePanelAdapter(NodeKind kindToUse) : nodeKind(kindToUse) {
    jassert(nodeKind == NodeKind::GuideCurve
            || nodeKind == NodeKind::ImpulseResponse
            || nodeKind == NodeKind::Waveshaper);
}

bool FlatCurvePanelAdapter::needsNodeSync(const Node& node) const {
    if (node.kind != nodeKind) {
        return false;
    }
    return node.model != nullptr
            && node.model->schemaId() == "flatCurve"
            && (syncedNodeId != node.id || syncedModelRevision != node.model->revision());
}

bool FlatCurvePanelAdapter::syncFromNode(const Node& node) {
    if (node.kind != nodeKind) {
        return false;
    }
    const auto typed = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
    if (!needsNodeSync(node) || typed == nullptr || typed->flatCurve() == nullptr
            || !model.copyFrom(*typed->flatCurve())) {
        return false;
    }
    syncedNodeId = node.id;
    syncedModelRevision = node.model->revision();
    model.selectVertex((uint64_t) (int64) node.editorState.getProperty("selectedVertexId", 0));
    model.setPublicationRevision(node.model->revision());
    syncedMeshState = serializedMeshState();
    return true;
}

void FlatCurvePanelAdapter::initialiseDefaultMesh() {
    if (mesh().getNumVerts() > 0) {
        return;
    }
    if (nodeKind == NodeKind::GuideCurve) {
        addVertex(kGuidePadding, 0.5f, 1.f);
        addVertex(0.34f, 0.64f, 0.4f);
        addVertex(0.62f, 0.36f, 0.4f);
        addVertex(1.f - kGuidePadding, 0.5f, 1.f);
    } else if (nodeKind == NodeKind::ImpulseResponse) {
        addVertex(kIrPadding * 0.5f, 0.5f);
        addVertex(kIrPadding - 0.001f, 0.5f);
        addVertex(kIrPadding + 0.001f, 0.5f);
        addVertex(kIrPadding + 0.003f, 0.5f);
        addVertex(kIrPadding + 0.005f, 1.0f);
        addVertex(kIrPadding + 0.010f, 0.1313f);
        addVertex(kIrPadding + 0.1f, 0.6f);
        addVertex(kIrPadding + 0.15f, 0.5f);
        addVertex(kIrPadding + 0.2f, 0.5f);
        addVertex(1.f, 0.5f);
    } else {
        addVertex(kWaveshaperPadding * 0.5f, kWaveshaperPadding * 0.5f, 1.f);
        addVertex(kWaveshaperPadding, kWaveshaperPadding, 1.f);
        addVertex(1.f - kWaveshaperPadding, 1.f - kWaveshaperPadding, 1.f);
        addVertex(1.f - kWaveshaperPadding * 0.5f, 1.f - kWaveshaperPadding * 0.5f, 1.f);
    }
}

String FlatCurvePanelAdapter::serializedMeshState() {
    std::vector<Effect2DVertexState> vertices;
    vertices.reserve((size_t) mesh().getNumVerts());
    for (const Vertex* vertex : mesh().getVerts()) {
        if (vertex != nullptr) {
            vertices.push_back({
                    vertex->values[Vertex::Phase],
                    vertex->values[Vertex::Amp],
                    vertex->values[Vertex::Curve]
            });
        }
    }
    return Effect2DMeshState::serialize(vertices);
}

NodeModelStatePtr FlatCurvePanelAdapter::modelPublication(
        Vertex* selectedVertex,
        uint64_t publicationRevision) {
    if (!model.synchronizeFromMesh(selectedVertex)) {
        return {};
    }
    model.setPublicationRevision(publicationRevision);
    auto editor = std::make_unique<DynamicObject>();
    if (model.selectedVertexId().has_value()) {
        editor->setProperty("selectedVertexId", (int64) *model.selectedVertexId());
    }
    return CurveNodeModelState::copyOf(
            model, publicationRevision, var(editor.release()));
}

std::vector<CurvePreviewVertex> FlatCurvePanelAdapter::previewVertices() {
    std::vector<CurvePreviewVertex> result;
    result.reserve((size_t) mesh().getNumVerts());
    for (const Vertex* vertex : mesh().getVerts()) {
        if (vertex != nullptr) {
            result.push_back({
                    vertex->values[Vertex::Phase],
                    vertex->values[Vertex::Amp],
                    vertex->values[Vertex::Curve]
            });
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.x < rhs.x;
    });
    return result;
}

bool FlatCurvePanelAdapter::registerMeshEdit() {
    const String nextState = serializedMeshState();
    if (nextState == syncedMeshState) {
        return false;
    }
    syncedMeshState = nextState;
    return true;
}

void FlatCurvePanelAdapter::addVertex(float x, float y, float curve) {
    auto* vertex = new Vertex(x, y);
    vertex->values[Vertex::Curve] = curve;
    if (curve >= 1.f) {
        vertex->setMaxSharpness();
    }
    mesh().addVertex(vertex);
}

}
