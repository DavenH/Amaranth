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
    const String snapshot = parameterValueForNode(
            node, CurveNodeModelCodec::snapshotParameterId(), {});
    return snapshot.isNotEmpty()
            && (syncedNodeId != node.id || syncedModelSnapshot != snapshot);
}

bool EnvelopePanelAdapter::syncFromNode(const Node& node) {
    if (node.kind != NodeKind::Envelope) {
        return false;
    }
    const String snapshot = parameterValueForNode(
            node, CurveNodeModelCodec::snapshotParameterId(), {});
    if (!needsNodeSync(node) || !model.syncFromNode(node)) {
        return false;
    }
    syncedNodeId = node.id;
    syncedModelSnapshot = snapshot;
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

String EnvelopePanelAdapter::serializedModelSnapshot(
        VertCube* selectedCube,
        uint64_t publicationRevision) {
    if (!model.synchronizeFromMesh(selectedCube)) {
        return {};
    }
    model.setPublicationRevision(publicationRevision);
    syncedModelSnapshot = model.snapshot();
    return syncedModelSnapshot;
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
