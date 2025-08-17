#pragma once

#include "../../Vertex/Intercept.h"
#include "../Interpolator.h"

using std::vector;

class SimpleInterpolator : public Interpolator<const vector<Intercept>&, Intercept> {
public:
    vector<Intercept> interpolate(const vector<Intercept>& arg, const InterpolatorParameters& params) override {
        return arg;
    }
};