#include "CurveReshapeStrategy.h"

#include <algorithm>
#include <cmath>

float CurveReshapeStrategy::sharpnessDelta(
        float previousPointerY,
        float currentPointerY,
        float curvePole,
        float verticalZoom,
        float dragScale,
        float curveScaleY) {
    const float movement = currentPointerY - previousPointerY;
    return movement * curvePole * dragScale
            / (sqrtf(verticalZoom) * (0.1f + curveScaleY));
}

float CurveReshapeStrategy::applySharpnessDelta(float sharpness, float delta) {
    return std::clamp(sharpness + delta, 0.f, 1.f);
}

float CurveReshapeStrategy::hiddenDimensionScale(
        bool pairedVertexMoves,
        float morphValue,
        float nearValue,
        float farValue) {
    if (pairedVertexMoves) {
        return 1.f;
    }

    const float maximumDistance = std::max(
            fabsf(morphValue - nearValue),
            fabsf(morphValue - farValue));
    return maximumDistance < 0.0001f
            ? 1.f
            : fabsf(nearValue - farValue) / maximumDistance;
}
