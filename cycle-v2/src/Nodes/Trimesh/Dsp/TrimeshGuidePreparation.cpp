#include <Curve/Mesh/Mesh.h>

#include "Nodes/Trimesh/Dsp/TrimeshGuidePreparation.h"

namespace CycleV2 {

namespace {

std::shared_ptr<Mesh> copyMesh() {
    return std::shared_ptr<Mesh>(new Mesh(), [](Mesh* mesh) {
        mesh->destroy();
        delete mesh;
    });
}

}

PreparedTrimeshGuides TrimeshGuidePreparation::prepare(
        const NodeGraph& graph,
        const Node& trimeshNode,
        const Mesh& sourceMesh) {
    PreparedTrimeshGuides result;
    result.mesh = copyMesh();
    result.mesh->deepCopy(&sourceMesh);
    auto assignments = GuideCurveMeshPreparation::apply(
            *result.mesh,
            graph,
            trimeshNode.id,
            GuideCurveTargetKind::TrimeshCubeComponent);
    result.provider = std::move(assignments.provider);
    result.assignmentCount = assignments.assignmentCount;
    return result;
}

}
