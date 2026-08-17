#include "TrimeshGuidePreparation.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Mesh/VertCube.h>

#include <algorithm>
#include <unordered_map>

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

int vertexDimension(GuideCurveField field) {
    switch (field) {
        case GuideCurveField::Time:       return Vertex::Time;
        case GuideCurveField::Red:        return Vertex::Red;
        case GuideCurveField::Blue:       return Vertex::Blue;
        case GuideCurveField::Phase:      return Vertex::Phase;
        case GuideCurveField::Amplitude:  return Vertex::Amp;
        case GuideCurveField::Curve:      return Vertex::Curve;
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
        const TrimeshCubeComponentGuideTarget& target,
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
    for (const auto& assignment : graph.getGuideAssignments()) {
        if (assignment.targetNodeId != trimeshNode.id
                || slots.find(assignment.guideId) != slots.end()) {
            continue;
        }
        const GuideCurveResource* resource = graph.findGuideCurve(assignment.guideId);
        if (resource != nullptr && result.provider->addGuide(*resource)) {
            slots.emplace(resource->id, result.provider->size() - 1);
        }
    }
    for (const auto& assignment : graph.getGuideAssignments()) {
        if (assignment.targetNodeId != trimeshNode.id) {
            continue;
        }
        const auto slot = slots.find(assignment.guideId);
        if (slot == slots.end()) {
            continue;
        }
        result.assignmentCount += applyTarget(*result.mesh, assignment.target, slot->second);
    }

    return result;
}

String TrimeshGuidePreparation::configurationKey(
        const NodeGraph& graph,
        const String& trimeshNodeId) {
    String key;
    for (const auto& assignment : graph.getGuideAssignments()) {
        if (assignment.targetNodeId != trimeshNodeId) {
            continue;
        }
        key << ":guide=" << assignment.guideId
                << ":cube=" << assignment.target.cubeIndex
                << ":field=" << (int) assignment.target.field;
        const GuideCurveResource* resource = graph.findGuideCurve(assignment.guideId);
        if (resource != nullptr) {
            key << ":enabled=" << (resource->enabled ? 1 : 0)
                    << ":noise=" << resource->noise
                    << ":dc=" << resource->dcOffset
                    << ":phase=" << resource->phase;
            if (resource->model != nullptr) {
                key << ":model=" << resource->model->schemaId()
                        << ":" << String((int64) resource->model->revision());
            }
        }
    }
    return key;
}

}
