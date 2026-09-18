#pragma once

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Nodes/Trimesh/Model/TrimeshVertexEditCore.h"

class Mesh;
class Vertex;
class VertCube;

namespace CycleV2 {

class TrimeshMeshDeltaOverlay final {
public:
    static std::shared_ptr<const TrimeshMeshDeltaOverlay> create(
            std::shared_ptr<const Mesh> baseMesh,
            TrimeshVertexEditDelta delta);
    ~TrimeshMeshDeltaOverlay();

    Mesh& rasterizerMesh() const;
    VertCube* resolve(VertCube* cube) const;

private:
    struct CubeOverride {
        std::unique_ptr<VertCube> cube;
        std::array<std::unique_ptr<Vertex>, 8> vertices;
    };

    TrimeshMeshDeltaOverlay(
            std::shared_ptr<const Mesh> baseMesh,
            TrimeshVertexEditDelta delta);
    bool prepare();

    std::shared_ptr<const Mesh> sourceMesh;
    TrimeshVertexEditDelta edit;
    std::vector<CubeOverride> cubes;
    std::unordered_map<const VertCube*, VertCube*> replacements;
};

}
