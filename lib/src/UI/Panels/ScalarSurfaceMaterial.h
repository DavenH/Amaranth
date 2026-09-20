#pragma once

#include <cstdint>

#include "JuceHeader.h"

enum class ScalarSurfacePalette {
    SignedAmplitude,
    UnipolarMagnitude,
    BipolarPhase
};

struct ScalarSurfaceMaterial {
    static ScalarSurfaceMaterial signedAmplitude();
    static ScalarSurfaceMaterial unipolarMagnitude();
    static ScalarSurfaceMaterial bipolarPhase();

    ScalarSurfacePalette palette { ScalarSurfacePalette::SignedAmplitude };
    juce::Colour negativeAnchor;
    juce::Colour neutralAnchor;
    juce::Colour positiveAnchor;
    float opacity { 1.f };
    float opacityValueScale {};
    float reliefGain { 4.f };
    float diffuseStrength { 0.16f };
    float specularStrength { 0.08f };
    float curvatureThreshold { 0.012f };
    float curvatureSoftness { 0.035f };
    float accentStrength { 0.16f };
    float lightX { -0.55f };
    float lightY { -0.75f };
    int opacityPower { 1 };
};

struct ScalarSurfaceDerivatives {
    float slopeX {};
    float slopeY {};
    float curvature {};
};

struct ScalarSurfaceRenderData {
    const float* values {};
    int valueCount {};
    juce::Rectangle<float> bounds;
    ScalarSurfaceMaterial material;
    uint64_t revision {};
    int columns {};
    int rows {};
    float valueScale { 1.f };
    float valueOffset {};
    bool hasStableRevision {};

    bool isValid() const {
        return columns >= 2
                && rows >= 2
                && values != nullptr
                && valueCount >= columns * rows
                && !bounds.isEmpty();
    }
};

class ScalarSurfaceMaterialEvaluator {
public:
    static ScalarSurfaceDerivatives derivativesAt(
            const float* values,
            int columns,
            int rows,
            int column,
            int row);
    static juce::Colour colourFor(
            float value,
            const ScalarSurfaceDerivatives& derivatives,
            const ScalarSurfaceMaterial& material);
    static juce::Image createImage(
            const float* values,
            int valueCount,
            int columns,
            int rows,
            const ScalarSurfaceMaterial& material,
            bool opaque = false);
    static juce::Image createGradientImage(
            const ScalarSurfaceMaterial& material,
            int width = 512);
};
