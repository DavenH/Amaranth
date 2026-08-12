#include "CurveReshapeStrategy.h"

#include <algorithm>
#include <cmath>

float CurveReshapeStrategy::sharpnessDelta(
        float gestureStartY,
        float previousPointerY,
        float currentPointerY,
        float controlY,
        float verticalZoom,
        float dragScale,
        float curveScaleY) {
    const float directionTowardControl = gestureStartY <= controlY ? 1.f : -1.f;
    const float movement = currentPointerY - previousPointerY;
    return movement * directionTowardControl * dragScale
            / (sqrtf(verticalZoom) * (0.1f + curveScaleY));
}

float CurveReshapeStrategy::applySharpnessDelta(float sharpness, float delta) {
    return std::clamp(sharpness + delta, 0.f, 1.f);
}
