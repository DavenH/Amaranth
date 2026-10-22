#include "PreparedMeshSlice.h"

#include <Curve/Mesh/Mesh.h>
#include <Curve/Rasterization/Interpolation/TrilinearMeshSlicer.h>

namespace Rasterization {

void PreparedMeshSlice::copy(
        const VertCube::ReductionData& source, VertCube::ReductionData& destination) {
    destination.pointOverlaps = source.pointOverlaps;
    destination.lineOverlaps = source.lineOverlaps;
    const Vertex* inputs[] { &source.v, &source.v0, &source.v1,
            &source.v00, &source.v01, &source.v10, &source.v11 };
    Vertex* outputs[] { &destination.v, &destination.v0, &destination.v1,
            &destination.v00, &destination.v01, &destination.v10, &destination.v11 };
    for (int index = 0; index < 7; ++index) {
        VecOps::copy(inputs[index]->values, outputs[index]->values, Vertex::numElements);
    }
}

PreparedMeshSlice PreparedMeshSlice::forMesh(
        const Mesh& mesh, const MorphPosition& morph, int axis) {
    PreparedMeshSlice result;
    result.cells.resize(mesh.getCubes().size());
    TrilinearMeshSlicer slicer;
    for (size_t index = 0; index < result.cells.size(); ++index) {
        slicer.slice(*mesh.getCubes()[index], axis, result.cells[index], morph, 0);
    }
    return result;
}

}
