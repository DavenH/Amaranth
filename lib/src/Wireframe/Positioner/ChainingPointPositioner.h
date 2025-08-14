#pragma once

#include "PointPositioner.h"

/**
 * Enforces contiguity for audio Oscillator-use curves.
 */
class ChainingPointPositioner : public PointPositioner {
public:
    // should make the points
    void adjustControlPoints(vector<Intercept>& controlPoints, CurveParameters config) override;
};
