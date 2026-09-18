#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentTarget.h"

#include "Graph/NodeGraph.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>

#include <algorithm>

#include "Nodes/Trimesh/Model/TrimeshMeshState.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"

namespace CycleV2 {

const std::array<juce::String, TrimeshGuideAttachmentTarget::fieldCount>&
TrimeshGuideAttachmentTarget::fields() {
    static const std::array<juce::String, fieldCount> values {
            "time",
            "red",
            "blue",
            "phase",
            "amp",
            "curve"
    };

    return values;
}

GuideCurveField TrimeshGuideAttachmentTarget::guideField(const juce::String& field) {
    if (field == "time") {
        return GuideCurveField::Time;
    }
    if (field == "red") {
        return GuideCurveField::Red;
    }
    if (field == "blue") {
        return GuideCurveField::Blue;
    }
    if (field == "phase") {
        return GuideCurveField::Phase;
    }
    if (field == "amp") {
        return GuideCurveField::Amplitude;
    }
    return GuideCurveField::Curve;
}

bool TrimeshGuideAttachmentTarget::isValid(
        const Node& trimeshNode,
        const TrimeshCubeComponentGuideTarget& target) {
    const auto model = std::dynamic_pointer_cast<const TrimeshNodeModelState>(trimeshNode.model);
    const int field = (int) target.field;
    return trimeshNode.kind == NodeKind::TrilinearMesh
            && model != nullptr
            && isPositiveAndBelow(target.cubeIndex, model->mesh().getNumCubes())
            && isPositiveAndBelow(field, fieldCount);
}

std::vector<TrimeshCubeComponentGuideTarget> TrimeshGuideAttachmentTarget::cubeTargetsForVertex(
        const Node& trimeshNode,
        int vertexIndex,
        const juce::String& field) {
    const auto model = std::dynamic_pointer_cast<const TrimeshNodeModelState>(trimeshNode.model);
    if (model == nullptr || !isPositiveAndBelow(vertexIndex, model->mesh().getNumVerts())) {
        return {};
    }

    Mesh& mesh = *const_cast<Mesh*>(&model->mesh());
    Vertex* vertex = mesh.getVerts()[(size_t) vertexIndex];
    std::vector<TrimeshCubeComponentGuideTarget> targets;
    for (auto* owner : vertex->owners) {
        const auto found = std::find(mesh.getCubes().begin(), mesh.getCubes().end(), owner);
        if (found != mesh.getCubes().end()) {
            targets.push_back({
                    (int) std::distance(mesh.getCubes().begin(), found),
                    guideField(field)
            });
        }
    }
    return targets;
}

bool MeshGuideAttachmentTarget::isValid(
        const Node& node,
        const TrimeshCubeComponentGuideTarget& target) {
    if (node.kind == NodeKind::TrilinearMesh) {
        return TrimeshGuideAttachmentTarget::isValid(node, target);
    }
    const auto model = std::dynamic_pointer_cast<const CurveNodeModelState>(node.model);
    return node.kind == NodeKind::Envelope
            && model != nullptr
            && model->envelope() != nullptr
            && isPositiveAndBelow(target.cubeIndex, model->envelope()->getMesh().getNumCubes())
            && isPositiveAndBelow((int) target.field, TrimeshGuideAttachmentTarget::fieldCount);
}

std::vector<TrimeshCubeComponentGuideTarget> MeshGuideAttachmentTarget::cubeTargetsForSelection(
        const Node& node,
        int selectionIndex,
        const String& field) {
    if (node.kind == NodeKind::TrilinearMesh) {
        return TrimeshGuideAttachmentTarget::cubeTargetsForVertex(
                node, selectionIndex, field);
    }
    if (std::find(
                TrimeshGuideAttachmentTarget::fields().begin(),
                TrimeshGuideAttachmentTarget::fields().end(),
                field) == TrimeshGuideAttachmentTarget::fields().end()) {
        return {};
    }
    const TrimeshCubeComponentGuideTarget target {
            selectionIndex,
            TrimeshGuideAttachmentTarget::guideField(field)
    };
    return isValid(node, target)
            ? std::vector<TrimeshCubeComponentGuideTarget> { target }
            : std::vector<TrimeshCubeComponentGuideTarget> {};
}

}
