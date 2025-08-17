#pragma once

#include <memory>
#include <vector>

#include "Rasterizer.h"
#include "Wireframe/Vertex/Intercept.h"

class CurveGenerator;
class CurveSampler;
class PointPositioner;
class Mesh;

template <typename InputType, typename OutputPointType>
class Interpolator;

using std::unique_ptr;
using std::vector;

// Only addition to base Rasterizer is the Mesh property.
class MeshRasterizer : Rasterizer<Mesh*, Intercept> {
public:

    MeshRasterizer(
        Mesh* mesh,
        unique_ptr<Interpolator<Mesh*, Intercept>> interpolator,
        unique_ptr<PointPositioner> positioners[],
        unique_ptr<CurveGenerator> generator,
        unique_ptr<CurveSampler> sampler
    );

private:
    Mesh* mesh;
};
