#include "CurveReshapeStrategy.h"

#include <algorithm>
#include <cmath>

float CurveReshapeStrategy::sharpnessDelta(
        float previousPointerY,
        float currentPointerY,
        float controlY,
        float verticalZoom,
        float dragScale,
        float curveScaleY) {
    const float previousDistance = fabsf(previousPointerY - controlY);
    const float currentDistance = fabsf(currentPointerY - controlY);
    const float approach = (previousDistance - currentDistance) / sqrtf(verticalZoom);
    return approach * dragScale / (0.1f + curveScaleY);
}

float CurveReshapeStrategy::applySharpnessDelta(float sharpness, float delta) {
    return std::clamp(sharpness + delta, 0.f, 1.f);
}
