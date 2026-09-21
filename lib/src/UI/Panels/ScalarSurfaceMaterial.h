#pragma once

#include <array>
#include <cstdint>

#include "JuceHeader.h"

enum class ScalarSurfacePalette {
    SignedAmplitude,
    UnipolarMagnitude,
    BipolarPhase
};

struct ScalarSurfaceMaterial {
    static constexpr int signedPaletteStopCount = 9;

    static ScalarSurfaceMaterial signedAmplitude();
    static ScalarSurfaceMaterial unipolarMagnitude();
    static ScalarSurfaceMaterial bipolarPhase();

    ScalarSurfacePalette palette { ScalarSurfacePalette::SignedAmplitude };
    juce::Colour negativeAnchor;
    juce::Colour neutralAnchor;
    juce::Colour positiveAnchor;
    std::array<juce::Colour, signedPaletteStopCount> signedPaletteStops;
    std::array<float, signedPaletteStopCount> signedPalettePositions;
    juce::Colour negativePearlTint;
    juce::Colour neutralPearlTint;
    juce::Colour positivePearlTint;
    float opacity { 1.f };
    float opacityValueScale {};
    float normalSampleRadius { 0.012f };
    float largeSampleRadius { 0.045f };
    float reliefScale { 1.8f };
    float ambientStrength { 0.58f };
    float diffuseStrength { 0.52f };
    float specularStrength { 0.20f };
    float pearlTintStrength { 0.72f };
    float shadowStrength { 0.24f };
    float shadowStart { 0.006f };
    float shadowSoftness { 0.12f };
    float cavityStrength { 0.12f };
    float curvatureThreshold { 0.008f };
    float curvatureSoftness { 0.08f };
    float lightX { -0.46f };
    float lightY { -0.54f };
    float lightZ { 0.70f };
    int specularPower { 8 };
    int opacityPower { 1 };
};

struct ScalarSurfaceDerivatives {
    float slopeX {};
    float slopeY {};
    float curvature {};
    float largeCurvature {};
    float horizonShadow {};
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
    static ScalarSurfaceDerivatives derivativesAt(
            const float* values,
            int columns,
            int rows,
            int column,
            int row,
            const ScalarSurfaceMaterial& material,
            float surfaceAspectRatio);
    static juce::Colour baseColourFor(
            float value,
            const ScalarSurfaceMaterial& material);
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
            bool opaque = false,
            float surfaceAspectRatio = 0.f);
    static juce::Image createGradientImage(
            const ScalarSurfaceMaterial& material,
            int width = 512);
};
