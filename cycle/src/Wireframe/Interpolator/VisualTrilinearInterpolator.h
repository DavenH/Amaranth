#pragma once

#include "Obj/ColorPoint.h"
#include "Wireframe/Interpolator/Interpolator.h"
#include "Wireframe/Interpolator/Trilinear/MorphPosition.h"

class PathDeformingPositioner;

class VisualTrilinearInterpolator : public Interpolator<Mesh*, ColorPoint> {
    explicit VisualTrilinearInterpolator(PathDeformingPositioner* pathDeformer);

    vector<ColorPoint> interpolate(Mesh* usedMesh, const InterpolatorParameters& params) override;
    virtual int getPrimaryViewDimension() { return Vertex::Time; };

protected:
    MorphPosition morph;
    TrilinearCube::ReductionData reduct {};
    PathDeformingPositioner* pathDeformer;
};
