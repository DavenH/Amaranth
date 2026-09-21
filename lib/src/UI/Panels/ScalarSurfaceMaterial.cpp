#include <array>
#include <cmath>

#include <Binary/Gradients.h>

#include "ScalarSurfaceMaterial.h"

namespace {

constexpr float minimumIllumination = 0.18f;
constexpr float neutralMinimumIllumination = 0.46f;
constexpr int linearTransferTableSize = 4096;

float smoothUnit(float value) {
    const float unit = juce::jlimit(0.f, 1.f, value);
    return unit * unit * (3.f - 2.f * unit);
}

const std::array<float, 256>& srgbTransferTable() {
    static const std::array<float, 256> table = [] {
        std::array<float, 256> result;
        for (int index = 0; index < (int) result.size(); ++index) {
            const float value = (float) index / (float) (result.size() - 1);
            result[(size_t) index] = value <= 0.04045f
                    ? value / 12.92f
                    : std::pow((value + 0.055f) / 1.055f, 2.4f);
        }
        return result;
    }();
    return table;
}

const std::array<float, linearTransferTableSize>& linearTransferTable() {
    static const std::array<float, linearTransferTableSize> table = [] {
        std::array<float, linearTransferTableSize> result;
        for (int index = 0; index < (int) result.size(); ++index) {
            const float value = (float) index / (float) (result.size() - 1);
            result[(size_t) index] = value <= 0.0031308f
                    ? 12.92f * value
                    : 1.055f * std::pow(value, 1.f / 2.4f) - 0.055f;
        }
        return result;
    }();
    return table;
}

float srgbToLinear(float value) {
    return srgbTransferTable()[(size_t) juce::roundToInt(
            juce::jlimit(0.f, 1.f, value) * 255.f)];
}

float linearToSrgb(float value) {
    return linearTransferTable()[(size_t) juce::roundToInt(
            juce::jlimit(0.f, 1.f, value) * (float) (linearTransferTableSize - 1))];
}

struct LinearColour {
    float red {};
    float green {};
    float blue {};
};

LinearColour toLinear(juce::Colour colour) {
    return { srgbToLinear(colour.getFloatRed()),
             srgbToLinear(colour.getFloatGreen()),
             srgbToLinear(colour.getFloatBlue()) };
}

LinearColour interpolate(LinearColour from, LinearColour to, float amount) {
    const float unit = juce::jlimit(0.f, 1.f, amount);
    return { from.red + unit * (to.red - from.red),
             from.green + unit * (to.green - from.green),
             from.blue + unit * (to.blue - from.blue) };
}

juce::Colour signedAmplitudeColour(float value, const ScalarSurfaceMaterial& material) {
    const float unit = juce::jlimit(0.f, 1.f, value);
    for (int index = 0; index < ScalarSurfaceMaterial::signedPaletteStopCount - 1; ++index) {
        const float lower = material.signedPalettePositions[(size_t) index];
        const float upper = material.signedPalettePositions[(size_t) index + 1];
        if (unit <= upper) {
            const float amount = (unit - lower) / juce::jmax(0.000001f, upper - lower);
            return material.signedPaletteStops[(size_t) index].interpolatedWith(
                    material.signedPaletteStops[(size_t) index + 1],
                    juce::jlimit(0.f, 1.f, amount));
        }
    }
    return material.signedPaletteStops.back();
}

juce::Colour bipolarColour(float value, const ScalarSurfaceMaterial& material) {
    const float unit = juce::jlimit(0.f, 1.f, value);
    const float magnitude = unit < 0.5f ? 1.f - 2.f * unit : 2.f * unit - 1.f;
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
    if (material.palette == ScalarSurfacePalette::UnipolarMagnitude) {
        return magnitudeColour(value);
    }
    if (material.palette == ScalarSurfacePalette::SignedAmplitude) {
        return signedAmplitudeColour(value, material);
    }
    return bipolarColour(value, material);
}

LinearColour pearlColour(float value, const ScalarSurfaceMaterial& material) {
    const float unit = juce::jlimit(0.f, 1.f, value);
    const float distance = unit < 0.5f ? 0.5f - unit : unit - 0.5f;
    const float magnitude = smoothUnit(distance * 2.f);
    return interpolate(
            toLinear(material.neutralPearlTint),
            toLinear(unit < 0.5f ? material.negativePearlTint : material.positivePearlTint),
            magnitude);
}

int sampleRadius(float radius, int dimension) {
    return juce::jmax(1, juce::roundToInt(radius * (float) juce::jmax(1, dimension - 1)));
}

struct SamplingGeometry {
    int smallX {};
    int smallY {};
    int largeX {};
    int largeY {};
    float xStep {};
    float yStep {};
    float aspect {};
};

struct LightGeometry {
    float horizontalX {};
    float horizontalY {};
    float x {};
    float y {};
    float z {};
    float halfX {};
    float halfY {};
    float halfZ {};
};

LightGeometry lightGeometry(const ScalarSurfaceMaterial& material) {
    const float horizontalLength = std::sqrt(
            material.lightX * material.lightX + material.lightY * material.lightY);
    const float lightLength = std::sqrt(
            material.lightX * material.lightX
                    + material.lightY * material.lightY
                    + material.lightZ * material.lightZ);
    LightGeometry geometry;
    geometry.horizontalX = horizontalLength > 0.f ? material.lightX / horizontalLength : 0.f;
    geometry.horizontalY = horizontalLength > 0.f ? material.lightY / horizontalLength : 0.f;
    const float safeLightLength = juce::jmax(0.000001f, lightLength);
    geometry.x = material.lightX / safeLightLength;
    geometry.y = material.lightY / safeLightLength;
    geometry.z = material.lightZ / safeLightLength;
    const float halfLength = std::sqrt(
            geometry.x * geometry.x
                    + geometry.y * geometry.y
                    + (geometry.z + 1.f) * (geometry.z + 1.f));
    const float safeHalfLength = juce::jmax(0.000001f, halfLength);
    geometry.halfX = geometry.x / safeHalfLength;
    geometry.halfY = geometry.y / safeHalfLength;
    geometry.halfZ = (geometry.z + 1.f) / safeHalfLength;
    return geometry;
}

SamplingGeometry samplingGeometry(
        int columns,
        int rows,
        const ScalarSurfaceMaterial& material,
        float surfaceAspectRatio) {
    SamplingGeometry geometry;
    geometry.smallX = sampleRadius(material.normalSampleRadius, columns);
    geometry.smallY = sampleRadius(material.normalSampleRadius, rows);
    geometry.largeX = sampleRadius(material.largeSampleRadius, columns);
    geometry.largeY = sampleRadius(material.largeSampleRadius, rows);
    geometry.xStep = (float) geometry.smallX / (float) juce::jmax(1, columns - 1);
    geometry.yStep = (float) geometry.smallY / (float) juce::jmax(1, rows - 1);
    geometry.aspect = surfaceAspectRatio > 0.f
            ? surfaceAspectRatio
            : (float) juce::jmax(1, columns - 1) / (float) juce::jmax(1, rows - 1);
    return geometry;
}

template<typename Sample>
float horizonShadowAt(
        const Sample& sample,
        int columns,
        int rows,
        int column,
        int row,
        float centre,
        const ScalarSurfaceMaterial& material,
        const LightGeometry& light) {
    const std::array<float, 3> radii {
        material.normalSampleRadius * 2.f,
        material.largeSampleRadius,
        material.largeSampleRadius * 2.f
    };
    float occlusion = 0.f;
    for (int index = 0; index < (int) radii.size(); ++index) {
        const int offsetX = juce::roundToInt(
                light.horizontalX * radii[(size_t) index]
                        * (float) juce::jmax(1, columns - 1));
        const int offsetY = juce::roundToInt(
                light.horizontalY * radii[(size_t) index]
                        * (float) juce::jmax(1, rows - 1));
        const float bias = material.shadowStart * (float) (index + 1);
        occlusion = juce::jmax(
                occlusion,
                sample(column - offsetX, row - offsetY) - centre - bias);
    }
    return smoothUnit(occlusion / material.shadowSoftness);
}

struct SurfaceLighting {
    float diffuse {};
    float specular {};
};

SurfaceLighting lightingFor(
        const ScalarSurfaceDerivatives& derivatives,
        const ScalarSurfaceMaterial& material,
        const LightGeometry& light) {
    const float normalX = -derivatives.slopeX * material.reliefScale;
    const float normalY = -derivatives.slopeY * material.reliefScale;
    const float normalLength = std::sqrt(normalX * normalX + normalY * normalY + 1.f);
    const float nx = normalX / normalLength;
    const float ny = normalY / normalLength;
    const float nz = 1.f / normalLength;
    const float halfDot = juce::jmax(
            0.f,
            nx * light.halfX + ny * light.halfY + nz * light.halfZ);

    SurfaceLighting lighting;
    lighting.diffuse = juce::jmax(
            0.f,
            nx * light.x + ny * light.y + nz * light.z);
    lighting.specular = halfDot;
    for (int power = 1; power < material.specularPower; ++power) {
        lighting.specular *= halfDot;
    }
    return lighting;
}

float opacityFor(float unitValue, const ScalarSurfaceMaterial& material) {
    if (material.opacityValueScale <= 0.f) {
        return material.opacity;
    }
    const float opacityValue = material.opacityPower == 2
            ? unitValue * unitValue
            : unitValue;
    return juce::jmin(material.opacity, material.opacityValueScale * opacityValue);
}

ScalarSurfaceDerivatives derivativesFor(
        const float* values,
        int columns,
        int rows,
        int column,
        int row,
        const ScalarSurfaceMaterial& material,
        float surfaceAspectRatio,
        const LightGeometry& light) {
    const auto sample = [values, columns, rows](int x, int y) {
        return values[juce::jlimit(0, columns - 1, x) * rows
                + juce::jlimit(0, rows - 1, y)];
    };
    const SamplingGeometry sampling = samplingGeometry(
            columns, rows, material, surfaceAspectRatio);
    const float centre = sample(column, row);
    const float left = sample(column - sampling.smallX, row);
    const float right = sample(column + sampling.smallX, row);
    const float lower = sample(column, row - sampling.smallY);
    const float upper = sample(column, row + sampling.smallY);

    ScalarSurfaceDerivatives result;
    result.slopeX = (right - left) / (2.f * sampling.xStep * sampling.aspect);
    result.slopeY = (upper - lower) / (2.f * sampling.yStep);
    result.curvature = left + right + lower + upper - 4.f * centre;
    result.largeCurvature = sample(column - sampling.largeX, row)
            + sample(column + sampling.largeX, row)
            + sample(column, row - sampling.largeY)
            + sample(column, row + sampling.largeY)
            - 4.f * centre;
    result.horizonShadow = horizonShadowAt(
            sample, columns, rows, column, row, centre, material, light);
    return result;
}

juce::Colour evaluateColour(
        float value,
        const ScalarSurfaceDerivatives& derivatives,
        const ScalarSurfaceMaterial& material,
        const LightGeometry& light) {
    const float unitValue = juce::jlimit(0.f, 1.f, value);
    const SurfaceLighting lighting = lightingFor(derivatives, material, light);
    const float detailCurvature = 0.65f * derivatives.curvature
            + 0.35f * derivatives.largeCurvature;
    const float cavity = smoothUnit(
            (detailCurvature - material.curvatureThreshold)
                    / material.curvatureSoftness);
    const float distanceFromNeutral = unitValue < 0.5f
            ? 0.5f - unitValue
            : unitValue - 0.5f;
    const float semanticMagnitude = smoothUnit(distanceFromNeutral * 2.f);
    const float illuminationFloor = neutralMinimumIllumination
            + semanticMagnitude * (minimumIllumination - neutralMinimumIllumination);
    const float illumination = juce::jmax(
            illuminationFloor,
            material.ambientStrength
                    + material.diffuseStrength * lighting.diffuse
                    - material.shadowStrength * derivatives.horizonShadow
                    - material.cavityStrength * cavity);
    const juce::Colour base = paletteColour(unitValue, material);
    LinearColour shaded = toLinear(base);
    shaded.red *= illumination;
    shaded.green *= illumination;
    shaded.blue *= illumination;
    const LinearColour highlight = interpolate(
            toLinear(base),
            pearlColour(unitValue, material),
            material.pearlTintStrength);
    shaded = interpolate(
            shaded,
            highlight,
            juce::jlimit(0.f, 1.f, material.specularStrength * lighting.specular));

    return juce::Colour::fromFloatRGBA(
            linearToSrgb(shaded.red),
            linearToSrgb(shaded.green),
            linearToSrgb(shaded.blue),
            juce::jlimit(0.f, 1.f, opacityFor(unitValue, material)));
}

}

ScalarSurfaceMaterial ScalarSurfaceMaterial::signedAmplitude() {
    ScalarSurfaceMaterial material;
    material.palette = ScalarSurfacePalette::SignedAmplitude;
    material.signedPaletteStops = {
        juce::Colour(0xff11153b),
        juce::Colour(0xff283f87),
        juce::Colour(0xff99a7df),
        juce::Colour(0xff626a88),
        juce::Colour(0xff342e39),
        juce::Colour(0xff825861),
        juce::Colour(0xffdf7e58),
        juce::Colour(0xfff5a979),
        juce::Colour(0xffffd0a2)
    };
    material.signedPalettePositions = {
        0.f, 0.18f, 0.34f, 0.465f, 0.5f, 0.535f, 0.66f, 0.82f, 1.f
    };
    material.negativeAnchor = material.signedPaletteStops.front();
    material.neutralAnchor = material.signedPaletteStops[4];
    material.positiveAnchor = material.signedPaletteStops.back();
    material.negativePearlTint = juce::Colour(0xffc3ccff);
    material.neutralPearlTint = juce::Colour(0xff766f7c);
    material.positivePearlTint = juce::Colour(0xffffddba);
    return material;
}

ScalarSurfaceMaterial ScalarSurfaceMaterial::unipolarMagnitude() {
    ScalarSurfaceMaterial material;
    material.palette = ScalarSurfacePalette::UnipolarMagnitude;
    material.negativeAnchor = juce::Colour(0xff15100a);
    material.neutralAnchor = juce::Colour(0xff15100a);
    material.positiveAnchor = juce::Colour(0xffffc052);
    material.negativePearlTint = juce::Colour(0xff8b745d);
    material.neutralPearlTint = juce::Colour(0xff776f68);
    material.positivePearlTint = juce::Colour(0xffffd6a0);
    material.opacityValueScale = 25.f;
    material.opacityPower = 2;
    material.reliefScale = 0.9f;
    material.diffuseStrength = 0.24f;
    material.specularStrength = 0.08f;
    material.shadowStrength = 0.12f;
    material.cavityStrength = 0.06f;
    return material;
}

ScalarSurfaceMaterial ScalarSurfaceMaterial::bipolarPhase() {
    ScalarSurfaceMaterial material;
    material.palette = ScalarSurfacePalette::BipolarPhase;
    material.negativeAnchor = juce::Colour(0xffff7a3d);
    material.neutralAnchor = juce::Colour(0xff120d18);
    material.positiveAnchor = juce::Colour(0xffb887ff);
    material.negativePearlTint = juce::Colour(0xffffc09e);
    material.neutralPearlTint = juce::Colour(0xff756b7c);
    material.positivePearlTint = juce::Colour(0xffdfc4ff);
    material.opacityValueScale = 5.f;
    material.reliefScale = 1.f;
    material.diffuseStrength = 0.28f;
    material.specularStrength = 0.10f;
    material.shadowStrength = 0.14f;
    material.cavityStrength = 0.06f;
    return material;
}

ScalarSurfaceDerivatives ScalarSurfaceMaterialEvaluator::derivativesAt(
        const float* values, int columns, int rows, int column, int row) {
    return derivativesAt(
            values,
            columns,
            rows,
            column,
            row,
            ScalarSurfaceMaterial::signedAmplitude(),
            (float) juce::jmax(1, columns - 1) / (float) juce::jmax(1, rows - 1));
}

ScalarSurfaceDerivatives ScalarSurfaceMaterialEvaluator::derivativesAt(
        const float* values,
        int columns,
        int rows,
        int column,
        int row,
        const ScalarSurfaceMaterial& material,
        float surfaceAspectRatio) {
    return derivativesFor(
            values,
            columns,
            rows,
            column,
            row,
            material,
            surfaceAspectRatio,
            lightGeometry(material));
}

juce::Colour ScalarSurfaceMaterialEvaluator::baseColourFor(
        float value, const ScalarSurfaceMaterial& material) {
    const float unitValue = juce::jlimit(0.f, 1.f, value);
    return paletteColour(unitValue, material).withAlpha(opacityFor(unitValue, material));
}

juce::Colour ScalarSurfaceMaterialEvaluator::colourFor(
        float value,
        const ScalarSurfaceDerivatives& derivatives,
        const ScalarSurfaceMaterial& material) {
    return evaluateColour(value, derivatives, material, lightGeometry(material));
}

juce::Image ScalarSurfaceMaterialEvaluator::createImage(
        const float* values,
        int valueCount,
        int columns,
        int rows,
        const ScalarSurfaceMaterial& material,
        bool opaque,
        float surfaceAspectRatio) {
    if (columns < 2 || rows < 2 || values == nullptr || valueCount < columns * rows) {
        return {};
    }
    const float aspect = surfaceAspectRatio > 0.f
            ? surfaceAspectRatio
            : (float) (columns - 1) / (float) (rows - 1);
    juce::Image image(opaque ? juce::Image::RGB : juce::Image::ARGB, columns, rows, true);
    juce::Image::BitmapData bitmap(image, juce::Image::BitmapData::writeOnly);
    const LightGeometry light = lightGeometry(material);
    for (int column = 0; column < columns; ++column) {
        for (int row = 0; row < rows; ++row) {
            const int index = column * rows + row;
            const ScalarSurfaceDerivatives derivatives = derivativesFor(
                    values, columns, rows, column, row, material, aspect, light);
            juce::Colour colour = evaluateColour(values[index], derivatives, material, light);
            if (opaque) {
                colour = colour.withAlpha(1.f);
            }
            bitmap.setPixelColour(column, rows - 1 - row, colour);
        }
    }
    return image;
}

juce::Image ScalarSurfaceMaterialEvaluator::createGradientImage(
        const ScalarSurfaceMaterial& material, int width) {
    if (width <= 0) {
        return {};
    }
    juce::Image image(juce::Image::ARGB, width, 1, true);
    const ScalarSurfaceDerivatives flat;
    for (int x = 0; x < width; ++x) {
        image.setPixelAt(
                x,
                0,
                colourFor(
                        (float) x / (float) juce::jmax(1, width - 1),
                        flat,
                        material));
    }
    return image;
}
