#include <unordered_set>

#include <Curve/Mesh/EnvelopeMesh.h>
#include <Curve/Mesh/VertCube.h>

#include "Graph/EnvelopeMeshEditPreparation.h"
#include "Nodes/Guide/GuideCurveMeshPreparation.h"

namespace CycleV2 {

EnvelopeMeshEditPreparation::EnvelopeMeshEditPreparation(
        EnvelopeMesh& meshToUse, const NodeGraph& graph, const String& nodeId) :
        mesh(meshToUse) {
    authored.reserve(mesh.getCubes().size());
    std::vector<std::pair<Vertex*, float>> curves;
    curves.reserve(mesh.getCubes().size() * VertCube::numVerts);
    for (auto* cube : mesh.getCubes()) {
        AuthoredSlots saved;
        saved.cube = cube;
        for (int dimension = 0; dimension < Vertex::numElements; ++dimension) {
            saved.values[(size_t) dimension] = cube->guideCurveAt(dimension);
        }
        authored.push_back(saved);
        for (auto* vertex : cube->lineVerts) {
            curves.emplace_back(vertex, vertex->values[Vertex::Curve]);
        }
    }
    provider = GuideCurveMeshPreparation::apply(
            mesh, graph, nodeId, GuideCurveTargetKind::EnvelopeCubeComponent,
            GuideCurveMeshPreparation::Scope::TargetNode).provider;
    for (const auto& curve : curves) {
        curve.first->values[Vertex::Curve] = curve.second;
    }
}

EnvelopeMeshEditPreparation::~EnvelopeMeshEditPreparation() {
    restore();
}

GuideCurveProvider* EnvelopeMeshEditPreparation::guideProvider() const {
    return provider.get();
}

void EnvelopeMeshEditPreparation::restore() {
    if (restored) {
        return;
    }
    const std::unordered_set<VertCube*> live(mesh.getCubes().begin(), mesh.getCubes().end());
    for (const auto& saved : authored) {
        if (live.find(saved.cube) == live.end()) {
            continue;
        }
        for (int dimension = 0; dimension < Vertex::numElements; ++dimension) {
            saved.cube->guideCurveAt(dimension) = saved.values[(size_t) dimension];
        }
    }
    restored = true;
}

}
