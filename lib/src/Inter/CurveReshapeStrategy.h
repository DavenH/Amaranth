#pragma once

class CurveReshapeStrategy {
public:
    static float sharpnessDelta(
            float previousPointerY,
            float currentPointerY,
            float curvePole,
            float verticalZoom,
            float dragScale,
            float curveScaleY);

    static float applySharpnessDelta(float sharpness, float delta);
    static float hiddenDimensionScale(
            bool pairedVertexMoves,
            float morphValue,
            float nearValue,
            float farValue);
};
