#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "JuceHeader.h"

enum class ScalarSurfacePalette {
    SignedAmplitude,
    UnipolarMagnitude,
    BipolarPhase,
    LegacyBlue,
    Greyscale,
    BipolarMagnitude
};

enum class ScalarSurfaceRelief {
    MicroEmboss,
    MultiscaleTerrain,
    None,
    DirectionalShaded,
    Program13,
    Program14,
    Program15,
    Program16,
    Program17,
    Program18,
    Program19,
    Program20
};

enum class ScalarSurfaceTimeStyle {
    // Stable document values; menu order and labels live in TimeSurfaceStyles.
    Bipolar = 0,
    BlueDepthDirectionalDetail = 1,
    BlueDepth = 2,
    BipolarFlat = 3,
    BipolarShaded = 4,
    IcyHot13 = 5,
    IcyHot14 = 6,
    Greyscale = 7,
    Recipe15 = 8,
    Recipe16 = 9,
    Recipe17 = 10,
    Recipe18 = 11,
    Recipe19 = 12,
    Bullion = 13
};

enum class ScalarSurfaceDetailColour {
    Signed,
    DirectionalCmy
};

struct ScalarSurfaceMaterial {
    static constexpr int signedPaletteStopCount = 9;

    static ScalarSurfaceMaterial signedAmplitude();
    static ScalarSurfaceMaterial signedAmplitudeFlat();
    static ScalarSurfaceMaterial bipolarShaded();
    static ScalarSurfaceMaterial icyHotProgram(bool program14);
    static ScalarSurfaceMaterial savedProgram(int number);
    static ScalarSurfaceMaterial greyscale();
    static ScalarSurfaceMaterial blueDepth();
    static ScalarSurfaceMaterial blueDepthDirectionalDetail();
    static ScalarSurfaceMaterial forTimeStyle(ScalarSurfaceTimeStyle style);
    static ScalarSurfaceMaterial timeDomain();
    static ScalarSurfaceMaterial unipolarMagnitude();
    static ScalarSurfaceMaterial bipolarMagnitude();
    static ScalarSurfaceMaterial bipolarPhase();
    static ScalarSurfaceTimeStyle timeSurfaceStyle();
    static ScalarSurfaceTimeStyle timeSurfaceStyleFromIndex(int index);
    static int timeSurfaceStyleIndex(ScalarSurfaceTimeStyle style);
    static void setTimeSurfaceStyle(ScalarSurfaceTimeStyle style);
    // -1 selects the dedicated bipolar magnitude material.
    static int bipolarMagnitudeStyleIndex();
    static void setBipolarMagnitudeStyleIndex(int styleIndex);
    int detailLowpassScale() const;
    int detailBlurRadius(int sampleCount) const;

    ScalarSurfacePalette palette { ScalarSurfacePalette::SignedAmplitude };
    juce::Colour negativeAnchor;
    juce::Colour neutralAnchor;
    juce::Colour positiveAnchor;
    std::array<juce::Colour, signedPaletteStopCount> signedPaletteStops;
    std::array<float, signedPaletteStopCount> signedPalettePositions;
    juce::Colour negativePearlTint;
    juce::Colour neutralPearlTint;
    juce::Colour positivePearlTint;
    juce::Colour negativeEdgeTint;
    juce::Colour positiveEdgeTint;
    std::array<juce::Colour, 3> directionalDetailColours;
    std::array<float, 3> blurRadii { 0.004f, 0.012f, 0.04f };
    std::array<float, 4> hillshadeWeights { 0.15f, 0.25f, 0.35f, 0.25f };
    std::array<float, 3> obscuranceBiases { 0.002f, 0.006f, 0.012f };
    float opacity { 1.f };
    float opacityValueScale {};
    float reliefScale { 1.15f };
    float ambientStrength { 0.52f };
    float diffuseStrength { 0.54f };
    float specularStrength { 0.045f };
    float pearlTintStrength { 0.16f };
    float obscuranceStrength { 0.24f };
    float obscuranceScale { 4.f };
    float exposureStrength { 0.08f };
    float exposureScale { 2.f };
    float exposureBias { 0.006f };
    float detailReliefScale { 0.25f };
    float embossLimit { 0.08f };
    float embossStrength { 0.06f };
    float detailGradientKnee { 0.02f };
    float detailEnergyKnee { 0.004f };
    float edgeTintStrength { 0.04f };
    float neutralAccentWidth { 0.12f };
    float shadedSlopeScale { 0.7f };
    float shadedHighlightStrength { 0.55f };
    float shadedShadowStrength { 0.22f };
    float lightX { -0.46f };
    float lightY { -0.54f };
    float lightZ { 0.70f };
    int specularPower { 24 };
    int opacityPower { 1 };
    ScalarSurfaceRelief relief { ScalarSurfaceRelief::MultiscaleTerrain };
    ScalarSurfaceDetailColour detailColour { ScalarSurfaceDetailColour::Signed };
};

struct ScalarSurfaceDerivatives {
    std::array<float, 4> slopeX {};
    std::array<float, 4> slopeY {};
    float obscurance {};
    float exposure {};
    float detailSlopeX {};
    float detailSlopeY {};
    float detailEnergy {};
    float boundaryFade { 1.f };
};

struct ScalarSurfaceHeightScales {
    std::vector<float> packedValues;
    int columns {};
    int rows {};

    bool isValid() const {
        return columns >= 2
                && rows >= 2
                && packedValues.size() >= (size_t) columns * (size_t) rows * 4;
    }
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
    static ScalarSurfaceHeightScales createHeightScales(
            const float* values,
            int valueCount,
            int columns,
            int rows,
            const ScalarSurfaceMaterial& material,
            float valueScale = 1.f,
            float valueOffset = 0.f);
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
    static ScalarSurfaceDerivatives derivativesAt(
            const ScalarSurfaceHeightScales& scales,
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
