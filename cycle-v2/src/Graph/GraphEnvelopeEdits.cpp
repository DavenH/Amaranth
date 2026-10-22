#include <limits>

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>
#include <Curve/Rasterization/Rasterizer/EnvRasterizer.h>

#include "Graph/EnvelopeAuthoringEdits.h"
#include "Graph/EnvelopeMeshEditPreparation.h"
#include "Graph/GraphCommandDispatcher.h"
#include "Graph/InteractionComplexityDiagnostics.h"
#include "Graph/MeshDimensionality.h"
#include "Nodes/Curve/Model/CurveNodeModels.h"
#include "Nodes/Envelope/EnvelopeEditCore.h"

namespace CycleV2 {

namespace {

GraphEditResult rejected(const EnvelopeAuthoringEdits& request, GraphEditCode code,
        const String& message, int index = -1) {
    GraphEditResult result { code, request.nodeId, {} };
    result.changed = false;
    result.message = message;
    result.editIndex = index;
    return result;
}

void refresh(EnvelopeMesh& mesh, EnvRasterizer& rasterizer, VertCube* edited) {
    rasterizer.updateWaveform(&mesh, 0.f);
    EnvelopeEditCore::validateMarkers(mesh, rasterizer);
    const bool loop = edited != nullptr && EnvelopeEditCore::markerState(mesh, *edited, true);
    const bool sustain = edited != nullptr && EnvelopeEditCore::markerState(mesh, *edited, false);
    if (loop != sustain && EnvelopeEditCore::synchronizeLoopSeam(mesh, rasterizer, loop)) {
        rasterizer.updateWaveform(&mesh, 0.f);
        EnvelopeEditCore::validateMarkers(mesh, rasterizer);
    }
}

}

GraphEditResult GraphCommandDispatcher::editEnvelope(const EnvelopeAuthoringEdits& request) {
    if (hasActiveEdit()) {
        return rejected(request, GraphEditCode::ValidationRejected, "An edit gesture is already active");
    }
    if (request.expectedDocumentRevision != document.revision()) {
        return rejected(request, GraphEditCode::StaleRevision, "Document revision has changed");
    }
    if (request.edits.empty() || request.edits.size() > maximumEnvelopeAuthoringEdits) {
        return rejected(request, GraphEditCode::ValidationRejected, "Requires 1 to 64 Envelope edits");
    }
    const auto* node = document.graph().findNode(request.nodeId);
    if (node == nullptr) {
        return rejected(request, GraphEditCode::MissingNode, "Envelope node does not exist");
    }
    const auto state = std::dynamic_pointer_cast<const CurveNodeModelState>(node->model);
    if (node->kind != NodeKind::Envelope || state == nullptr || state->envelope() == nullptr) {
        return rejected(request, GraphEditCode::WrongNodeKind, "Requires an Envelope model");
    }
    if (request.expectedModelRevision != state->revision()) {
        return rejected(request, GraphEditCode::StaleRevision, "Envelope model revision has changed");
    }
    if (request.dimensionCount < 0 || request.dimensionCount > 3
            || request.dimensionCount != MeshDimensionality::dimensions(*node).axisCount()) {
        return rejected(request, GraphEditCode::InvalidParameterValue, "Envelope dimensions have changed");
    }

    // One owned model stage per discrete request. Existing Envelope copy/baseline
    // preparation remains here; no operation below copies or serializes a model.
    auto model = std::make_unique<EnvelopeNodeModel>();
    InteractionComplexityDiagnostics::recordMeshCopy(
            (size_t) state->envelope()->getMesh().getNumVerts(),
            (size_t) state->envelope()->getMesh().getNumCubes());
    if (!model->copyFrom(*state->envelope())) {
        return rejected(request, GraphEditCode::InvalidTypedSnapshot, "Envelope model is invalid");
    }
    auto& mesh = model->getMesh();
    EnvelopeMeshEditPreparation preparation(mesh, document.graph(), request.nodeId);
    EnvRasterizer rasterizer(preparation.guideProvider(), "EnvelopeAuthoring");
    rasterizer.setMesh(&mesh);
    rasterizer.setDimensionality(request.dimensionCount);
    rasterizer.setMorphPosition(MeshDimensionality::position(*node));
    rasterizer.setCalcDepthDimensions(false);
    rasterizer.updateWaveform(&mesh, 0.f);
    const EnvelopeEditScope scope { request.dimensionCount, request.linkedAxes };
    for (size_t index = 0; index < request.edits.size(); ++index) {
        const auto& edit = request.edits[index];
        auto* cube = model->cubeForIdentity(edit.cubeId);
        if (cube == nullptr || edit.corner < 0 || edit.corner >= VertCube::numVerts) {
            return rejected(request, GraphEditCode::InvalidParameterValue,
                    "Unknown Envelope cube or corner", (int) index);
        }
        bool accepted = true;
        bool changed = false;
        const int dimension = EnvelopeEditCore::fieldDimension(edit.field);
        switch (edit.kind) {
            case EnvelopeAuthoringEditKind::Value:
                accepted = (edit.field == "yellow" || edit.field == "red" || edit.field == "blue"
                        || edit.field == "phase" || edit.field == "amp" || edit.field == "curve")
                        && EnvelopeEditCore::setValue(*cube, edit.corner, dimension, edit.value, scope, &changed);
                break;
            case EnvelopeAuthoringEditKind::GuideGain:
                accepted = (edit.field == "component" || edit.field == "red" || edit.field == "blue"
                        || edit.field == "phase" || edit.field == "amp" || edit.field == "curve")
                        && EnvelopeEditCore::setGuideGain(
                                *cube, dimension, edit.value, request.dimensionCount, &changed);
                break;
            case EnvelopeAuthoringEditKind::LoopMarker:
            case EnvelopeAuthoringEditKind::SustainMarker:
                accepted = edit.field.isEmpty();
                if (accepted) {
                    changed = EnvelopeEditCore::setMarker(mesh, *cube,
                            edit.kind == EnvelopeAuthoringEditKind::LoopMarker, edit.enabled, rasterizer);
                }
                break;
            default:
                accepted = false;
        }
        if (!accepted) {
            return rejected(request, GraphEditCode::InvalidParameterValue,
                    "Invalid Envelope field or value", (int) index);
        }
        if (changed) {
            refresh(mesh, rasterizer, cube);
        }
    }
    preparation.restore();
    if (model->equalsGeometryAndTopology(*state->envelope())) {
        GraphEditResult result { GraphEditCode::Connected, request.nodeId, {} };
        result.changed = false;
        return result;
    }
    if (state->revision() == std::numeric_limits<uint64_t>::max()) {
        return rejected(request, GraphEditCode::ConflictingRevision, "Envelope revision cannot advance");
    }
    model->synchronizeFromMesh(model->selectedMeshCube());
    return replaceNodeModel(request.nodeId, state->revision(), CurveNodeModelState::takeEnvelope(
            std::move(model), state->envelopeRed(), state->envelopeBlue(),
            state->revision() + 1, state->editorJSON()));
}

}
