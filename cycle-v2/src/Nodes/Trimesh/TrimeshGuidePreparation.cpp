#include "TrimeshGuidePreparation.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Mesh/VertCube.h>

#include <algorithm>
#include <unordered_map>

#include "TrimeshGuideAttachmentTarget.h"

namespace CycleV2 {

namespace {

struct StringHash {
    size_t operator()(const String& value) const {
        return (size_t) value.hashCode64();
    }
};

std::shared_ptr<Mesh> copyMesh() {
    return std::shared_ptr<Mesh>(new Mesh(), [](Mesh* mesh) {
        mesh->destroy();
        delete mesh;
    });
}

bool isGuideAttachment(const Edge& edge, const String& trimeshNodeId) {
    return edge.destNodeId == trimeshNodeId
            && edge.isProcessingAttachment()
            && edge.attachmentType == AttachmentType::GuideCurve;
}

int vertexDimension(const String& field) {
    if (field == "time") {
        return Vertex::Time;
    }
    if (field == "red") {
        return Vertex::Red;
    }
    if (field == "blue") {
        return Vertex::Blue;
    }
    if (field == "phase") {
        return Vertex::Phase;
    }
    if (field == "amp") {
        return Vertex::Amp;
    }
    if (field == "curve") {
        return Vertex::Curve;
    }

    return -1;
}

void clearGuideAssignments(Mesh& mesh) {
    for (auto* cube : mesh.getCubes()) {
        if (cube == nullptr) {
            continue;
        }

        for (int dimension = 0; dimension < Vertex::numElements; ++dimension) {
            cube->guideCurveAt(dimension) = -1;
        }
    }
}

void preserveComponentCurveSharpness(VertCube& cube) {
    for (Vertex* vertex : cube.lineVerts) {
        if (vertex != nullptr && vertex->values[Vertex::Curve] > 0.01f) {
            return;
        }
    }

    for (Vertex* vertex : cube.lineVerts) {
        if (vertex != nullptr) {
            vertex->setMaxSharpness();
        }
    }
}

bool assignCube(VertCube* cube, int dimension, int guideSlot) {
    if (cube == nullptr || !isPositiveAndBelow(dimension, Vertex::numElements)) {
        return false;
    }

    cube->guideCurveAt(dimension) = (char) guideSlot;
    if (dimension == Vertex::Time) {
        preserveComponentCurveSharpness(*cube);
    }
    return true;
}

size_t applyTarget(
        Mesh& mesh,
        const TrimeshGuideAttachmentTarget& target,
        int guideSlot) {
    const int dimension = vertexDimension(target.field);
    if (!isPositiveAndBelow(target.cubeIndex, mesh.getNumCubes())) {
        return 0;
    }
    return assignCube(mesh.getCubes()[(size_t) target.cubeIndex], dimension, guideSlot)
            ? 1u
            : 0u;
}

}

PreparedTrimeshGuides TrimeshGuidePreparation::prepare(
        const NodeGraph& graph,
        const Node& trimeshNode,
        const Mesh& sourceMesh) {
    PreparedTrimeshGuides result;
    result.mesh = copyMesh();
    result.mesh->deepCopy(&sourceMesh);
    result.provider = std::make_shared<GuideCurveSnapshotProvider>();
    clearGuideAssignments(*result.mesh);

    std::unordered_map<String, int, StringHash> slots;
    for (const auto& node : graph.getNodes()) {
        if (node.kind != NodeKind::GuideCurve) {
            continue;
        }

        const bool attached = std::any_of(
                graph.getEdges().begin(),
                graph.getEdges().end(),
                [&](const Edge& edge) {
                    return isGuideAttachment(edge, trimeshNode.id)
                            && edge.sourceNodeId == node.id;
                });
        if (!attached || !result.provider->addGuide(node)) {
            continue;
        }

        slots.emplace(node.id, result.provider->size() - 1);
    }

    for (const auto& edge : graph.getEdges()) {
        if (!isGuideAttachment(edge, trimeshNode.id)) {
            continue;
        }

        const auto slot = slots.find(edge.sourceNodeId);
        if (slot == slots.end()) {
            continue;
        }

        result.assignmentCount += applyTarget(
                *result.mesh,
                TrimeshGuideAttachmentTarget::parse(edge.destPortId),
                slot->second);
    }

    return result;
}

String TrimeshGuidePreparation::configurationKey(
        const NodeGraph& graph,
        const String& trimeshNodeId) {
    String key;
    for (const auto& edge : graph.getEdges()) {
        if (!isGuideAttachment(edge, trimeshNodeId)) {
            continue;
        }

        key << ":guide=" << edge.sourceNodeId << ":target=" << edge.destPortId;
        const Node* source = graph.findNode(edge.sourceNodeId);
        if (source == nullptr) {
            continue;
        }
        for (const auto& parameter : source->parameters) {
            key << ":" << parameter.id << "=" << parameter.value;
        }
        if (source->model != nullptr) {
            key << ":model=" << source->model->schemaId()
                    << ":" << String((int64) source->model->revision());
        }
    }
    return key;
}

}
