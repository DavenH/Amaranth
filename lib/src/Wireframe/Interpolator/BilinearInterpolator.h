#pragma once
#include "Trilinear/TrilinearInterpolator.h"

/**
 * This is what
 */
class BilinearInterpolator : public TrilinearInterpolator {
public:
    vector<Intercept> interpolate(Mesh* mesh, const RasterizerParams& params) override {
        morph.time = 0;
        return TrilinearInterpolator::interpolate(mesh, params);
    }

};
