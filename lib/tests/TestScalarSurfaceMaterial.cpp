#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <Binary/Gradients.h>
#include <cmath>
#include <vector>

#include "UI/Panels/ScalarSurfaceMaterial.h"
#include "UI/Panels/GLScalarSurfaceRenderer.h"

namespace {

ScalarSurfaceDerivatives derivativesFor(
        std::vector<float>& values,
        int columns,
        int rows,
        int column,
        int row) {
    return ScalarSurfaceMaterialEvaluator::derivativesAt(
            values.data(),
            columns,
            rows,
            column,
            row);
}

template<typename Sample>
std::vector<float> extrudedSurface(int columns, int rows, const Sample& sample) {
    std::vector<float> values((size_t) columns * (size_t) rows);
    for (int column = 0; column < columns; ++column) {
        const float x = (float) column / (float) (columns - 1);
        for (int row = 0; row < rows; ++row) {
            values[(size_t) column * rows + row] = sample(x);
        }
    }
    return values;
}

int maximumChannelDifference(juce::Colour first, juce::Colour second) {
    return juce::jmax(
            std::abs((int) first.getRed() - (int) second.getRed()),
            std::abs((int) first.getGreen() - (int) second.getGreen()),
            std::abs((int) first.getBlue() - (int) second.getBlue()));
}

float meanDetailEnergy(
        const std::vector<float>& values,
        int columns,
        int rows,
        const ScalarSurfaceMaterial& material) {
    const ScalarSurfaceHeightScales scales = ScalarSurfaceMaterialEvaluator::createHeightScales(
            values.data(), (int) values.size(), columns, rows, material);
    float total = 0.f;
    for (int column = 8; column < columns - 8; ++column) {
        total += ScalarSurfaceMaterialEvaluator::derivativesAt(
                scales, column, rows / 2, material, 1.f).detailEnergy;
    }
    return total / (float) juce::jmax(1, columns - 16);
}

}

TEST_CASE("Time scalar surface uses continuous bipolar semantics", "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();

    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(0.f, material)
            .withAlpha(1.f) == material.negativeAnchor.withAlpha(1.f));
    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(1.f, material)
            .withAlpha(1.f) == material.positiveAnchor.withAlpha(1.f));
    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(0.5f, material)
            .withAlpha(1.f) == material.neutralAnchor.withAlpha(1.f));
    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(0.25f, material).getBlue()
            > ScalarSurfaceMaterialEvaluator::baseColourFor(0.25f, material).getRed());
    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(0.75f, material).getRed()
            > ScalarSurfaceMaterialEvaluator::baseColourFor(0.75f, material).getBlue());

    constexpr int side = 4;
    const std::vector<float> constantSurface(side * side, 0.375f);
    const juce::Image image = ScalarSurfaceMaterialEvaluator::createImage(
            constantSurface.data(),
            (int) constantSurface.size(),
            side,
            side,
            material);
    const juce::Colour expected = image.getPixelAt(0, 0);
    for (int x = 0; x < side; ++x) {
        for (int y = 0; y < side; ++y) {
            REQUIRE(image.getPixelAt(x, y) == expected);
        }
    }
}

TEST_CASE("Time palette has no zero-crossing luminance trench",
        "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();
    const juce::Colour below = ScalarSurfaceMaterialEvaluator::baseColourFor(0.49f, material);
    const juce::Colour neutral = ScalarSurfaceMaterialEvaluator::baseColourFor(0.5f, material);
    const juce::Colour above = ScalarSurfaceMaterialEvaluator::baseColourFor(0.51f, material);

    const juce::Colour justBelow = ScalarSurfaceMaterialEvaluator::baseColourFor(0.499f, material);
    const juce::Colour justAbove = ScalarSurfaceMaterialEvaluator::baseColourFor(0.501f, material);

    REQUIRE(below.getPerceivedBrightness() > neutral.getPerceivedBrightness());
    REQUIRE(above.getPerceivedBrightness() > neutral.getPerceivedBrightness());
    REQUIRE(maximumChannelDifference(justBelow, neutral) < 2);
    REQUIRE(maximumChannelDifference(justAbove, neutral) < 2);
    REQUIRE(maximumChannelDifference(justBelow, justAbove) < 4);
}

TEST_CASE("Blue time surface is smooth monotonic depth with inferno detail",
        "[ui][surface-material]") {
    ScalarSurfaceMaterial material = ScalarSurfaceMaterial::blueDepthWarmDetail();
    REQUIRE(material.detailColour == ScalarSurfaceDetailColour::Inferno);

    float previousBrightness = -1.f;
    float maximumBrightnessStep = 0.f;
    for (int index = 0; index < 512; ++index) {
        const juce::Colour colour = ScalarSurfaceMaterialEvaluator::baseColourFor(
                (float) index / 511.f,
                material);
        const float brightness = colour.getPerceivedBrightness();
        REQUIRE(brightness + 0.0001f >= previousBrightness);
        if (index > 0) {
            maximumBrightnessStep = juce::jmax(
                    maximumBrightnessStep,
                    brightness - previousBrightness);
        }
        previousBrightness = brightness;
    }
    REQUIRE(maximumBrightnessStep < 0.02f);
    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(0.5f, material).getSaturation()
            < 0.20f);

    constexpr int columns = 512;
    constexpr int rows = 8;
    const auto ramp = extrudedSurface(columns, rows, [](float x) { return x; });
    const juce::Image rampImage = ScalarSurfaceMaterialEvaluator::createImage(
            ramp.data(), (int) ramp.size(), columns, rows, material, true, 2.f);
    for (int column = 12; column < columns - 12; column += 17) {
        const juce::Colour expected = ScalarSurfaceMaterialEvaluator::baseColourFor(
                ramp[(size_t) column * rows], material).withAlpha(1.f);
        REQUIRE(maximumChannelDifference(
                rampImage.getPixelAt(column, rows / 2), expected) <= 1);
    }

    material.embossStrength = 0.f;
    const ScalarSurfaceDerivatives flat;
    ScalarSurfaceDerivatives detail;
    detail.detailSlopeX = 0.2f;
    detail.detailSlopeY = 0.2f;
    detail.detailEnergy = material.detailEnergyHigh;
    const juce::Colour base = ScalarSurfaceMaterialEvaluator::baseColourFor(0.2f, material);
    const juce::Colour unaccented = ScalarSurfaceMaterialEvaluator::colourFor(
            0.2f,
            flat,
            material);
    const juce::Colour accented = ScalarSurfaceMaterialEvaluator::colourFor(
            0.2f,
            detail,
            material);
    REQUIRE(unaccented == base);
    REQUIRE(accented.getRed() > unaccented.getRed());
    REQUIRE(accented.getBlue() >= unaccented.getBlue());
}

TEST_CASE("Time surface style selection changes only the time material",
        "[ui][surface-material]") {
    const ScalarSurfaceTimeStyle previous = ScalarSurfaceMaterial::timeSurfaceStyle();
    ScalarSurfaceMaterial::setTimeSurfaceStyle(ScalarSurfaceTimeStyle::Bipolar);
    const ScalarSurfaceMaterial bipolar = ScalarSurfaceMaterial::timeDomain();
    ScalarSurfaceMaterial::setTimeSurfaceStyle(
            ScalarSurfaceTimeStyle::BlueDepthWarmDetail);
    const ScalarSurfaceMaterial blue = ScalarSurfaceMaterial::timeDomain();
    ScalarSurfaceMaterial::setTimeSurfaceStyle(previous);

    REQUIRE(bipolar.detailColour == ScalarSurfaceDetailColour::Signed);
    REQUIRE(blue.detailColour == ScalarSurfaceDetailColour::Inferno);
    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(0.75f, bipolar)
            != ScalarSurfaceMaterialEvaluator::baseColourFor(0.75f, blue));
    REQUIRE(ScalarSurfaceMaterial::unipolarMagnitude().palette
            == ScalarSurfacePalette::UnipolarMagnitude);
}

TEST_CASE("Micro emboss rejects broad planes and boundary bias", "[ui][surface-material]") {
    constexpr int columns = 7;
    constexpr int rows = 7;
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();
    std::vector<float> flat((size_t) columns * (size_t) rows, 0.5f);
    const auto flatDerivatives = derivativesFor(flat, columns, rows, 3, 3);
    REQUIRE(flatDerivatives.slopeX[0] == 0.f);
    REQUIRE(flatDerivatives.slopeY[0] == 0.f);
    REQUIRE(flatDerivatives.detailSlopeX == 0.f);
    REQUIRE(flatDerivatives.detailSlopeY == 0.f);
    REQUIRE(flatDerivatives.detailEnergy == 0.f);

    std::vector<float> ramp(flat.size());
    for (int column = 0; column < columns; ++column) {
        for (int row = 0; row < rows; ++row) {
            ramp[(size_t) column * rows + row] = 0.2f + 0.05f * (float) column;
        }
    }
    const auto rampDerivatives = derivativesFor(ramp, columns, rows, 3, 3);
    const auto edgeDerivatives = derivativesFor(ramp, columns, rows, 0, 3);
    REQUIRE(rampDerivatives.slopeX[0] > 0.f);
    REQUIRE(rampDerivatives.slopeY[0] == 0.f);
    REQUIRE(edgeDerivatives.slopeX[0]
            == Catch::Approx(rampDerivatives.slopeX[0]).margin(0.0001f));
    REQUIRE(rampDerivatives.detailSlopeX == Catch::Approx(0.f).margin(0.0001f));
    REQUIRE(edgeDerivatives.boundaryFade == 0.f);

    std::vector<float> ridge = flat;
    ridge[(size_t) 3 * rows + 3] = 0.8f;
    REQUIRE(derivativesFor(ridge, columns, rows, 3, 3).detailEnergy > 0.f);

    const juce::Image constantImage = ScalarSurfaceMaterialEvaluator::createImage(
            flat.data(), (int) flat.size(), columns, rows, material, true, 1.f);
    const juce::Colour expected = ScalarSurfaceMaterialEvaluator::baseColourFor(0.5f, material)
            .withAlpha(1.f);
    for (int x = 0; x < constantImage.getWidth(); ++x) {
        for (int y = 0; y < constantImage.getHeight(); ++y) {
            REQUIRE(constantImage.getPixelAt(x, y) == expected);
        }
    }
}

TEST_CASE("Micro emboss responds only to high-pass detail",
        "[ui][surface-material]") {
    ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();
    material.edgeTintStrength = 0.f;
    const ScalarSurfaceDerivatives flat;
    ScalarSurfaceDerivatives towardLight;
    towardLight.detailSlopeX = 0.2f;
    towardLight.detailSlopeY = 0.2f;
    ScalarSurfaceDerivatives awayFromLight;
    awayFromLight.detailSlopeX = -0.2f;
    awayFromLight.detailSlopeY = -0.2f;

    const juce::Colour lit = ScalarSurfaceMaterialEvaluator::colourFor(0.75f, towardLight, material);
    const juce::Colour shaded = ScalarSurfaceMaterialEvaluator::colourFor(
            0.75f, awayFromLight, material);
    REQUIRE(lit.getPerceivedBrightness() > shaded.getPerceivedBrightness());
    REQUIRE(lit.getRed() > lit.getBlue());
    REQUIRE(shaded.getRed() > shaded.getBlue());

    const juce::Colour negative = ScalarSurfaceMaterialEvaluator::colourFor(
            0.25f, towardLight, material);
    REQUIRE(negative.getBlue() > negative.getRed());
    ScalarSurfaceDerivatives broadSlope;
    broadSlope.slopeX.fill(0.4f);
    broadSlope.slopeY.fill(0.4f);
    REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(0.75f, broadSlope, material)
            == ScalarSurfaceMaterialEvaluator::colourFor(0.75f, flat, material));
}

TEST_CASE("Smooth analytical signals remain colour-dominant", "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();
    constexpr int columns = 512;
    constexpr int rows = 8;
    const auto sine = extrudedSurface(columns, rows, [](float x) {
        return 0.5f + 0.35f * std::sin(juce::MathConstants<float>::twoPi * 3.f * x);
    });
    const juce::Image image = ScalarSurfaceMaterialEvaluator::createImage(
            sine.data(), (int) sine.size(), columns, rows, material, true, 2.f);
    for (int column = 8; column < columns - 8; column += 11) {
        const juce::Colour base = ScalarSurfaceMaterialEvaluator::baseColourFor(
                sine[(size_t) column * rows], material).withAlpha(1.f);
        REQUIRE(maximumChannelDifference(image.getPixelAt(column, rows / 2), base) <= 10);
    }
}

TEST_CASE("Micro detail is stable across resolution and emphasizes weak ripple",
        "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();
    const auto signal = [](int columns, bool ripple) {
        return extrudedSurface(columns, 8, [ripple](float x) {
            const float broad = 0.28f * std::sin(
                    juce::MathConstants<float>::twoPi * 3.f * x);
            const float detail = ripple ? 0.018f * std::sin(
                    juce::MathConstants<float>::twoPi * 48.f * x) : 0.f;
            return 0.5f + broad + detail;
        });
    };
    const auto broad512 = signal(512, false);
    const auto mixed512 = signal(512, true);
    const auto mixed1024 = signal(1024, true);
    const float broadEnergy = meanDetailEnergy(broad512, 512, 8, material);
    const float detail512 = meanDetailEnergy(mixed512, 512, 8, material);
    const float detail1024 = meanDetailEnergy(mixed1024, 1024, 8, material);

    REQUIRE(detail512 > broadEnergy * 3.f);
    REQUIRE(detail1024 == Catch::Approx(detail512).epsilon(0.4f));
}

TEST_CASE("Spectral magnitude retains the legacy burnt alum palette", "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::unipolarMagnitude();
    const juce::Image legacy = juce::PNGImageFormat::loadFrom(
            Gradients::burntalum_png,
            Gradients::burntalum_pngSize);

    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(0.f, material)
            .withAlpha(1.f) == legacy.getPixelAt(0, 0).withAlpha(1.f));
    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(0.5f, material)
            .withAlpha(1.f) == legacy.getPixelAt(256, 0).withAlpha(1.f));
    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(1.f, material)
            .withAlpha(1.f) == legacy.getPixelAt(511, 0).withAlpha(1.f));
}

TEST_CASE("Scalar texture uploads depend only on product identity and transform",
        "[ui][surface-material][performance]") {
    std::vector<float> values(64, 0.5f);
    ScalarSurfaceRenderData data;
    data.values = values.data();
    data.valueCount = (int) values.size();
    data.bounds = { 0.f, 0.f, 100.f, 80.f };
    data.revision = 7;
    data.columns = 8;
    data.rows = 8;
    data.hasStableRevision = true;

    ScalarSurfaceUploadState state;
    REQUIRE(state.needsUpload(data));
    state.markUploaded(data);
    REQUIRE_FALSE(state.needsUpload(data));

    data.bounds = { 0.f, 0.f, 220.f, 140.f };
    data.material.reliefScale += 1.f;
    REQUIRE_FALSE(state.needsUpload(data));

    ++data.revision;
    REQUIRE(state.needsUpload(data));
    state.markUploaded(data);
    data.valueOffset = 0.5f;
    REQUIRE(state.needsUpload(data));

    data.valueOffset = 0.f;
    data.hasStableRevision = false;
    state.markUploaded(data);
    REQUIRE(state.needsUpload(data));
}
