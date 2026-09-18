#include <Curve/Mesh/Mesh.h>

#include "Nodes/Guide/GuideAttachmentTarget.h"

#include "Graph/NodeGraph.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Trimesh/Model/TrimeshMeshState.h"

namespace CycleV2 {

bool GuideAttachmentTarget::isValid(
        const Node& node,
        const TrimeshCubeComponentGuideTarget& target) {
    const int field = (int) target.field;
    if (!isPositiveAndBelow(field, fieldCount)) {
        return false;
    }
    if (node.kind == NodeKind::TrilinearMesh) {
        const auto model = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node.model);
        return model != nullptr
                && isPositiveAndBelow(target.cubeIndex, model->mesh().getNumCubes());
    }
    if (node.kind == NodeKind::Envelope) {
        const auto model = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
        return model != nullptr
                && model->envelope() != nullptr
                && isPositiveAndBelow(
                        target.cubeIndex,
                        model->envelope()->getMesh().getNumCubes());
    }
    return false;
}

}
