#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <Binary/Gradients.h>
#include <cmath>
#include <vector>

#include "UI/Panels/ScalarSurfaceMaterial.h"
#include "UI/Panels/GLScalarSurfaceRenderer.h"
#include "Array/Buffer.h"

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

ScalarSurfaceDerivatives exaggeratedDerivatives() {
    ScalarSurfaceDerivatives derivatives;
    derivatives.slopeX.fill(100.f);
    derivatives.slopeY.fill(-100.f);
    derivatives.obscurance = 1.f;
    derivatives.exposure = 1.f;
    derivatives.detailSlopeX = 100.f;
    derivatives.detailSlopeY = -100.f;
    derivatives.detailEnergy = 1.f;
    derivatives.boundaryFade = 1.f;
    return derivatives;
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

TEST_CASE("Blue time surface is smooth monotonic depth with directional detail",
        "[ui][surface-material]") {
    ScalarSurfaceMaterial material = ScalarSurfaceMaterial::blueDepthDirectionalDetail();
    REQUIRE(material.detailColour == ScalarSurfaceDetailColour::DirectionalCmy);

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
    detail.detailEnergy = material.detailEnergyKnee * 4.f;
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
            ScalarSurfaceTimeStyle::BlueDepthDirectionalDetail);
    const ScalarSurfaceMaterial directional = ScalarSurfaceMaterial::timeDomain();
    ScalarSurfaceMaterial::setTimeSurfaceStyle(ScalarSurfaceTimeStyle::BlueDepth);
    const ScalarSurfaceMaterial blue = ScalarSurfaceMaterial::timeDomain();
    ScalarSurfaceMaterial::setTimeSurfaceStyle(ScalarSurfaceTimeStyle::BipolarFlat);
    const ScalarSurfaceMaterial bipolarFlat = ScalarSurfaceMaterial::timeDomain();
    ScalarSurfaceMaterial::setTimeSurfaceStyle(previous);

    REQUIRE(bipolar.detailColour == ScalarSurfaceDetailColour::Signed);
    REQUIRE(directional.detailColour == ScalarSurfaceDetailColour::DirectionalCmy);
    REQUIRE(blue.relief == ScalarSurfaceRelief::None);
    REQUIRE(bipolarFlat.relief == ScalarSurfaceRelief::None);
    REQUIRE(ScalarSurfaceMaterial::timeSurfaceStyleIndex(
            ScalarSurfaceTimeStyle::Bipolar) == 0);
    REQUIRE(ScalarSurfaceMaterial::timeSurfaceStyleIndex(
            ScalarSurfaceTimeStyle::BlueDepthDirectionalDetail) == 1);
    REQUIRE(ScalarSurfaceMaterial::timeSurfaceStyleIndex(
            ScalarSurfaceTimeStyle::BlueDepth) == 2);
    REQUIRE(ScalarSurfaceMaterial::timeSurfaceStyleIndex(
            ScalarSurfaceTimeStyle::BipolarFlat) == 3);
    REQUIRE(ScalarSurfaceMaterial::timeSurfaceStyleFromIndex(3)
            == ScalarSurfaceTimeStyle::BipolarFlat);
    REQUIRE(ScalarSurfaceMaterial::timeSurfaceStyleFromIndex(99)
            == ScalarSurfaceTimeStyle::BlueDepthDirectionalDetail);
    REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(0.75f, bipolar)
            != ScalarSurfaceMaterialEvaluator::baseColourFor(0.75f, blue));
    REQUIRE(ScalarSurfaceMaterial::unipolarMagnitude().palette
            == ScalarSurfacePalette::UnipolarMagnitude);
}

TEST_CASE("Flat bipolar maps value directly to colour", "[ui][surface-material]") {
    const ScalarSurfaceMaterial detailed = ScalarSurfaceMaterial::signedAmplitude();
    const ScalarSurfaceMaterial flat = ScalarSurfaceMaterial::signedAmplitudeFlat();
    const ScalarSurfaceDerivatives derivatives = exaggeratedDerivatives();

    REQUIRE(flat.relief == ScalarSurfaceRelief::None);
    REQUIRE(flat.palette == ScalarSurfacePalette::SignedAmplitude);
    REQUIRE(flat.embossStrength == 0.f);
    REQUIRE(flat.edgeTintStrength == 0.f);
    for (const float value: { 0.f, 0.25f, 0.5f, 0.75f, 1.f }) {
        REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(value, flat)
                == ScalarSurfaceMaterialEvaluator::baseColourFor(value, detailed));
        REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(value, derivatives, flat)
                == ScalarSurfaceMaterialEvaluator::baseColourFor(value, flat));
    }
}

TEST_CASE("Bipolar shaded lighting follows direction continuously", "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::bipolarShaded();
    REQUIRE(ScalarSurfaceMaterial::timeSurfaceStyleFromIndex(4)
            == ScalarSurfaceTimeStyle::BipolarShaded);
    const ScalarSurfaceTimeStyle previous = ScalarSurfaceMaterial::timeSurfaceStyle();
    ScalarSurfaceMaterial::setTimeSurfaceStyle(ScalarSurfaceTimeStyle::BipolarShaded);
    const auto selected = ScalarSurfaceMaterial::timeDomain();
    ScalarSurfaceMaterial::setTimeSurfaceStyle(previous);
    REQUIRE(selected.relief == ScalarSurfaceRelief::DirectionalShaded);

    for (const float value: { 0.1f, 0.5f, 0.9f }) {
        const auto base = ScalarSurfaceMaterialEvaluator::baseColourFor(value, material);
        ScalarSurfaceDerivatives derivatives;
        REQUIRE(maximumChannelDifference(base,
                ScalarSurfaceMaterialEvaluator::colourFor(value, derivatives, material)) <= 1);
        derivatives.detailSlopeX = 4.f;
        derivatives.detailEnergy = 0.01f;
        const auto lit = ScalarSurfaceMaterialEvaluator::colourFor(value, derivatives, material);
        derivatives.detailSlopeX = -4.f;
        const auto shaded = ScalarSurfaceMaterialEvaluator::colourFor(value, derivatives, material);
        REQUIRE(lit.getPerceivedBrightness() > base.getPerceivedBrightness());
        REQUIRE(shaded.getPerceivedBrightness() < base.getPerceivedBrightness());
        if (value < 0.5f) {
            REQUIRE(lit.getBlue() > lit.getRed());
        } else if (value > 0.5f) {
            REQUIRE(lit.getRed() > lit.getBlue());
        }
    }

    juce::Colour previousColour;
    for (int index = 0; index <= 200; ++index) {
        ScalarSurfaceDerivatives derivatives;
        derivatives.detailSlopeX = -2.f + 0.02f * (float) index;
        derivatives.detailEnergy = 0.01f;
        const auto colour = ScalarSurfaceMaterialEvaluator::colourFor(0.3f, derivatives, material);
        if (index > 0) {
            REQUIRE(maximumChannelDifference(colour, previousColour) <= 2);
        }
        previousColour = colour;
    }
}

TEST_CASE("Copper ice shading preserves its palette without a pearl overlay", "[ui][surface-material]") {
    auto material = ScalarSurfaceMaterial::bipolarShaded();
    ScalarSurfaceDerivatives derivatives;
    derivatives.detailSlopeX = 8.f;
    derivatives.detailEnergy = 0.01f;
    const auto lit = ScalarSurfaceMaterialEvaluator::colourFor(0.5f, derivatives, material);
    material.neutralPearlTint = juce::Colours::magenta;
    material.negativePearlTint = juce::Colours::yellow;
    material.positivePearlTint = juce::Colours::green;
    REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(0.5f, derivatives, material) == lit);
    REQUIRE(lit.getBlue() > lit.getRed());
    float previousBrightness = 0.f;
    for (int index = 0; index < 512; ++index) {
        const auto base = ScalarSurfaceMaterialEvaluator::baseColourFor((float) index / 511.f, material);
        // Independent 8-bit channel rounding can introduce sub-code dips as hue turns.
        REQUIRE(base.getPerceivedBrightness() + 1.f / 255.f >= previousBrightness);
        previousBrightness = juce::jmax(previousBrightness, base.getPerceivedBrightness());
    }
}

TEST_CASE("Copper ice accent rejects broad form and follows real ripple detail", "[ui][surface-material]") {
    const auto material = ScalarSurfaceMaterial::bipolarShaded();
    int previousRippleAccent = 0;
    for (const int columns: { 512, 1024 }) {
        constexpr int rows = 32;
        std::array<int, 3> maximumAccent {};
        for (int fixture = 0; fixture < 3; ++fixture) {
            const auto values = extrudedSurface(columns, rows, [fixture](float x) {
                const float broad = 0.5f + 0.4f * std::sin(x * 12.5663706f);
                return fixture == 0 ? x : broad
                        + (fixture == 2 ? 0.035f * std::sin(x * 201.06193f) : 0.f);
            });
            const auto scales = ScalarSurfaceMaterialEvaluator::createHeightScales(
                    values.data(), (int) values.size(), columns, rows, material);
            for (int column = 0; column < columns; ++column) {
                const float value = values[(size_t) column * rows + rows / 2];
                const auto derivatives = ScalarSurfaceMaterialEvaluator::derivativesAt(
                        scales, column, rows / 2, material, 1.f);
                const auto base = ScalarSurfaceMaterialEvaluator::baseColourFor(value, material);
                const auto shaded = ScalarSurfaceMaterialEvaluator::colourFor(value, derivatives, material);
                maximumAccent[(size_t) fixture] = juce::jmax(
                        maximumAccent[(size_t) fixture], maximumChannelDifference(base, shaded));
            }
        }
        INFO("resolution " << columns << "; ramp/sine/ripple "
                << maximumAccent[0] << "/" << maximumAccent[1] << "/" << maximumAccent[2]);
        REQUIRE(maximumAccent[0] <= 1);
        REQUIRE(maximumAccent[1] <= 3);
        REQUIRE(maximumAccent[2] > maximumAccent[1] + 3);
        if (previousRippleAccent != 0) {
            REQUIRE(std::abs(maximumAccent[2] - previousRippleAccent) <= 8);
        }
        previousRippleAccent = maximumAccent[2];
    }
}

TEST_CASE("Bipolar shaded fixtures remain smooth across resolutions", "[ui][surface-material]") {
    const auto material = ScalarSurfaceMaterial::bipolarShaded();
    std::array<juce::Colour, 2> centres;
    for (int resolutionIndex = 0; resolutionIndex < 2; ++resolutionIndex) {
        const int side = resolutionIndex == 0 ? 128 : 256;
        const auto ramp = extrudedSurface(side, side, [](float x) { return 0.1f + 0.8f * x; });
        const auto scales = ScalarSurfaceMaterialEvaluator::createHeightScales(
                ramp.data(), (int) ramp.size(), side, side, material);
        const auto derivatives = ScalarSurfaceMaterialEvaluator::derivativesAt(
                scales, side / 2, side / 2, material, 1.f);
        centres[(size_t) resolutionIndex] = ScalarSurfaceMaterialEvaluator::colourFor(
                0.3f, derivatives, material);
        const auto boundary = ScalarSurfaceMaterialEvaluator::derivativesAt(
                scales, 0, side / 2, material, 1.f);
        REQUIRE(maximumChannelDifference(centres[(size_t) resolutionIndex],
                ScalarSurfaceMaterialEvaluator::colourFor(0.3f, boundary, material)) <= 1);
        const std::vector<float> constant((size_t) side * side, 0.3f);
        const auto image = ScalarSurfaceMaterialEvaluator::createImage(
                constant.data(), (int) constant.size(), side, side, material);
        REQUIRE(image.getPixelAt(0, 0) == image.getPixelAt(side / 2, side / 2));
        REQUIRE(image.getPixelAt(side - 1, side - 1) == image.getPixelAt(0, 0));
    }
    REQUIRE(maximumChannelDifference(centres[0], centres[1]) <= 1);

    // Optional reference sheet: columns are palette-only, shaded, previous bipolar detail.
    if (const char* path = std::getenv("CYCLE_SURFACE_REFERENCE_PNG")) {
        constexpr int side = 256;
        juce::Image sheet(juce::Image::RGB, side * 3, side * 3, true);
        juce::Graphics graphics(sheet);
        auto flat = material;
        flat.relief = ScalarSurfaceRelief::None;
        const std::array<ScalarSurfaceMaterial, 3> materials {
            flat, material, ScalarSurfaceMaterial::signedAmplitude()
        };
        for (int fixture = 0; fixture < 3; ++fixture) {
            const auto values = extrudedSurface(side, side, [fixture](float x) {
                const float broad = 0.5f + 0.4f * std::sin(x * 12.5663706f);
                return fixture == 0 ? x : broad
                        + (fixture == 2 ? 0.035f * std::sin(x * 201.06193f) : 0.f);
            });
            for (int column = 0; column < 3; ++column) {
                graphics.drawImageAt(ScalarSurfaceMaterialEvaluator::createImage(
                        values.data(), (int) values.size(), side, side,
                        materials[(size_t) column]), column * side, fixture * side);
            }
        }
        juce::FileOutputStream stream { juce::File(path) };
        REQUIRE(stream.openedOk());
        REQUIRE(juce::PNGImageFormat().writeImageToStream(sheet, stream));
    }
}

TEST_CASE("Shaded detail band suppresses near-grid ripple without losing mid detail", "[ui][surface-material]") {
    constexpr int columns = 512;
    constexpr int rows = 32;
    for (const float cycles: { 24.f, 192.f }) {
        const auto values = extrudedSurface(columns, rows, [cycles](float x) {
            return 0.5f + 0.05f * std::sin(x * cycles * 6.2831853f);
        });
        std::array<float, 2> energy {};
        const std::array<ScalarSurfaceMaterial, 2> materials {
            ScalarSurfaceMaterial::signedAmplitude(), ScalarSurfaceMaterial::bipolarShaded()
        };
        for (size_t index = 0; index < materials.size(); ++index) {
            const auto scales = ScalarSurfaceMaterialEvaluator::createHeightScales(
                    values.data(), (int) values.size(), columns, rows, materials[index]);
            for (int column = 16; column < columns - 16; ++column) {
                const auto derivatives = ScalarSurfaceMaterialEvaluator::derivativesAt(
                        scales, column, rows / 2, materials[index], 1.f);
                energy[index] += derivatives.detailSlopeX * derivatives.detailSlopeX;
            }
        }
        INFO("cycles " << cycles << "; highpass/bandpass " << energy[0] << "/" << energy[1]);
        if (cycles > 100.f) {
            REQUIRE(energy[1] < energy[0] * 0.1f);
        } else {
            REQUIRE(energy[1] > energy[0] * 0.5f);
        }
    }
}

TEST_CASE("Pure blue depth ignores derivative effects", "[ui][surface-material]") {
    const ScalarSurfaceMaterial material = ScalarSurfaceMaterial::blueDepth();
    const juce::Image legacyBlue = juce::PNGImageFormat::loadFrom(
            Gradients::blue_png,
            Gradients::blue_pngSize);
    const ScalarSurfaceDerivatives derivatives = exaggeratedDerivatives();

    REQUIRE(material.relief == ScalarSurfaceRelief::None);
    REQUIRE(material.palette == ScalarSurfacePalette::LegacyBlue);
    REQUIRE(material.embossStrength == 0.f);
    REQUIRE(material.edgeTintStrength == 0.f);
    REQUIRE(legacyBlue.isValid());
    for (const float value: { 0.f, 0.25f, 0.5f, 0.75f, 1.f }) {
        const int x = juce::roundToInt(value * (float) (legacyBlue.getWidth() - 1));
        REQUIRE(ScalarSurfaceMaterialEvaluator::baseColourFor(value, material)
                == legacyBlue.getPixelAt(x, 0));
        REQUIRE(ScalarSurfaceMaterialEvaluator::colourFor(value, derivatives, material)
                == ScalarSurfaceMaterialEvaluator::baseColourFor(value, material));
    }
}

TEST_CASE("Directional detail response is continuous and energy graded",
        "[ui][surface-material]") {
    ScalarSurfaceMaterial material = ScalarSurfaceMaterial::blueDepthDirectionalDetail();
    material.embossStrength = 0.f;
    material.edgeTintStrength = 0.5f;

    const auto colourAtEnergy = [&material](float energy) {
        ScalarSurfaceDerivatives detail;
        detail.detailSlopeX = 0.04f;
        detail.detailSlopeY = 0.04f;
        detail.detailEnergy = energy;
        return ScalarSurfaceMaterialEvaluator::colourFor(0.2f, detail, material);
    };

    const juce::Colour base = colourAtEnergy(0.f);
    const juce::Colour low = colourAtEnergy(0.0005f);
    const juce::Colour medium = colourAtEnergy(material.detailEnergyKnee);
    const juce::Colour high = colourAtEnergy(material.detailEnergyKnee * 4.f);
    REQUIRE(low.getRed() > base.getRed());
    REQUIRE(medium.getRed() > low.getRed());
    REQUIRE(high.getRed() > medium.getRed());
    REQUIRE(high.getGreen() > medium.getGreen());

    const juce::Colour belowOldThreshold = colourAtEnergy(0.00149f);
    const juce::Colour aboveOldThreshold = colourAtEnergy(0.00151f);
    REQUIRE(maximumChannelDifference(belowOldThreshold, aboveOldThreshold) <= 1);
}

TEST_CASE("Directional detail maps undirected angle to magenta cyan and yellow",
        "[ui][surface-material]") {
    ScalarSurfaceMaterial material = ScalarSurfaceMaterial::blueDepthDirectionalDetail();
    material.embossStrength = 0.f;
    material.edgeTintStrength = 1.f;

    const auto colourForSlope = [&material](float slopeX, float slopeY) {
        ScalarSurfaceDerivatives detail;
        detail.detailSlopeX = slopeX;
        detail.detailSlopeY = slopeY;
        detail.detailEnergy = material.detailEnergyKnee * 8.f;
        return ScalarSurfaceMaterialEvaluator::colourFor(0.5f, detail, material);
    };

    const juce::Colour magenta = colourForSlope(0.1f, 0.f);
    const juce::Colour cyan = colourForSlope(0.05f, 0.08660254f);
    const juce::Colour yellow = colourForSlope(-0.05f, 0.08660254f);
    REQUIRE(magenta.getRed() > magenta.getGreen());
    REQUIRE(magenta.getBlue() > magenta.getGreen());
    REQUIRE(cyan.getGreen() > cyan.getRed());
    REQUIRE(cyan.getBlue() > cyan.getRed());
    REQUIRE(yellow.getRed() > yellow.getBlue());
    REQUIRE(yellow.getGreen() > yellow.getBlue());

    const juce::Colour oppositeMagenta = colourForSlope(-0.1f, 0.f);
    const juce::Colour oppositeCyan = colourForSlope(-0.05f, -0.08660254f);
    const juce::Colour oppositeYellow = colourForSlope(0.05f, -0.08660254f);
    REQUIRE(maximumChannelDifference(magenta, oppositeMagenta) <= 1);
    REQUIRE(maximumChannelDifference(cyan, oppositeCyan) <= 1);
    REQUIRE(maximumChannelDifference(yellow, oppositeYellow) <= 1);
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

TEST_CASE("Scalar texture uploads track product and height preprocessing",
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

    data.material = ScalarSurfaceMaterial::signedAmplitudeFlat();
    REQUIRE(state.needsUpload(data));
    state.markUploaded(data);
    data.material = ScalarSurfaceMaterial::bipolarShaded();
    REQUIRE(state.needsUpload(data));
    state.markUploaded(data);
    REQUIRE_FALSE(state.needsUpload(data));
    data.material.shadedHighlightStrength += 0.1f;
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

TEST_CASE("Saved Icy-hot programs match the lab Stengah reference", "[scalar-surface][surface-program]") {
    const juce::File fixtures = juce::File(__FILE__).getParentDirectory().getParentDirectory()
            .getParentDirectory().getChildFile("scripts/fixtures/surface-colour-lab");
    juce::FileInputStream source(fixtures.getChildFile("stengah-b0-spy1.f32"));
    REQUIRE(source.openedOk());
    const int columns = source.readInt();
    const int rows = source.readInt();
    std::vector<float> values((size_t) columns * rows);
    for (float& value : values) {
        value = source.readFloat();
    }
    std::vector<float> magnitude = values;
    Buffer<float>(magnitude.data(), (int) magnitude.size()).abs();
    const float peak = Buffer<float>(magnitude.data(), (int) magnitude.size()).max();
    Buffer<float>(values.data(), (int) values.size()).mul(0.5f / peak).add(0.5f);
    for (const bool program14 : { false, true }) {
        const auto material = ScalarSurfaceMaterial::icyHotProgram(program14);
        const auto image = ScalarSurfaceMaterialEvaluator::createImage(
                values.data(), (int) values.size(), columns, rows, material);
        const juce::String name = program14 ? "program-14.png" : "program-13.png";
        const auto expected = juce::ImageFileFormat::loadFrom(fixtures.getChildFile(name));
        REQUIRE(expected.isValid());
        int maximumError = 0;
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < columns; ++x) {
                const auto actual = image.getPixelAt(x, y);
                const auto reference = expected.getPixelAt(x, y);
                maximumError = juce::jmax(maximumError,
                        std::abs((int) actual.getRed() - reference.getRed()),
                        std::abs((int) actual.getGreen() - reference.getGreen()),
                        std::abs((int) actual.getBlue() - reference.getBlue()));
            }
        }
        INFO(name << " maximum channel error " << maximumError);
        REQUIRE(maximumError <= 3);
    }
}

TEST_CASE("Icy-hot style changes invalidate cached scalar products", "[scalar-surface][surface-program]") {
    std::vector<float> values(64, 0.5f);
    ScalarSurfaceRenderData data;
    data.values = values.data();
    data.valueCount = 64;
    data.columns = 8;
    data.rows = 8;
    data.bounds = { 0.f, 0.f, 100.f, 100.f };
    data.hasStableRevision = true;
    data.material = ScalarSurfaceMaterial::icyHotProgram(false);
    ScalarSurfaceUploadState cache;
    cache.markUploaded(data);
    REQUIRE_FALSE(cache.needsUpload(data));
    data.material = ScalarSurfaceMaterial::icyHotProgram(true);
    REQUIRE(cache.needsUpload(data));
    for (auto style : { ScalarSurfaceTimeStyle::IcyHot13, ScalarSurfaceTimeStyle::IcyHot14 }) {
        REQUIRE(ScalarSurfaceMaterial::timeSurfaceStyleFromIndex(
                ScalarSurfaceMaterial::timeSurfaceStyleIndex(style)) == style);
    }
}

TEST_CASE("Icy-hot constant surfaces stay uniform and preserve authored level", "[surface-program]") {
    for (const bool program14 : { false, true }) {
        const auto material = ScalarSurfaceMaterial::icyHotProgram(program14);
        std::vector<float> values(64, 0.2f);
        const auto first = ScalarSurfaceMaterialEvaluator::createImage(values.data(), 64, 8, 8, material);
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                REQUIRE(first.getPixelAt(x, y) == first.getPixelAt(4, 4));
            }
        }
        REQUIRE(values.front() == 0.2f);
        std::fill(values.begin(), values.end(), 0.4f);
        const auto second = ScalarSurfaceMaterialEvaluator::createImage(values.data(), 64, 8, 8, material);
        REQUIRE(first.getPixelAt(4, 4) != second.getPixelAt(4, 4));
    }
}
