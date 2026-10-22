#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_map>

#include <Curve/Mesh/CollisionDetector.h>
#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>
#include <Obj/MorphPosition.h>

#include "Graph/EnvelopeMeshEditPreparation.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/GraphNodeStateEditor.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Graph/MeshDimensionality.h"
#include "Graph/MeshTopologyEdits.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Envelope/EnvelopeEditCore.h"
#include "Nodes/ModelAuthoringValidation.h"
#include "Nodes/Trimesh/Model/TrimeshNodeModel.h"

namespace CycleV2 {

namespace {

struct DetachedMeshObjects {
    std::vector<std::unique_ptr<VertCube>> cubes;
    std::vector<std::unique_ptr<Vertex>> vertices;
};

GraphEditResult rejected(const MeshTopologyEdits& request, GraphEditCode code,
        const String& message, int index = -1) {
    GraphEditResult result { code, request.nodeId, {} };
    result.changed = false;
    result.message = message;
    result.editIndex = index;
    return result;
}

bool applyTopology(Mesh& mesh, const MeshTopologyEdit& edit, int index,
        std::vector<int>& retained, bool envelope, DetachedMeshObjects& detachedObjects) {
    if (edit.kind == MeshTopologyEditKind::Remove) {
        if (!isPositiveAndBelow(index, mesh.getNumCubes())) {
            return false;
        }
        auto* cube = mesh.getCubes()[(size_t) index];
        if (envelope) {
            auto& env = static_cast<EnvelopeMesh&>(mesh);
            env.loopCubes.erase(cube);
            env.sustainCubes.erase(cube);
        }
        std::vector<Vertex*> detached;
        mesh.detachCube(cube, detached);
        detachedObjects.cubes.emplace_back(cube);
        for (auto* vertex : detached) {
            detachedObjects.vertices.emplace_back(vertex);
        }
        retained.erase(retained.begin() + index);
        return true;
    }
    if (edit.kind != MeshTopologyEditKind::Insert || index < 0 || index > mesh.getNumCubes()
            || mesh.getNumCubes() >= ModelAuthoringValidation::maximumCubes
            || mesh.getNumVerts() > ModelAuthoringValidation::maximumVertices - (int) VertCube::numVerts
            || !std::isfinite(edit.phase) || edit.phase < 0.f
            || edit.phase > (envelope ? EnvelopeEditCore::maximumPhase : 1.f)
            || !std::isfinite(edit.amp) || edit.amp < 0.f || edit.amp > 1.f
            || !std::isfinite(edit.curve) || edit.curve < 0.f || edit.curve > 1.f) {
        return false;
    }
    auto* cube = new VertCube(&mesh);
    cube->initLine(MorphPosition(0.f, 0.f, 0.f), edit.phase, edit.amp, edit.curve);
    mesh.getCubes().insert(mesh.getCubes().begin() + index, cube);
    retained.insert(retained.begin() + index, -1);
    if (!envelope) {
        CollisionDetector detector(nullptr, CollisionDetector::Time);
        detector.setCurrentSelection(&mesh, cube);
        if (!detector.validate()) {
            return false;
        }
    }
    return true;
}

}

GraphEditResult GraphCommandDispatcher::editMeshTopology(const MeshTopologyEdits& request) {
    if (hasActiveEdit() || request.edits.empty() || request.edits.size() > maximumMeshTopologyEdits) {
        return rejected(request, GraphEditCode::ValidationRejected,
                "Finish active gestures; requires 1 to 16 topology operations");
    }
    if (request.expectedDocumentRevision != document.revision()) {
        return rejected(request, GraphEditCode::StaleRevision, "Document revision has changed");
    }
    const auto* node = document.graph().findNode(request.nodeId);
    if (node == nullptr || !MeshDimensionality::applies(node->kind) || node->model == nullptr) {
        return rejected(request, GraphEditCode::WrongNodeKind, "Requires a Trimesh or Envelope model");
    }
    const auto revision = node->model->revision();
    if (revision != request.expectedModelRevision || revision == std::numeric_limits<uint64_t>::max()) {
        return rejected(request, GraphEditCode::StaleRevision, "Model revision has changed or cannot advance");
    }
    const bool envelope = node->kind == NodeKind::Envelope;
    TrimeshNodeModel trimesh;
    std::unique_ptr<EnvelopeNodeModel> env;
    const auto curveState = std::dynamic_pointer_cast<const CurveNodeModelState>(node->model);
    if (envelope) {
        if (curveState == nullptr || curveState->envelope() == nullptr) {
            return rejected(request, GraphEditCode::InvalidTypedSnapshot, "Envelope model is missing");
        }
        env = std::make_unique<EnvelopeNodeModel>();
        InteractionComplexityDiagnostics::recordMeshCopy(
                (size_t) curveState->envelope()->getMesh().getNumVerts(),
                (size_t) curveState->envelope()->getMesh().getNumCubes());
        if (!env->copyFrom(*curveState->envelope())) {
            return rejected(request, GraphEditCode::InvalidTypedSnapshot, "Envelope model is invalid");
        }
    } else {
        if (std::dynamic_pointer_cast<const TrimeshNodeModelState>(node->model) == nullptr) {
            return rejected(request, GraphEditCode::InvalidTypedSnapshot, "Trimesh model is missing");
        }
        trimesh.syncFromNode(*node);
    }
    Mesh& mesh = envelope ? env->getMesh() : trimesh.currentMesh();
    DetachedMeshObjects detachedObjects;
    std::unique_ptr<EnvelopeMeshEditPreparation> envelopePreparation;
    if (envelope) {
        envelopePreparation = std::make_unique<EnvelopeMeshEditPreparation>(
                env->getMesh(), document.graph(), request.nodeId);
    }
    std::vector<int> retained((size_t) mesh.getNumCubes());
    std::iota(retained.begin(), retained.end(), 0);
    std::unordered_map<uint64_t, VertCube*> envelopeCubes;
    VertCube* selection = envelope ? env->selectedMeshCube() : nullptr;
    const int selectedVertexIndex = (int) node->editorState.getProperty("selectedVertexId", -1);
    const auto* selectedVertex = !envelope && isPositiveAndBelow(selectedVertexIndex, mesh.getNumVerts())
            ? mesh.getVerts()[(size_t) selectedVertexIndex] : nullptr;
    if (envelope) {
        for (const auto id : env->getCubeIds()) {
            envelopeCubes.emplace(id, env->cubeForIdentity(id));
        }
    }
    for (size_t operation = 0; operation < request.edits.size(); ++operation) {
        const auto& edit = request.edits[operation];
        int index = edit.cubeIndex;
        if (envelope && edit.kind == MeshTopologyEditKind::Remove) {
            const auto found = envelopeCubes.find(edit.cubeId);
            const auto* cube = found != envelopeCubes.end() ? found->second : nullptr;
            const auto position = std::find(mesh.getCubes().begin(), mesh.getCubes().end(), cube);
            index = position == mesh.getCubes().end() ? -1 : (int) std::distance(mesh.getCubes().begin(), position);
        }
        if ((!envelope && edit.cubeId != 0)
                || (envelope && edit.kind == MeshTopologyEditKind::Remove && edit.cubeIndex != -1)
                || (edit.kind == MeshTopologyEditKind::Insert && edit.cubeId != 0)
                || !applyTopology(mesh, edit, index, retained, envelope, detachedObjects)) {
            return rejected(request, GraphEditCode::ValidationRejected,
                    "Invalid topology address, value or colliding new cube", (int) operation);
        }
        if (envelope && edit.kind == MeshTopologyEditKind::Remove) {
            if (selection == envelopeCubes[edit.cubeId]) {
                selection = nullptr;
            }
            envelopeCubes.erase(edit.cubeId);
        }
    }
    mesh.validate();
    NodeModelStatePtr publication;
    var editorState = node->editorState;
    bool editorStateChanged = false;
    if (envelope) {
        EnvRasterizer rasterizer(envelopePreparation->guideProvider(), "CycleV2EnvelopeTopology");
        rasterizer.setMesh(&env->getMesh());
        rasterizer.setDimensionality(MeshDimensionality::dimensions(*node).axisCount());
        rasterizer.setMorphPosition(MeshDimensionality::position(*node));
        rasterizer.setCalcDepthDimensions(false);
        rasterizer.updateWaveform(&env->getMesh(), 0.f);
        EnvelopeEditCore::validateMarkers(env->getMesh(), rasterizer);
        EnvelopeEditCore::synchronizeLoopSeam(env->getMesh(), rasterizer, true);
        envelopePreparation->restore();
        if (!env->synchronizeFromMesh(selection)) {
            return rejected(request, GraphEditCode::ValidationRejected, "Envelope cube identities cannot advance");
        }
        if (env->equals(*curveState->envelope())) {
            auto result = rejected(request, GraphEditCode::Connected, {});
            return result;
        }
        var modelEditorState = curveState->editorJSON();
        if (modelEditorState.hasProperty("selectedCubeId")
                && env->cubeForIdentity((uint64_t) (int64) modelEditorState["selectedCubeId"]) == nullptr) {
            modelEditorState = modelEditorState.clone();
            modelEditorState.getDynamicObject()->removeProperty("selectedCubeId");
        }
        if (editorState.hasProperty("selectedCubeId")
                && env->cubeForIdentity((uint64_t) (int64) editorState["selectedCubeId"]) == nullptr) {
            editorState = editorState.clone();
            editorState.getDynamicObject()->removeProperty("selectedCubeId");
            editorStateChanged = true;
        }
        publication = CurveNodeModelState::takeEnvelope(std::move(env), curveState->envelopeRed(),
                curveState->envelopeBlue(), revision + 1, modelEditorState);
    } else {
        const auto before = std::dynamic_pointer_cast<const TrimeshNodeModelState>(node->model);
        if (mesh.equals(before->mesh())) {
            return rejected(request, GraphEditCode::Connected, {});
        }
        if (editorState.hasProperty("selectedVertexId")) {
            const auto found = std::find(mesh.getVerts().begin(), mesh.getVerts().end(), selectedVertex);
            const int nextIndex = found == mesh.getVerts().end() ? -1
                    : (int) std::distance(mesh.getVerts().begin(), found);
            if (nextIndex != selectedVertexIndex) {
                editorState = editorState.clone();
                editorState.getDynamicObject()->setProperty("selectedVertexId", nextIndex);
                editorStateChanged = true;
            }
        }
        publication = TrimeshNodeModelState::takeOwnership(trimesh.releaseMesh(), revision + 1);
    }
    return applyIncremental(
            [&](GraphDeltaBuilder& delta, const NodeGraph& graph) {
                delta.captureNodeModel(graph, request.nodeId);
                delta.captureNodeEditorState(graph, request.nodeId);
            },
            [&](NodeGraph& graph) {
                GraphNodeStateEditor editor;
                auto result = editor.replaceNodeModel(
                        graph, request.nodeId, revision, publication, &retained);
                if (result.succeeded() && editorStateChanged) {
                    const auto selected = editor.setNodeEditorState(graph, request.nodeId, editorState);
                    result.changed = result.changed || selected.changed;
                    accumulateChange(result.changes, selected.changes);
                }
                return result;
            });
}

}
