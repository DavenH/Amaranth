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
    enum class Scope { AllResources, TargetNode };

    static PreparedMeshGuideAssignments apply(
            Mesh& mesh,
            const NodeGraph& graph,
            const String& nodeId,
            GuideCurveTargetKind targetKind,
            Scope scope = Scope::AllResources);
    static String configurationKey(const NodeGraph& graph, const String& nodeId);
};

}
