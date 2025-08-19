#pragma once
#include "../Vertex/Intercept.h"
#include "Wireframe/Rasterizer/RasterizerParams.h"

class PointPositioner {
public:
    virtual ~PointPositioner() = default;

    virtual void adjustControlPoints(vector<Intercept>& controlPoints, PositionerParameters config) = 0;
};
