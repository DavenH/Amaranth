#include <array>

#include <Binary/Gradients.h>

#include "ScalarSurfaceMaterial.h"

namespace {

float smoothUnit(float value) {
    const float unit = juce::jlimit(0.f, 1.f, value);
    return unit * unit * (3.f - 2.f * unit);
}

juce::Colour signedAmplitudeColour(float value, const ScalarSurfaceMaterial& material) {
    const float unit = juce::jlimit(0.f, 1.f, value);
    const float palettePosition = unit
            * (float) (ScalarSurfaceMaterial::signedPaletteStopCount - 1);
    const int lowerIndex = juce::jmin(
            (int) palettePosition,
            ScalarSurfaceMaterial::signedPaletteStopCount - 2);
    return material.signedPaletteStops[(size_t) lowerIndex].interpolatedWith(
            material.signedPaletteStops[(size_t) lowerIndex + 1],
            palettePosition - (float) lowerIndex);
}

juce::Colour bipolarColour(float value, const ScalarSurfaceMaterial& material) {
    const float unit = juce::jlimit(0.f, 1.f, value);
    const float magnitude = unit < 0.5f
            ? 1.f - 2.f * unit
            : 2.f * unit - 1.f;
    const float amount = magnitude * (2.f - magnitude);
    return unit < 0.5f
            ? material.neutralAnchor.interpolatedWith(material.negativeAnchor, amount)
            : material.neutralAnchor.interpolatedWith(material.positiveAnchor, amount);
}

juce::Colour magnitudeColour(float value) {
    static const juce::Image legacyBurntAlum = juce::PNGImageFormat::loadFrom(
            Gradients::burntalum_png,
            Gradients::burntalum_pngSize);
    if (!legacyBurntAlum.isValid()) {
        return juce::Colour(0xff53657a);
    }
    const int x = juce::jlimit(
            0,
            legacyBurntAlum.getWidth() - 1,
            juce::roundToInt(juce::jlimit(0.f, 1.f, value)
                    * (float) (legacyBurntAlum.getWidth() - 1)));
    return legacyBurntAlum.getPixelAt(x, 0);
}

juce::Colour paletteColour(float value, const ScalarSurfaceMaterial& material) {
    const float unit = juce::jlimit(0.f, 1.f, value);

    if (material.palette == ScalarSurfacePalette::UnipolarMagnitude) {
        return magnitudeColour(unit);
    }

    if (material.palette == ScalarSurfacePalette::SignedAmplitude) {
        return signedAmplitudeColour(unit, material);
    }

    return bipolarColour(unit, material);
}

juce::Colour modulateBrightness(
        juce::Colour colour,
        float amount,
        float opacity,
        const ScalarSurfaceMaterial& material) {
    const float multiplier = juce::jlimit(
            material.minimumBrightness,
            material.maximumBrightness,
            1.f + amount);
    return juce::Colour::fromFloatRGBA(
            juce::jlimit(0.f, 1.f, colour.getFloatRed() * multiplier),
            juce::jlimit(0.f, 1.f, colour.getFloatGreen() * multiplier),
            juce::jlimit(0.f, 1.f, colour.getFloatBlue() * multiplier),
            juce::jlimit(0.f, 1.f, opacity));
}

}

ScalarSurfaceMaterial ScalarSurfaceMaterial::signedAmplitude() {
    ScalarSurfaceMaterial material;
    material.palette = ScalarSurfacePalette::SignedAmplitude;
    material.signedPaletteStops = {
        juce::Colour(0xff11153b),
        juce::Colour(0xff283f87),
        juce::Colour(0xff536fbd),
        juce::Colour(0xff99a7df),
        juce::Colour(0xff2a252f),
        juce::Colour(0xffb76252),
        juce::Colour(0xffdf7e58),
        juce::Colour(0xfff5a979),
        juce::Colour(0xffffd0a2)
    };
    material.negativeAnchor = material.signedPaletteStops.front();
    material.neutralAnchor = material.signedPaletteStops[4];
    material.positiveAnchor = material.signedPaletteStops.back();
    material.opacity = 1.f;
    material.reliefGain = 6.f;
    material.diffuseStrength = 0.24f;
    material.specularStrength = 0.24f;
    material.ridgeHighlightStrength = 0.14f;
    material.valleyShadowStrength = 0.26f;
    material.minimumBrightness = 0.56f;
    material.maximumBrightness = 1.46f;
    return material;
}

ScalarSurfaceMaterial ScalarSurfaceMaterial::unipolarMagnitude() {
    ScalarSurfaceMaterial material;
    material.palette = ScalarSurfacePalette::UnipolarMagnitude;
    material.negativeAnchor = juce::Colour(0xff15100a);
    material.neutralAnchor = juce::Colour(0xff15100a);
    material.positiveAnchor = juce::Colour(0xffffc052);
    material.opacity = 1.f;
    material.opacityValueScale = 25.f;
    material.opacityPower = 2;
    material.reliefGain = 3.5f;
    material.diffuseStrength = 0.14f;
    material.specularStrength = 0.10f;
    material.ridgeHighlightStrength = 0.05f;
    material.valleyShadowStrength = 0.12f;
    return material;
}

ScalarSurfaceMaterial ScalarSurfaceMaterial::bipolarPhase() {
    ScalarSurfaceMaterial material;
    material.palette = ScalarSurfacePalette::BipolarPhase;
    material.negativeAnchor = juce::Colour(0xffff7a3d);
    material.neutralAnchor = juce::Colour(0xff120d18);
    material.positiveAnchor = juce::Colour(0xffb887ff);
    material.opacity = 1.f;
    material.opacityValueScale = 5.f;
    material.reliefGain = 2.5f;
    material.ridgeHighlightStrength = 0.05f;
    material.valleyShadowStrength = 0.10f;
    return material;
}

ScalarSurfaceDerivatives ScalarSurfaceMaterialEvaluator::derivativesAt(
        const float* values,
        int columns,
        int rows,
        int column,
        int row) {
    const int leftColumn = juce::jmax(0, column - 1);
    const int rightColumn = juce::jmin(columns - 1, column + 1);
    const int lowerRow = juce::jmax(0, row - 1);
    const int upperRow = juce::jmin(rows - 1, row + 1);
    const auto sample = [values, columns, rows](int x, int y) {
        const int clampedX = juce::jlimit(0, columns - 1, x);
        const int clampedY = juce::jlimit(0, rows - 1, y);
        return values[clampedX * rows + clampedY];
    };
    const auto smoothSample = [&sample](int x, int y) {
        return 0.5f * sample(x, y)
                + 0.125f * (sample(x - 1, y)
                        + sample(x + 1, y)
                        + sample(x, y - 1)
                        + sample(x, y + 1));
    };

    const float centre = smoothSample(column, row);
    const float left = smoothSample(leftColumn, row);
    const float right = smoothSample(rightColumn, row);
    const float lower = smoothSample(column, lowerRow);
    const float upper = smoothSample(column, upperRow);

    ScalarSurfaceDerivatives result;
    result.slopeX = 0.5f * (right - left);
    result.slopeY = 0.5f * (upper - lower);
    result.curvature = left + right + lower + upper - 4.f * centre;
    return result;
}

juce::Colour ScalarSurfaceMaterialEvaluator::colourFor(
        float value,
        const ScalarSurfaceDerivatives& derivatives,
        const ScalarSurfaceMaterial& material) {
    const float unitValue = juce::jlimit(0.f, 1.f, value);
    const float directionalSlope = -material.reliefGain
            * (derivatives.slopeX * material.lightX
                    + derivatives.slopeY * material.lightY);
    const float diffuse = juce::jlimit(-1.f, 1.f, directionalSlope)
            * material.diffuseStrength;
    const float positiveSlope = juce::jmax(0.f, directionalSlope);
    const float specularAmount = positiveSlope * (2.f - juce::jmin(1.f, positiveSlope));
    const float specular = juce::jmin(1.f, specularAmount)
            * material.specularStrength;
    const float ridgeAmount = smoothUnit(
            (-derivatives.curvature - material.curvatureThreshold)
                    / material.curvatureSoftness);
    const float valleyAmount = smoothUnit(
            (derivatives.curvature - material.curvatureThreshold)
                    / material.curvatureSoftness);

    float opacity = material.opacity;
    if (material.opacityValueScale > 0.f) {
        const float opacityValue = material.opacityPower == 2
                ? unitValue * unitValue
                : unitValue;
        opacity = juce::jmin(opacity, material.opacityValueScale * opacityValue);
    }

    juce::Colour colour = paletteColour(unitValue, material);
    const float relief = diffuse
            + specular
            + ridgeAmount * material.ridgeHighlightStrength
            - valleyAmount * material.valleyShadowStrength;
    return modulateBrightness(colour, relief, opacity, material);
}

juce::Image ScalarSurfaceMaterialEvaluator::createImage(
        const float* values,
        int valueCount,
        int columns,
        int rows,
        const ScalarSurfaceMaterial& material,
        bool opaque) {
    if (columns < 2 || rows < 2 || values == nullptr || valueCount < columns * rows) {
        return {};
    }

    juce::Image image(opaque ? juce::Image::RGB : juce::Image::ARGB, columns, rows, true);
    juce::Image::BitmapData bitmap(image, juce::Image::BitmapData::writeOnly);

    for (int column = 0; column < columns; ++column) {
        for (int row = 0; row < rows; ++row) {
            const int index = column * rows + row;
            const ScalarSurfaceDerivatives derivatives = derivativesAt(
                    values, columns, rows, column, row);
            juce::Colour colour = colourFor(values[index], derivatives, material);
            if (opaque) {
                colour = colour.withAlpha(1.f);
            }
            bitmap.setPixelColour(column, rows - 1 - row, colour);
        }
    }

    return image;
}

juce::Image ScalarSurfaceMaterialEvaluator::createGradientImage(
        const ScalarSurfaceMaterial& material,
        int width) {
    if (width <= 0) {
        return {};
    }

    juce::Image image(juce::Image::ARGB, width, 1, true);
    const ScalarSurfaceDerivatives flat;
    for (int x = 0; x < width; ++x) {
        image.setPixelAt(
                x,
                0,
                colourFor((float) x / (float) width, flat, material));
    }
    return image;
}
