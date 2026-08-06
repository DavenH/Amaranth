#pragma once

#include <memory>

#include "../Guide/GuideCurveSnapshotProvider.h"

class Mesh;

namespace CycleV2 {

struct PreparedTrimeshGuides {
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<GuideCurveSnapshotProvider> provider;
    size_t assignmentCount {};
};

class TrimeshGuidePreparation {
public:
    static PreparedTrimeshGuides prepare(
            const NodeGraph& graph,
            const Node& trimeshNode,
            const Mesh& sourceMesh);
    static String configurationKey(
            const NodeGraph& graph,
            const String& trimeshNodeId);
};

}
