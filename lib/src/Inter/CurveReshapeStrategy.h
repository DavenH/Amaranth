#pragma once

class CurveReshapeStrategy {
public:
    static float sharpnessDelta(
            float previousPointerY,
            float currentPointerY,
            float controlY,
            float verticalZoom,
            float dragScale,
            float curveScaleY);

    static float applySharpnessDelta(float sharpness, float delta);
};
