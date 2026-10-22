#pragma once

#include <array>
#include <memory>
#include <vector>

#include <Curve/Mesh/Vertex.h>

class EnvelopeMesh;
class GuideCurveProvider;
class VertCube;

namespace CycleV2 {

class GuideCurveSnapshotProvider;
class NodeGraph;

// Install graph-owned Guides for canonical marker/seam evaluation without
// publishing provider slots or preparation-only sharpness into authored geometry.
class EnvelopeMeshEditPreparation final {
public:
    EnvelopeMeshEditPreparation(EnvelopeMesh& mesh, const NodeGraph& graph, const String& nodeId);
    ~EnvelopeMeshEditPreparation();

    GuideCurveProvider* guideProvider() const;
    void restore();

private:
    struct AuthoredSlots {
        VertCube* cube {};
        std::array<char, Vertex::numElements> values;
    };

    EnvelopeMesh& mesh;
    std::vector<AuthoredSlots> authored;
    std::shared_ptr<GuideCurveSnapshotProvider> provider;
    bool restored {};
};

}
