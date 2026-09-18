#include "Nodes/Trimesh/Model/TrimeshMeshDeltaOverlay.h"

#include "Graph/InteractionComplexityDiagnostics.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Mesh/VertCube.h>

namespace CycleV2 {

TrimeshMeshDeltaOverlay::TrimeshMeshDeltaOverlay(
        std::shared_ptr<const Mesh> baseMesh,
        TrimeshVertexEditDelta delta) :
        sourceMesh(std::move(baseMesh))
    ,   edit(std::move(delta)) {
}

TrimeshMeshDeltaOverlay::~TrimeshMeshDeltaOverlay() = default;

std::shared_ptr<const TrimeshMeshDeltaOverlay> TrimeshMeshDeltaOverlay::create(
        std::shared_ptr<const Mesh> baseMesh,
        TrimeshVertexEditDelta delta) {
    if (baseMesh == nullptr
            || !TrimeshVertexEditCore::canApply(*baseMesh, delta)) {
        return {};
    }
    auto overlay = std::shared_ptr<TrimeshMeshDeltaOverlay>(
            new TrimeshMeshDeltaOverlay(std::move(baseMesh), std::move(delta)));
    return overlay->prepare() ? overlay : nullptr;
}

bool TrimeshMeshDeltaOverlay::prepare() {
    if (!edit.changed()) {
        return true;
    }
    const Vertex* editedVertex = sourceMesh->getVerts()[(size_t) edit.vertexIndex];
    size_t changeIndex {};
    for (int ordinal = 0; ordinal < editedVertex->owners.size(); ++ordinal) {
        InteractionComplexityDiagnostics::recordMeshEditOwnerVisit();
        VertCube* source = editedVertex->owners[ordinal];
        if (source == nullptr) {
            continue;
        }
        const TrimeshVertexValueChange* changedGuide = nullptr;
        if (edit.target == TrimeshVertexEditTarget::GuideGain) {
            if (changeIndex >= edit.changes.size()
                    || edit.changes[changeIndex].ownerOrdinal != ordinal) {
                continue;
            }
            changedGuide = &edit.changes[changeIndex++];
        }

        CubeOverride replacement;
        replacement.cube = std::make_unique<VertCube>();
        replacement.cube->setPropertiesFrom(source);
        for (int slot = 0; slot < (int) replacement.vertices.size(); ++slot) {
            const Vertex* original = source->getVertex(slot);
            if (original == nullptr) {
                return false;
            }
            replacement.vertices[(size_t) slot] = std::make_unique<Vertex>();
            VecOps::copy(
                    original->values,
                    replacement.vertices[(size_t) slot]->values,
                    Vertex::numElements);
            if (edit.target == TrimeshVertexEditTarget::VertexValue
                    && original == editedVertex && edit.changed()) {
                replacement.vertices[(size_t) slot]->values[edit.valueIndex]
                        = edit.changes.front().after;
            }
            replacement.cube->setVertex(replacement.vertices[(size_t) slot].get(), slot);
        }
        if (edit.target == TrimeshVertexEditTarget::GuideGain) {
            replacement.cube->guideCurveGainAt(edit.valueIndex) = changedGuide->after;
        }
        replacements.emplace(source, replacement.cube.get());
        cubes.push_back(std::move(replacement));
    }
    return true;
}

Mesh& TrimeshMeshDeltaOverlay::rasterizerMesh() const {
    return *const_cast<Mesh*>(sourceMesh.get());
}

VertCube* TrimeshMeshDeltaOverlay::resolve(VertCube* cube) const {
    const auto found = replacements.find(cube);
    return found != replacements.end() ? found->second : cube;
}

}
