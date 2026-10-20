#include "Graph/MeshDimensionality.h"

#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Guide/GuideCurveMeshPreparation.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"

namespace CycleV2 {

NodeModelStatePtr MeshDimensionality::projectModel(
        const Node& node, int count, const MorphPosition& position, const NodeGraph* graph, std::vector<int>* retainedCubeIndices) {
    const uint64_t revision = node.model != nullptr ? node.model->revision() + 1 : 1;
    if (node.kind == NodeKind::Envelope) {
        const auto typed = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
        if (typed == nullptr || typed->envelope() == nullptr) {
            return {};
        }
        EnvelopeNodeModel model;
        model.copyFrom(*typed->envelope());
        auto guides = graph != nullptr ? GuideCurveMeshPreparation::apply(model.getMesh(),
                *graph, node.id, GuideCurveTargetKind::EnvelopeCubeComponent) : PreparedMeshGuideAssignments {};
        MorphDimensions::project(model.getMesh(), MorphDimensions(count), position, guides.provider.get(), retainedCubeIndices);
        model.synchronizeFromMesh(nullptr);
        return CurveNodeModelState::copyOf(model, position.red, position.blue,
                revision, typed->editorJSON());
    }
    const auto typed = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node.model);
    if (typed == nullptr) {
        return {};
    }
    Mesh mesh;
    mesh.deepCopy(&typed->mesh());
    auto guides = graph != nullptr ? GuideCurveMeshPreparation::apply(mesh,
            *graph, node.id, GuideCurveTargetKind::TrimeshCubeComponent) : PreparedMeshGuideAssignments {};
    MorphDimensions::project(mesh, MorphDimensions(count), position, guides.provider.get(), retainedCubeIndices);
    auto result = TrimeshNodeModelState::copyOf(mesh, revision);
    mesh.destroy();
    return result;
}

}
