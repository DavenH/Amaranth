#pragma once

#include "../Vertex/Intercept.h"

using std::vector;

template<typename T>
class MeshInterpolator {
public:
    virtual ~MeshInterpolator() = default;

    virtual vector<Intercept> interpolate(T arg) = 0;
};