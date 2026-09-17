#include <Curve/Mesh/Mesh.h>
#include <Curve/Mesh/Vertex.h>
#include <Curve/Mesh/VertCube.h>

#include <algorithm>
#include <unordered_map>

#include "Nodes/Guide/GuideCurveMeshPreparation.h"

namespace CycleV2 {

namespace {

struct StringHash {
    size_t operator()(const String& value) const {
        return (size_t) value.hashCode64();
    }
};

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

size_t applyTarget(
        Mesh& mesh,
        const TrimeshCubeComponentGuideTarget& target,
        int guideSlot) {
    if (!isPositiveAndBelow(target.cubeIndex, mesh.getNumCubes())) {
        return 0;
    }
    const int dimension = vertexDimension(target.field);
    if (!isPositiveAndBelow(dimension, Vertex::numElements)) {
        return 0;
    }
    VertCube* cube = mesh.getCubes()[(size_t) target.cubeIndex];
    if (cube == nullptr) {
        return 0;
    }
    cube->guideCurveAt(dimension) = (char) guideSlot;
    if (dimension == Vertex::Time) {
        preserveComponentCurveSharpness(*cube);
    }
    return 1;
}

}

PreparedMeshGuideAssignments GuideCurveMeshPreparation::apply(
        Mesh& mesh,
        const NodeGraph& graph,
        const String& nodeId,
        GuideCurveTargetKind targetKind) {
    PreparedMeshGuideAssignments result;
    result.provider = std::make_shared<GuideCurveSnapshotProvider>();
    clearGuideAssignments(mesh);

    std::unordered_map<String, int, StringHash> slots;
    for (const auto& resource : graph.getGuideCurves()) {
        const GuideHeatmapAsset* heatmap = graph.findGuideHeatmap(resource.heatmapAssetId);
        if (result.provider->addGuide(resource, heatmap)) {
            slots.emplace(resource.id, result.provider->size() - 1);
        }
    }
    for (const auto& assignment : graph.getGuideAssignments()) {
        if (assignment.targetNodeId != nodeId || assignment.targetKind != targetKind) {
            continue;
        }
        const auto slot = slots.find(assignment.guideId);
        if (slot != slots.end()) {
            result.assignmentCount += applyTarget(mesh, assignment.target, slot->second);
        }
    }
    return result;
}

String GuideCurveMeshPreparation::configurationKey(
        const NodeGraph& graph,
        const String& nodeId) {
    String key;
    for (const auto& assignment : graph.getGuideAssignments()) {
        if (assignment.targetNodeId != nodeId) {
            continue;
        }
        key << ":guide=" << assignment.guideId
                << ":cube=" << assignment.target.cubeIndex
                << ":field=" << (int) assignment.target.field;
        const GuideCurveResource* resource = graph.findGuideCurve(assignment.guideId);
        if (resource != nullptr) {
            const auto& guides = graph.getGuideCurves();
            const auto guide = std::find_if(
                    guides.begin(),
                    guides.end(),
                    [&](const GuideCurveResource& candidate) {
                        return candidate.id == resource->id;
                    });
            key << ":slot=" << std::distance(guides.begin(), guide)
                    << ":revision=" << String((int64) resource->revision)
                    << ":heatmap=" << resource->heatmapAssetId
                    << ":enabled=" << (resource->enabled ? 1 : 0)
                    << ":noise=" << resource->noise
                    << ":noiseSeed=" << resource->noiseSeed
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
