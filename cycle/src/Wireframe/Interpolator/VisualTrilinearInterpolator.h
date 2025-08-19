#pragma once

#include "Obj/TrilinearEdge.h"
#include "Wireframe/Interpolator/Interpolator.h"
#include "Wireframe/Interpolator/Trilinear/MorphPosition.h"

class PathDeformingPositioner;

class VisualTrilinearInterpolator : public Interpolator<Mesh*, TrilinearEdge> {
    explicit VisualTrilinearInterpolator(PathDeformingPositioner* pathDeformer);

    vector<TrilinearEdge> interpolate(Mesh* usedMesh, const RasterizerParams& params) override;
    virtual int getPrimaryViewDimension() { return Vertex::Time; };

protected:
    MorphPosition morph;
    TrilinearCube::ReductionData reduct {};
    PathDeformingPositioner* pathDeformer;
};
