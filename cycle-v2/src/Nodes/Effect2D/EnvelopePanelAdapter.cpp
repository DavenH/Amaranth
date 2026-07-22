#include "EnvelopePanelAdapter.h"

#include "../Envelope/EnvelopeMeshState.h"

#include <Curve/Mesh/VertCube.h>
#include <Obj/MorphPosition.h>

#include <algorithm>

namespace CycleV2 {

EnvelopePanelAdapter::EnvelopePanelAdapter() = default;

bool EnvelopePanelAdapter::needsNodeSync(const Node& node) const {
    if (node.kind != NodeKind::Envelope) {
        return false;
    }
    return node.model != nullptr
            && node.model->schemaId() == "envelope"
            && (syncedNodeId != node.id || syncedModelRevision != node.model->revision());
}

bool EnvelopePanelAdapter::syncFromNode(const Node& node) {
    if (node.kind != NodeKind::Envelope) {
        return false;
    }
    if (!needsNodeSync(node) || !model.syncFromNode(node)) {
        return false;
    }
    syncedNodeId = node.id;
    syncedModelRevision = node.model->revision();
    model.selectCube((EnvelopeCubeId) (int64) node.editorState.getProperty("selectedCubeId", 0));
    model.setPublicationRevision(node.model->revision());
    syncedMeshState = serializedMeshState();
    return true;
}

void EnvelopePanelAdapter::initialiseDefaultMesh() {
    if (mesh().getNumVerts() == 0) {
        EnvelopeMeshState::apply(EnvelopeMeshState::defaultSnapshot(), mesh());
    }
}

String EnvelopePanelAdapter::serializedMeshState() {
    return EnvelopeMeshState::serialize(mesh());
}

NodeModelStatePtr EnvelopePanelAdapter::modelPublication(
        VertCube* selectedCube,
        uint64_t publicationRevision) {
    if (!model.synchronizeFromMesh(selectedCube)) {
        return {};
    }
    model.setPublicationRevision(publicationRevision);
    auto editor = std::make_unique<DynamicObject>();
    if (model.selectedCubeId().has_value()) {
        editor->setProperty("selectedCubeId", (int64) *model.selectedCubeId());
    }
    return std::make_shared<const CurveNodeModelState>(
            "envelope",
            EnvelopeNodeModel::currentVersion,
            publicationRevision,
            model.writeJSON(),
            var(editor.release()));
}

std::vector<CurvePreviewVertex> EnvelopePanelAdapter::previewVertices() {
    std::vector<CurvePreviewVertex> result;
    result.reserve((size_t) mesh().getNumCubes());
    MorphPosition position;
    position.time.setValueDirect(0.f);
    position.red.setValueDirect(model.red);
    position.blue.setValueDirect(model.blue);
    position.timeDepth = 0.001f;
    position.redDepth = 1.f;
    position.blueDepth = 1.f;

    for (VertCube* cube : mesh().getCubes()) {
        if (cube == nullptr) {
            continue;
        }
        VertCube::ReductionData reduction;
        cube->getFinalIntercept(reduction, position);
        if (reduction.pointOverlaps) {
            result.push_back({
                    reduction.v.values[Vertex::Phase],
                    reduction.v.values[Vertex::Amp],
                    reduction.v.values[Vertex::Curve]
            });
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.x < rhs.x;
    });
    return result;
}

bool EnvelopePanelAdapter::registerMeshEdit() {
    const String nextState = serializedMeshState();
    if (nextState == syncedMeshState) {
        return false;
    }
    syncedMeshState = nextState;
    return true;
}

void EnvelopePanelAdapter::setMorph(float red, float blue) {
    model.red = red;
    model.blue = blue;
}

void EnvelopePanelAdapter::setLogarithmic(bool logarithmicToUse) {
    model.logarithmic = logarithmicToUse;
}

void EnvelopePanelAdapter::setAxisLinks(bool redLinkedToUse, bool blueLinkedToUse) {
    model.redLinked = redLinkedToUse;
    model.blueLinked = blueLinkedToUse;
}

}
