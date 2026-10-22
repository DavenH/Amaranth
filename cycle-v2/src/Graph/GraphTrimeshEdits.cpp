#include <algorithm>
#include <cmath>
#include <limits>

#include <Curve/Mesh/CollisionDetector.h>

#include "Graph/GraphCommandDispatcher.h"
#include "Graph/TrimeshAuthoringEdits.h"
#include "Nodes/Trimesh/Editor/TrimeshGuideAttachmentTarget.h"
#include "Nodes/Trimesh/Model/TrimeshNodeModel.h"
#include "Nodes/Trimesh/Model/TrimeshVertexEditCore.h"

namespace CycleV2 {

namespace {

GraphEditResult rejected(const TrimeshAuthoringEdits& request, GraphEditCode code,
        const String& message, int editIndex = -1) {
    GraphEditResult result { code, request.nodeId, {} };
    result.changed = false;
    result.message = message;
    result.editIndex = editIndex;
    return result;
}

bool validAddress(const Mesh& mesh, const TrimeshAuthoringEdit& edit) {
    if (!std::isfinite(edit.value) || edit.value < 0.f || edit.value > 1.f) {
        return false;
    }
    if (edit.kind == TrimeshAuthoringEditKind::CubeCurve) {
        return edit.vertexIndex == -1 && edit.field.isEmpty()
                && isPositiveAndBelow(edit.cubeIndex, mesh.getNumCubes());
    }
    const auto& fields = TrimeshGuideAttachmentTarget::fields();
    return (edit.kind == TrimeshAuthoringEditKind::Vertex
                    || edit.kind == TrimeshAuthoringEditKind::GuideGain)
            && edit.cubeIndex == -1 && isPositiveAndBelow(edit.vertexIndex, mesh.getNumVerts())
            && std::find(fields.begin(), fields.end(), edit.field) != fields.end()
            && (edit.kind != TrimeshAuthoringEditKind::Vertex || edit.field != "curve");
}

bool applyEdit(TrimeshNodeModel& model, const TrimeshAuthoringEdit& edit,
        CollisionDetector& collisionDetector) {
    if (edit.kind == TrimeshAuthoringEditKind::Vertex) {
        return model.setVertexParameter(
                edit.vertexIndex, "vertex." + edit.field, edit.value, collisionDetector);
    }
    if (edit.kind == TrimeshAuthoringEditKind::GuideGain) {
        return model.setVertexGuideGain(edit.vertexIndex, "guideGain." + edit.field, edit.value);
    }
    auto* cube = model.currentMesh().getCubes()[(size_t) edit.cubeIndex];
    const auto delta = TrimeshVertexEditCore::prepareCubeCurve(*cube, edit.value);
    if (!delta.has_value() || !TrimeshVertexEditCore::apply(*cube, *delta)) {
        return false;
    }
    if (delta->changed()) {
        model.markMeshEdited();
    }
    return true;
}

}

GraphEditResult GraphCommandDispatcher::editTrimesh(const TrimeshAuthoringEdits& request) {
    if (hasActiveEdit()) {
        return rejected(request, GraphEditCode::ValidationRejected, "An edit gesture is already active");
    }
    if (request.expectedDocumentRevision != document.revision()) {
        return rejected(request, GraphEditCode::StaleRevision, "Document revision has changed");
    }
    if (request.edits.empty() || request.edits.size() > maximumTrimeshAuthoringEdits) {
        return rejected(request, GraphEditCode::ValidationRejected, "Requires 1 to 64 Trimesh edits");
    }
    const auto* node = document.graph().findNode(request.nodeId);
    if (node == nullptr || node->kind != NodeKind::TrilinearMesh) {
        return rejected(request, node == nullptr ? GraphEditCode::MissingNode : GraphEditCode::WrongNodeKind,
                "Node is not a Trimesh");
    }
    const auto state = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node->model);
    if (state == nullptr) {
        return rejected(request, GraphEditCode::InvalidTypedSnapshot, "Trimesh model is missing");
    }
    if (request.expectedModelRevision != state->revision()) {
        return rejected(request, GraphEditCode::StaleRevision, "Trimesh model revision has changed");
    }
    for (size_t index = 0; index < request.edits.size(); ++index) {
        if (!validAddress(state->mesh(), request.edits[index])) {
            return rejected(request, GraphEditCode::InvalidParameterValue,
                    "Invalid Trimesh address, field or canonical value", (int) index);
        }
    }
    TrimeshNodeModel candidate;
    candidate.syncFromNode(*node);
    CollisionDetector collisionDetector(nullptr, CollisionDetector::Time);
    for (size_t index = 0; index < request.edits.size(); ++index) {
        if (!applyEdit(candidate, request.edits[index], collisionDetector)) {
            return rejected(request, GraphEditCode::ValidationRejected,
                    "Trimesh edit rejected by dimensionality or collision rules", (int) index);
        }
    }
    if (candidate.currentMesh().equals(state->mesh())) {
        GraphEditResult result { GraphEditCode::Connected, request.nodeId, {} };
        result.changed = false;
        return result;
    }
    if (state->revision() == std::numeric_limits<uint64_t>::max()) {
        return rejected(request, GraphEditCode::ConflictingRevision, "Model revision cannot advance");
    }
    return replaceNodeModel(request.nodeId, state->revision(),
            TrimeshNodeModelState::takeOwnership(candidate.releaseMesh(), state->revision() + 1));
}

}
