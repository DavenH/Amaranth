#pragma once

#include "../Vertex/Intercept.h"

/**
 * Path Deformers can replace curve pieces with deformer parts, spanning
 * the phase axis, rather than the typically 'PathDeformed' other
 * dimensions of the TrilinearVertex.
 */
class PathDeformingCurveSampler {
public:

    struct DeformRegion {
        int deformChan;
        float amplitude;
        Intercept start, end;
    };

    PathDeformingCurveSampler(const PathDeformingCurveSampler&) = delete;

};
