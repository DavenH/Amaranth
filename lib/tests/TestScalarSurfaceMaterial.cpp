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

}

TEST_CASE("Signed scalar surface uses a scalar-only pearl palette", "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();

    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(0.f, material)
            .withAlpha(1.f) == material.negativeAnchor.withAlpha(1.f));
    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(0.5f, material)
            .withAlpha(1.f) == material.neutralAnchor.withAlpha(1.f));
    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(1.f, material)
            .withAlpha(1.f) == material.positiveAnchor.withAlpha(1.f));

    const auto troughShoulder = ScalarSurfaceMaterialEvaluator::baseColourFor(0.34f, material);
    const auto warmShoulder = ScalarSurfaceMaterialEvaluator::baseColourFor(0.75f, material);
    REQUIRE(troughShoulder.getBrightness() > material.negativeAnchor.getBrightness());
    REQUIRE(troughShoulder.getBrightness() > material.neutralAnchor.getBrightness());
    REQUIRE(troughShoulder.getBlue() > troughShoulder.getRed());
    REQUIRE(warmShoulder.getRed() > warmShoulder.getBlue());
    REQUIRE(material.positiveAnchor.getBrightness() > warmShoulder.getBrightness());
    REQUIRE(material.neutralAnchor.getBrightness() < 0.24f);
    REQUIRE(material.neutralAnchor.getSaturation() < 0.28f);

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

TEST_CASE("Signed palette gives zero crossings a narrow semantic transition",
        "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();
    const juce::Colour coolDusk = ScalarSurfaceMaterialEvaluator::baseColourFor(0.465f, material);
    const juce::Colour neutral = ScalarSurfaceMaterialEvaluator::baseColourFor(0.5f, material);
    const juce::Colour warmDusk = ScalarSurfaceMaterialEvaluator::baseColourFor(0.535f, material);
    const juce::Colour justNegative = ScalarSurfaceMaterialEvaluator::baseColourFor(0.499f, material);
    const juce::Colour justPositive = ScalarSurfaceMaterialEvaluator::baseColourFor(0.501f, material);

    REQUIRE(coolDusk.getBrightness() > neutral.getBrightness());
    REQUIRE(warmDusk.getBrightness() > neutral.getBrightness());
    REQUIRE(coolDusk.getBlue() > coolDusk.getRed());
    REQUIRE(warmDusk.getRed() > warmDusk.getBlue());
    REQUIRE(std::abs((int) justNegative.getRed() - (int) justPositive.getRed()) < 8);
    REQUIRE(std::abs((int) justNegative.getBlue() - (int) justPositive.getBlue()) < 8);
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

TEST_CASE("Pseudo-normal lighting responds to orientation and semantic hue",
        "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();
    const ScalarSurfaceDerivatives flat;
    ScalarSurfaceDerivatives towardLight;
    towardLight.slopeX = 0.2f;
    towardLight.slopeY = 0.2f;
    ScalarSurfaceDerivatives awayFromLight;
    awayFromLight.slopeX = -0.2f;
    awayFromLight.slopeY = -0.2f;

    const juce::Colour lit = ScalarSurfaceMaterialEvaluator::colourFor(0.75f, towardLight, material);
    const juce::Colour shaded = ScalarSurfaceMaterialEvaluator::colourFor(
            0.75f, awayFromLight, material);
    REQUIRE(lit.getPerceivedBrightness() > shaded.getPerceivedBrightness());
    REQUIRE(lit.getRed() > lit.getBlue());
    REQUIRE(shaded.getRed() > shaded.getBlue());

    const juce::Colour negative = ScalarSurfaceMaterialEvaluator::colourFor(
            0.25f, towardLight, material);
    REQUIRE(negative.getBlue() > negative.getRed());
    REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(0.75f, flat, material)
            == ScalarSurfaceMaterialEvaluator::colourFor(0.75f, flat, material));

    ScalarSurfaceMaterial specularOnly = material;
    specularOnly.diffuseStrength = 0.f;
    specularOnly.shadowStrength = 0.f;
    specularOnly.cavityStrength = 0.f;
    specularOnly.specularStrength = 0.45f;
    REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(0.75f, towardLight, specularOnly)
            .getPerceivedBrightness()
            > ScalarSurfaceMaterialEvaluator::colourFor(0.75f, awayFromLight, specularOnly)
                    .getPerceivedBrightness());
}

TEST_CASE("Broad scalar relief is stable across grid resolution", "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();
    const auto planeDerivatives = [&material](int side) {
        std::vector<float> values((size_t) side * (size_t) side);
        for (int column = 0; column < side; ++column) {
            const float x = (float) column / (float) (side - 1);
            for (int row = 0; row < side; ++row) {
                values[(size_t) column * side + row] = 0.35f + 0.3f * x;
            }
        }
        return ScalarSurfaceMaterialEvaluator::derivativesAt(
                values.data(), side, side, side / 2, side / 2, material, 1.f);
    };

    const ScalarSurfaceDerivatives lowResolution = planeDerivatives(33);
    const ScalarSurfaceDerivatives highResolution = planeDerivatives(129);
    REQUIRE(lowResolution.slopeX == Catch::Approx(highResolution.slopeX).margin(0.015f));
    REQUIRE(lowResolution.slopeY == Catch::Approx(0.f).margin(0.0001f));
    REQUIRE(highResolution.slopeY == Catch::Approx(0.f).margin(0.0001f));
}

TEST_CASE("Relief is offset invariant and horizon shadow darkens valleys",
        "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::signedAmplitude();
    ScalarSurfaceDerivatives unshadowed;
    ScalarSurfaceDerivatives shadowed;
    shadowed.horizonShadow = 1.f;
    REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(0.5f, shadowed, material)
            .getPerceivedBrightness()
            < ScalarSurfaceMaterialEvaluator::colourFor(0.5f, unshadowed, material)
                    .getPerceivedBrightness());

    constexpr int side = 33;
    std::vector<float> original((size_t) side * side);
    std::vector<float> offset((size_t) side * side);
    for (int column = 0; column < side; ++column) {
        for (int row = 0; row < side; ++row) {
            const float value = 0.2f + 0.3f * (float) column / (float) (side - 1);
            original[(size_t) column * side + row] = value;
            offset[(size_t) column * side + row] = value + 0.2f;
        }
    }
    const auto first = ScalarSurfaceMaterialEvaluator::derivativesAt(
            original.data(), side, side, side / 2, side / 2, material, 1.f);
    const auto second = ScalarSurfaceMaterialEvaluator::derivativesAt(
            offset.data(), side, side, side / 2, side / 2, material, 1.f);
    REQUIRE(first.slopeX == Catch::Approx(second.slopeX).margin(0.0001f));
    REQUIRE(first.slopeY == Catch::Approx(second.slopeY).margin(0.0001f));
    REQUIRE(first.curvature == Catch::Approx(second.curvature).margin(0.0001f));
    REQUIRE(first.largeCurvature == Catch::Approx(second.largeCurvature).margin(0.0001f));
    REQUIRE(first.horizonShadow == Catch::Approx(second.horizonShadow).margin(0.0001f));

    std::vector<float> ridge((size_t) side * side, 0.4f);
    for (int row = 0; row < side; ++row) {
        ridge[(size_t) (side / 2) * side + row] = 0.9f;
    }
    const auto opposedSide = ScalarSurfaceMaterialEvaluator::derivativesAt(
            ridge.data(), side, side, side / 2 - 2, side / 2, material, 1.f);
    const auto lightSide = ScalarSurfaceMaterialEvaluator::derivativesAt(
            ridge.data(), side, side, side / 2 + 2, side / 2, material, 1.f);
    REQUIRE(opposedSide.horizonShadow > lightSide.horizonShadow);
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
