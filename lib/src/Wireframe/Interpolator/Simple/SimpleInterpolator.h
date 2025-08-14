#pragma once

#include "../../Vertex/Intercept.h"
#include "../MeshInterpolator.h"

using std::vector;

class SimpleInterpolator : public MeshInterpolator<const vector<Intercept>&> {
public:
    vector<Intercept> interpolate(const vector<Intercept>& arg) override {
        // deep copy?
        return arg;
    }
};