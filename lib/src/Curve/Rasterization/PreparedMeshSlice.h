#pragma once

#include <vector>
#include <Curve/Mesh/VertCube.h>
#include <Obj/MorphPosition.h>

class Mesh;

namespace Rasterization {

struct PreparedMeshSlice {
    static PreparedMeshSlice forMesh(const Mesh& mesh, const MorphPosition& morph, int axis);
    static void copy(const VertCube::ReductionData& source, VertCube::ReductionData& destination);
    std::vector<VertCube::ReductionData> cells;
};

}
