#include <catch2/catch_test_macros.hpp>
#include <Binary/Gradients.h>
#include <cmath>
#include <vector>

#include "UI/Panels/ScalarSurfaceMaterial.h"
#include "UI/Panels/GLScalarSurfaceRenderer.h"

namespace {

float colourDistance(juce::Colour left, juce::Colour right) {
    const float red = left.getFloatRed() - right.getFloatRed();
    const float green = left.getFloatGreen() - right.getFloatGreen();
    const float blue = left.getFloatBlue() - right.getFloatBlue();
    return red * red + green * green + blue * blue;
}

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

}

TEST_CASE("Signed scalar surface preserves its semantic colour anchors", "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();
    const ScalarSurfaceDerivatives flat;

    REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(0.f, flat, material)
            .withAlpha(1.f) == material.negativeAnchor.withAlpha(1.f));
    REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(0.5f, flat, material)
            .withAlpha(1.f) == material.neutralAnchor.withAlpha(1.f));
    REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(1.f, flat, material)
            .withAlpha(1.f) == material.positiveAnchor.withAlpha(1.f));

    const auto neutral = material.neutralAnchor;
    float previousNegativeDistance = colourDistance(neutral, material.negativeAnchor);
    for (int step = 1; step <= 32; ++step) {
        const float value = 0.5f * (float) step / 32.f;
        const auto colour = ScalarSurfaceMaterialEvaluator::colourFor(value, flat, material);
        const float distance = colourDistance(neutral, colour);
        REQUIRE(distance <= previousNegativeDistance + 1.e-6f);
        previousNegativeDistance = distance;
    }

    float previousPositiveDistance = 0.f;
    for (int step = 0; step <= 32; ++step) {
        const float value = 0.5f + 0.5f * (float) step / 32.f;
        const auto colour = ScalarSurfaceMaterialEvaluator::colourFor(value, flat, material);
        const float distance = colourDistance(neutral, colour);
        REQUIRE(distance + 1.e-6f >= previousPositiveDistance);
        previousPositiveDistance = distance;
    }

    REQUIRE(neutral.getSaturation() < 0.18f);
}

TEST_CASE("Scalar surface derivatives distinguish planes from local features", "[ui][surface-material]") {
    constexpr int columns = 7;
    constexpr int rows = 7;
    std::vector<float> flat((size_t) columns * (size_t) rows, 0.5f);
    const auto flatDerivatives = derivativesFor(flat, columns, rows, 3, 3);
    REQUIRE(flatDerivatives.slopeX == 0.f);
    REQUIRE(flatDerivatives.slopeY == 0.f);
    REQUIRE(flatDerivatives.curvature == 0.f);

    std::vector<float> ramp(flat.size());
    for (int column = 0; column < columns; ++column) {
        for (int row = 0; row < rows; ++row) {
            ramp[(size_t) column * rows + row] = 0.2f + 0.05f * (float) column;
        }
    }
    const auto rampDerivatives = derivativesFor(ramp, columns, rows, 3, 3);
    REQUIRE(rampDerivatives.slopeX > 0.f);
    REQUIRE(rampDerivatives.curvature > -1.e-6f);
    REQUIRE(rampDerivatives.curvature < 1.e-6f);

    std::vector<float> ridge = flat;
    ridge[(size_t) 3 * rows + 3] = 0.8f;
    REQUIRE(derivativesFor(ridge, columns, rows, 3, 3).curvature < 0.f);
    REQUIRE(derivativesFor(ridge, columns, rows, 1, 1).curvature == 0.f);

    std::vector<float> valley = flat;
    valley[(size_t) 3 * rows + 3] = 0.2f;
    REQUIRE(derivativesFor(valley, columns, rows, 3, 3).curvature > 0.f);
    REQUIRE(derivativesFor(valley, columns, rows, 5, 5).curvature == 0.f);
}

TEST_CASE("Relief separates ridges and valleys without changing hue", "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();
    const ScalarSurfaceDerivatives flat;
    ScalarSurfaceDerivatives ripple;
    ripple.curvature = material.curvatureThreshold * 0.5f;

    const auto base = ScalarSurfaceMaterialEvaluator::colourFor(0.75f, flat, material);
    const auto belowThreshold = ScalarSurfaceMaterialEvaluator::colourFor(0.75f, ripple, material);
    REQUIRE(base == belowThreshold);

    ripple.curvature = -(material.curvatureThreshold + material.curvatureSoftness);
    const auto ridge = ScalarSurfaceMaterialEvaluator::colourFor(0.75f, ripple, material);
    ripple.curvature = material.curvatureThreshold + material.curvatureSoftness;
    const auto valley = ScalarSurfaceMaterialEvaluator::colourFor(0.75f, ripple, material);
    REQUIRE(ridge.getBrightness() > base.getBrightness());
    REQUIRE(valley.getBrightness() < base.getBrightness());
    REQUIRE(std::abs(ridge.getHue() - base.getHue()) < 0.01f);
    REQUIRE(std::abs(valley.getHue() - base.getHue()) < 0.01f);
}

TEST_CASE("Spectral magnitude retains the legacy burnt alum palette", "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::unipolarMagnitude();
    const ScalarSurfaceDerivatives flat;
    const juce::Image legacy = juce::PNGImageFormat::loadFrom(
            Gradients::burntalum_png,
            Gradients::burntalum_pngSize);

    REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(0.f, flat, material)
            .withAlpha(1.f) == legacy.getPixelAt(0, 0).withAlpha(1.f));
    REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(0.5f, flat, material)
            .withAlpha(1.f) == legacy.getPixelAt(256, 0).withAlpha(1.f));
    REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(1.f, flat, material)
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
    data.material.reliefGain += 1.f;
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
