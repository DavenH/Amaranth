#pragma once
#include "../Vertex/Intercept.h"
#include "Wireframe/State/RasterizerParameters.h"

class PointPositioner {
public:
    virtual ~PointPositioner() = default;

    virtual void adjustControlPoints(vector<Intercept>& controlPoints, PositionerParameters config) = 0;
};
