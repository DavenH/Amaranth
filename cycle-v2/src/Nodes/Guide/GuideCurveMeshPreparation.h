#pragma once

#include <memory>

#include "Nodes/Guide/GuideCurveSnapshotProvider.h"

class Mesh;

namespace CycleV2 {

struct PreparedMeshGuideAssignments {
    std::shared_ptr<GuideCurveSnapshotProvider> provider;
    size_t assignmentCount {};
};

class GuideCurveMeshPreparation {
public:
    static PreparedMeshGuideAssignments apply(
            Mesh& mesh,
            const NodeGraph& graph,
            const String& nodeId,
            GuideCurveTargetKind targetKind);
    static String configurationKey(const NodeGraph& graph, const String& nodeId);
};

}
