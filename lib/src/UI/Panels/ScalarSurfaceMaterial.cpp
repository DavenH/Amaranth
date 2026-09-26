#include <array>
#include <atomic>
#include <cmath>

#include <Binary/Gradients.h>

#include "Array/Buffer.h"
#include "Array/VecOps.h"
#include "ScalarSurfaceMaterial.h"

namespace {

constexpr float minimumIllumination = 0.16f;
constexpr int linearTransferTableSize = 4096;
std::atomic<ScalarSurfaceTimeStyle> selectedTimeSurfaceStyle {
        ScalarSurfaceTimeStyle::BlueDepthDirectionalDetail };

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
            const float amount = smoothUnit(
                    (unit - lower) / juce::jmax(0.000001f, upper - lower));
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

juce::Colour legacyBlueColour(float value) {
    static const juce::Image legacyBlue = juce::PNGImageFormat::loadFrom(
            Gradients::blue_png,
            Gradients::blue_pngSize);
    if (!legacyBlue.isValid()) {
        return juce::Colour(0xff53657a);
    }
    const int x = juce::jlimit(
            0,
            legacyBlue.getWidth() - 1,
            juce::roundToInt(juce::jlimit(0.f, 1.f, value)
                    * (float) (legacyBlue.getWidth() - 1)));
    return legacyBlue.getPixelAt(x, 0);
}

juce::Colour paletteColour(float value, const ScalarSurfaceMaterial& material) {
    if (material.palette == ScalarSurfacePalette::UnipolarMagnitude) {
        return magnitudeColour(value);
    }
    if (material.palette == ScalarSurfacePalette::SignedAmplitude) {
        return signedAmplitudeColour(value, material);
    }
    if (material.palette == ScalarSurfacePalette::LegacyBlue) {
        return legacyBlueColour(value);
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

LinearColour weightedDirectionalColour(
        const std::array<juce::Colour, 3>& colours,
        float magentaWeight,
        float cyanWeight,
        float yellowWeight) {
    const float totalWeight = juce::jmax(
            1.e-20f,
            magentaWeight + cyanWeight + yellowWeight);
    const LinearColour magenta = toLinear(colours[0]);
    const LinearColour cyan = toLinear(colours[1]);
    const LinearColour yellow = toLinear(colours[2]);
    return {
        (magenta.red * magentaWeight + cyan.red * cyanWeight
                + yellow.red * yellowWeight) / totalWeight,
        (magenta.green * magentaWeight + cyan.green * cyanWeight
                + yellow.green * yellowWeight) / totalWeight,
        (magenta.blue * magentaWeight + cyan.blue * cyanWeight
                + yellow.blue * yellowWeight) / totalWeight
    };
}

LinearColour directionalDetailColour(
        float slopeX,
        float slopeY,
        const ScalarSurfaceMaterial& material) {
    constexpr float sine60 = 0.8660254f;
    const float magentaProjection = slopeX;
    const float cyanProjection = 0.5f * slopeX + sine60 * slopeY;
    const float yellowProjection = -0.5f * slopeX + sine60 * slopeY;
    const float magentaSquared = magentaProjection * magentaProjection;
    const float cyanSquared = cyanProjection * cyanProjection;
    const float yellowSquared = yellowProjection * yellowProjection;
    const float magentaWeight = magentaSquared * magentaSquared;
    const float cyanWeight = cyanSquared * cyanSquared;
    const float yellowWeight = yellowSquared * yellowSquared;
    return weightedDirectionalColour(
            material.directionalDetailColours,
            magentaWeight,
            cyanWeight,
            yellowWeight);
}

LinearColour detailColour(
        float value,
        const ScalarSurfaceDerivatives& derivatives,
        const ScalarSurfaceMaterial& material) {
    if (material.detailColour == ScalarSurfaceDetailColour::DirectionalCmy) {
        return directionalDetailColour(
                derivatives.detailSlopeX,
                derivatives.detailSlopeY,
                material);
    }
    return toLinear(value < 0.5f ? material.negativeEdgeTint : material.positiveEdgeTint);
}

struct LightGeometry {
    float x {};
    float y {};
    float z {};
    float halfX {};
    float halfY {};
    float halfZ {};
};

LightGeometry lightGeometry(const ScalarSurfaceMaterial& material) {
    const float lightLength = std::sqrt(
            material.lightX * material.lightX
                    + material.lightY * material.lightY
                    + material.lightZ * material.lightZ);
    LightGeometry geometry;
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

struct SurfaceLighting {
    float diffuse {};
    float specular {};
};

SurfaceLighting lightingFor(
        float slopeX,
        float slopeY,
        const ScalarSurfaceMaterial& material,
        const LightGeometry& light) {
    const float normalX = -slopeX * material.reliefScale;
    const float normalY = -slopeY * material.reliefScale;
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

int boundedBlurRadius(float radius, int dimension, int minimum, int maximum) {
    const int scaled = juce::roundToInt(radius * (float) juce::jmax(1, dimension - 1));
    return juce::jlimit(1, juce::jmax(1, dimension - 1), juce::jlimit(minimum, maximum, scaled));
}

void blurRows(
        const std::vector<float>& source,
        std::vector<float>& destination,
        int columns,
        int rows,
        int radius) {
    for (int column = 0; column < columns; ++column) {
        const int offset = column * rows;
        float sum = 0.f;
        const int initialEnd = juce::jmin(rows - 1, radius);
        for (int tap = 0; tap <= initialEnd; ++tap) {
            sum += source[(size_t) offset + (size_t) tap];
        }
        int sampleCount = initialEnd + 1;
        for (int row = 0; row < rows; ++row) {
            destination[(size_t) offset + (size_t) row] = sum / (float) sampleCount;
            const int outgoing = row - radius;
            const int incoming = row + radius + 1;
            if (outgoing >= 0) {
                sum -= source[(size_t) offset + (size_t) outgoing];
                --sampleCount;
            }
            if (incoming < rows) {
                sum += source[(size_t) offset + (size_t) incoming];
                ++sampleCount;
            }
        }
    }
}

void blurColumns(
        const std::vector<float>& source,
        std::vector<float>& destination,
        int columns,
        int rows,
        int radius) {
    for (int row = 0; row < rows; ++row) {
        float sum = 0.f;
        const int initialEnd = juce::jmin(columns - 1, radius);
        for (int tap = 0; tap <= initialEnd; ++tap) {
            sum += source[(size_t) tap * rows + row];
        }
        int sampleCount = initialEnd + 1;
        for (int column = 0; column < columns; ++column) {
            destination[(size_t) column * rows + row] = sum / (float) sampleCount;
            const int outgoing = column - radius;
            const int incoming = column + radius + 1;
            if (outgoing >= 0) {
                sum -= source[(size_t) outgoing * rows + row];
                --sampleCount;
            }
            if (incoming < columns) {
                sum += source[(size_t) incoming * rows + row];
                ++sampleCount;
            }
        }
    }
}

void packScale(
        const std::vector<float>& source,
        std::vector<float>& packed,
        int channel) {
    for (size_t index = 0; index < source.size(); ++index) {
        packed[index * 4 + (size_t) channel] = source[index];
    }
}

float heightAt(
        const ScalarSurfaceHeightScales& scales,
        int column,
        int row,
        int channel) {
    const size_t index = ((size_t) column * scales.rows + (size_t) row) * 4;
    return scales.packedValues[index + (size_t) channel];
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
        const ScalarSurfaceHeightScales& scales,
        int column,
        int row,
        const ScalarSurfaceMaterial& material,
        float surfaceAspectRatio) {
    const int leftColumn = juce::jmax(0, column - 1);
    const int rightColumn = juce::jmin(scales.columns - 1, column + 1);
    const int lowerRow = juce::jmax(0, row - 1);
    const int upperRow = juce::jmin(scales.rows - 1, row + 1);
    const float aspect = surfaceAspectRatio > 0.f
            ? surfaceAspectRatio
            : (float) (scales.columns - 1) / (float) (scales.rows - 1);
    const float xDistance = (float) (rightColumn - leftColumn)
            / (float) (scales.columns - 1) * aspect;
    const float yDistance = (float) (upperRow - lowerRow)
            / (float) (scales.rows - 1);

    ScalarSurfaceDerivatives result;
    if (material.relief == ScalarSurfaceRelief::None) {
        return result;
    }
    const int derivativeScaleCount = material.relief == ScalarSurfaceRelief::MultiscaleTerrain
            ? 4
            : 2;
    for (int scale = 0; scale < derivativeScaleCount; ++scale) {
        result.slopeX[(size_t) scale] = (
                heightAt(scales, rightColumn, row, scale)
                - heightAt(scales, leftColumn, row, scale)) / xDistance;
        result.slopeY[(size_t) scale] = (
                heightAt(scales, column, upperRow, scale)
                - heightAt(scales, column, lowerRow, scale)) / yDistance;
    }

    const float original = heightAt(scales, column, row, 0);
    if (material.relief != ScalarSurfaceRelief::MultiscaleTerrain) {
        result.detailSlopeX = result.slopeX[0] - result.slopeX[1];
        result.detailSlopeY = result.slopeY[0] - result.slopeY[1];
        const float detail = original - heightAt(scales, column, row, 1);
        result.detailEnergy = detail < 0.f ? -detail : detail;
        const int columnRadius = boundedBlurRadius(
                material.blurRadii[0], scales.columns, 2, 4);
        const int rowRadius = boundedBlurRadius(material.blurRadii[0], scales.rows, 2, 4);
        const int columnDistance = juce::jmin(column, scales.columns - 1 - column);
        const int rowDistance = juce::jmin(row, scales.rows - 1 - row);
        const float columnFade = smoothUnit(
                (float) (columnDistance - columnRadius) * 0.5f);
        const float rowFade = smoothUnit((float) (rowDistance - rowRadius) * 0.5f);
        result.boundaryFade = juce::jmin(columnFade, rowFade);
        return result;
    }

    float obscurance = 0.f;
    for (int scale = 1; scale < 4; ++scale) {
        const float cavity = heightAt(scales, column, row, scale)
                - original - material.obscuranceBiases[(size_t) scale - 1];
        obscurance += material.hillshadeWeights[(size_t) scale]
                * juce::jmax(0.f, cavity);
    }
    result.obscurance = juce::jlimit(0.f, 1.f, material.obscuranceScale * obscurance);
    result.exposure = juce::jlimit(
            0.f,
            1.f,
            material.exposureScale * juce::jmax(
                    0.f,
                    original - heightAt(scales, column, row, 2) - material.exposureBias));
    return result;
}

float microEmboss(
        const ScalarSurfaceDerivatives& derivatives,
        const ScalarSurfaceMaterial& material,
        const LightGeometry& light) {
    const float normalX = -derivatives.detailSlopeX * material.detailReliefScale;
    const float normalY = -derivatives.detailSlopeY * material.detailReliefScale;
    const float normalLength = std::sqrt(normalX * normalX + normalY * normalY + 1.f);
    const float detailLight = (
            normalX * light.x + normalY * light.y + light.z) / normalLength - light.z;
    return juce::jlimit(
            -1.f,
            1.f,
            detailLight / juce::jmax(0.000001f, material.embossLimit))
            * derivatives.boundaryFade;
}

float saturatingDetailResponse(float value, float knee) {
    const float positiveValue = juce::jmax(0.f, value);
    return positiveValue / juce::jmax(0.000001f, positiveValue + knee);
}

float detailAccentAmount(
        float value,
        const ScalarSurfaceDerivatives& derivatives,
        const ScalarSurfaceMaterial& material) {
    const float gradientEnergy = derivatives.detailSlopeX * derivatives.detailSlopeX
            + derivatives.detailSlopeY * derivatives.detailSlopeY;
    const float gradientKnee = material.detailGradientKnee * material.detailGradientKnee;
    const float gradientResponse = saturatingDetailResponse(gradientEnergy, gradientKnee);
    const float detailResponse = saturatingDetailResponse(
            derivatives.detailEnergy,
            material.detailEnergyKnee);
    const float signedValue = 2.f * value - 1.f;
    const float magnitude = signedValue < 0.f ? -signedValue : signedValue;
    const bool directional = material.detailColour
            == ScalarSurfaceDetailColour::DirectionalCmy;
    const float semanticGate = directional
            ? 1.f
            : smoothUnit(
                    (magnitude - material.neutralAccentWidth)
                            / juce::jmax(0.000001f, 1.f - material.neutralAccentWidth));
    const float slopeWeight = directional
            ? gradientResponse
            : 0.35f + 0.65f * gradientResponse;
    return detailResponse * slopeWeight * semanticGate
            * material.edgeTintStrength * derivatives.boundaryFade;
}

juce::Colour evaluateMicroEmboss(
        float unitValue,
        const ScalarSurfaceDerivatives& derivatives,
        const ScalarSurfaceMaterial& material,
        const LightGeometry& light) {
    const juce::Colour base = paletteColour(unitValue, material);
    LinearColour colour = toLinear(base);
    const float illumination = 1.f
            + material.embossStrength * microEmboss(derivatives, material, light);
    colour.red *= illumination;
    colour.green *= illumination;
    colour.blue *= illumination;
    colour = interpolate(
            colour,
            detailColour(unitValue, derivatives, material),
            detailAccentAmount(unitValue, derivatives, material));
    return juce::Colour::fromFloatRGBA(
            linearToSrgb(colour.red),
            linearToSrgb(colour.green),
            linearToSrgb(colour.blue),
            juce::jlimit(0.f, 1.f, opacityFor(unitValue, material)));
}

juce::Colour evaluateDirectionalShaded(
        float value,
        const ScalarSurfaceDerivatives& derivatives,
        const ScalarSurfaceMaterial& material,
        const LightGeometry& light) {
    const float smoothing = (1.f - material.detailReliefScale) * derivatives.boundaryFade;
    const float slopeX = derivatives.slopeX[0] - smoothing * derivatives.detailSlopeX;
    const float slopeY = derivatives.slopeY[0] - smoothing * derivatives.detailSlopeY;
    const float projected = -material.shadedSlopeScale * (slopeX * light.x + slopeY * light.y);
    const float magnitude = projected < 0.f ? -projected : projected;
    const float response = projected / (1.f + magnitude);
    const float facing = juce::jmax(0.f, response);
    const float illumination = 1.f + material.shadedShadowStrength * juce::jmin(0.f, response);
    LinearColour colour = toLinear(paletteColour(value, material));
    colour.red *= illumination;
    colour.green *= illumination;
    colour.blue *= illumination;
    colour = interpolate(colour, pearlColour(value, material),
            material.shadedHighlightStrength * facing * facing);
    return juce::Colour::fromFloatRGBA(
            linearToSrgb(colour.red),
            linearToSrgb(colour.green),
            linearToSrgb(colour.blue),
            juce::jlimit(0.f, 1.f, opacityFor(value, material)));
}

juce::Colour evaluateColour(
        float value,
        const ScalarSurfaceDerivatives& derivatives,
        const ScalarSurfaceMaterial& material,
        const LightGeometry& light) {
    const float unitValue = juce::jlimit(0.f, 1.f, value);
    if (material.relief == ScalarSurfaceRelief::None) {
        return paletteColour(unitValue, material).withAlpha(opacityFor(unitValue, material));
    }
    if (material.relief == ScalarSurfaceRelief::DirectionalShaded) {
        return evaluateDirectionalShaded(unitValue, derivatives, material, light);
    }
    if (material.relief == ScalarSurfaceRelief::MicroEmboss) {
        return evaluateMicroEmboss(unitValue, derivatives, material, light);
    }
    float hillshade = 0.f;
    for (int scale = 0; scale < 4; ++scale) {
        hillshade += material.hillshadeWeights[(size_t) scale]
                * lightingFor(
                        derivatives.slopeX[(size_t) scale],
                        derivatives.slopeY[(size_t) scale],
                        material,
                        light).diffuse;
    }
    const SurfaceLighting specularLighting = lightingFor(
            derivatives.slopeX[2], derivatives.slopeY[2], material, light);
    const float illumination = juce::jmax(
            minimumIllumination,
            material.ambientStrength
                    + material.diffuseStrength * hillshade
                    - material.obscuranceStrength * derivatives.obscurance
                    + material.exposureStrength * derivatives.exposure);
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
            juce::jlimit(
                    0.f,
                    1.f,
                    material.specularStrength * specularLighting.specular
                            + material.exposureStrength * material.pearlTintStrength
                                    * derivatives.exposure));

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
    material.relief = ScalarSurfaceRelief::MicroEmboss;
    material.signedPaletteStops = {
        juce::Colour(0xff11163c),
        juce::Colour(0xff293f82),
        juce::Colour(0xff738bcd),
        juce::Colour(0xff596078),
        juce::Colour(0xff37323c),
        juce::Colour(0xff714d57),
        juce::Colour(0xffd37a5c),
        juce::Colour(0xfff0a071),
        juce::Colour(0xffffd0a0)
    };
    material.signedPalettePositions = {
        0.f, 0.18f, 0.32f, 0.42f, 0.5f, 0.58f, 0.68f, 0.82f, 1.f
    };
    material.negativeAnchor = material.signedPaletteStops.front();
    material.neutralAnchor = material.signedPaletteStops[4];
    material.positiveAnchor = material.signedPaletteStops.back();
    material.negativePearlTint = juce::Colour(0xffc6d1ff);
    material.neutralPearlTint = juce::Colour(0xff776f7c);
    material.positivePearlTint = juce::Colour(0xffffd9b8);
    material.negativeEdgeTint = juce::Colour(0xffc6d1ff);
    material.positiveEdgeTint = juce::Colour(0xffffcfad);
    material.directionalDetailColours = {
        juce::Colour(0xffc6d1ff),
        juce::Colour(0xffd5d5e8),
        juce::Colour(0xffffcfad)
    };
    return material;
}

ScalarSurfaceMaterial ScalarSurfaceMaterial::signedAmplitudeFlat() {
    ScalarSurfaceMaterial material = signedAmplitude();
    material.relief = ScalarSurfaceRelief::None;
    material.embossStrength = 0.f;
    material.edgeTintStrength = 0.f;
    return material;
}

ScalarSurfaceMaterial ScalarSurfaceMaterial::bipolarShaded() {
    ScalarSurfaceMaterial material = signedAmplitude();
    material.relief = ScalarSurfaceRelief::DirectionalShaded;
    material.signedPaletteStops = {
        juce::Colour(0xff647db6),
        juce::Colour(0xff435b94),
        juce::Colour(0xff354263),
        juce::Colour(0xff30323e),
        juce::Colour(0xff302c36),
        juce::Colour(0xff42343e),
        juce::Colour(0xff785053),
        juce::Colour(0xffb77460),
        juce::Colour(0xffe4a479)
    };
    material.negativeAnchor = material.signedPaletteStops.front();
    material.neutralAnchor = material.signedPaletteStops[4];
    material.positiveAnchor = material.signedPaletteStops.back();
    material.negativePearlTint = juce::Colour(0xffcfddff);
    material.neutralPearlTint = juce::Colour(0xffaaa4b0);
    material.positivePearlTint = juce::Colour(0xffffdfb1);
    material.detailReliefScale = 0.2f;
    return material;
}

ScalarSurfaceMaterial ScalarSurfaceMaterial::blueDepth() {
    ScalarSurfaceMaterial material;
    material.palette = ScalarSurfacePalette::LegacyBlue;
    material.relief = ScalarSurfaceRelief::None;
    material.signedPaletteStops = {
        juce::Colour(0xff050b18),
        juce::Colour(0xff0c1e38),
        juce::Colour(0xff19365b),
        juce::Colour(0xff36506c),
        juce::Colour(0xff6b7480),
        juce::Colour(0xff8197ad),
        juce::Colour(0xff9ebcd0),
        juce::Colour(0xffc5dfec),
        juce::Colour(0xfff2fbff)
    };
    material.signedPalettePositions = {
        0.f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.f
    };
    material.negativeAnchor = material.signedPaletteStops.front();
    material.neutralAnchor = material.signedPaletteStops[4];
    material.positiveAnchor = material.signedPaletteStops.back();
    material.negativePearlTint = juce::Colour(0xffb7cae0);
    material.neutralPearlTint = juce::Colour(0xff9aa4af);
    material.positivePearlTint = juce::Colour(0xfff3f6f8);
    material.negativeEdgeTint = juce::Colour(0xff8e2437);
    material.positiveEdgeTint = juce::Colour(0xffffb15d);
    material.directionalDetailColours = {
        juce::Colour(0xffff4fd8),
        juce::Colour(0xff54e5ff),
        juce::Colour(0xffffd84d)
    };
    material.embossStrength = 0.f;
    material.edgeTintStrength = 0.f;
    material.neutralAccentWidth = 0.f;
    return material;
}

ScalarSurfaceMaterial ScalarSurfaceMaterial::blueDepthDirectionalDetail() {
    ScalarSurfaceMaterial material = blueDepth();
    material.palette = ScalarSurfacePalette::SignedAmplitude;
    material.relief = ScalarSurfaceRelief::MicroEmboss;
    material.detailColour = ScalarSurfaceDetailColour::DirectionalCmy;
    material.embossStrength = 0.06f;
    material.edgeTintStrength = 0.10f;
    return material;
}

ScalarSurfaceMaterial ScalarSurfaceMaterial::timeDomain() {
    switch (timeSurfaceStyle()) {
        case ScalarSurfaceTimeStyle::Bipolar:
            return signedAmplitude();

        case ScalarSurfaceTimeStyle::BlueDepth:
            return blueDepth();

        case ScalarSurfaceTimeStyle::BlueDepthDirectionalDetail:
            return blueDepthDirectionalDetail();

        case ScalarSurfaceTimeStyle::BipolarFlat:
            return signedAmplitudeFlat();

        case ScalarSurfaceTimeStyle::BipolarShaded:
            return bipolarShaded();
    }
    return blueDepthDirectionalDetail();
}

ScalarSurfaceTimeStyle ScalarSurfaceMaterial::timeSurfaceStyle() {
    return selectedTimeSurfaceStyle.load(std::memory_order_relaxed);
}

ScalarSurfaceTimeStyle ScalarSurfaceMaterial::timeSurfaceStyleFromIndex(int index) {
    if (index == timeSurfaceStyleIndex(ScalarSurfaceTimeStyle::Bipolar)) {
        return ScalarSurfaceTimeStyle::Bipolar;
    }
    if (index == timeSurfaceStyleIndex(ScalarSurfaceTimeStyle::BlueDepth)) {
        return ScalarSurfaceTimeStyle::BlueDepth;
    }
    if (index == timeSurfaceStyleIndex(ScalarSurfaceTimeStyle::BipolarFlat)) {
        return ScalarSurfaceTimeStyle::BipolarFlat;
    }
    if (index == timeSurfaceStyleIndex(ScalarSurfaceTimeStyle::BipolarShaded)) {
        return ScalarSurfaceTimeStyle::BipolarShaded;
    }
    return ScalarSurfaceTimeStyle::BlueDepthDirectionalDetail;
}

int ScalarSurfaceMaterial::timeSurfaceStyleIndex(ScalarSurfaceTimeStyle style) {
    return (int) style;
}

void ScalarSurfaceMaterial::setTimeSurfaceStyle(ScalarSurfaceTimeStyle style) {
    selectedTimeSurfaceStyle.store(style, std::memory_order_relaxed);
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
    material.negativeEdgeTint = material.negativePearlTint;
    material.positiveEdgeTint = material.positivePearlTint;
    material.directionalDetailColours.fill(material.positivePearlTint);
    material.opacityValueScale = 25.f;
    material.opacityPower = 2;
    material.reliefScale = 0.65f;
    material.diffuseStrength = 0.24f;
    material.specularStrength = 0.035f;
    material.obscuranceStrength = 0.10f;
    material.exposureStrength = 0.035f;
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
    material.negativeEdgeTint = material.negativePearlTint;
    material.positiveEdgeTint = material.positivePearlTint;
    material.directionalDetailColours.fill(material.positivePearlTint);
    material.opacityValueScale = 5.f;
    material.reliefScale = 0.75f;
    material.diffuseStrength = 0.28f;
    material.specularStrength = 0.04f;
    material.obscuranceStrength = 0.12f;
    material.exposureStrength = 0.04f;
    return material;
}

ScalarSurfaceHeightScales ScalarSurfaceMaterialEvaluator::createHeightScales(
        const float* values,
        int valueCount,
        int columns,
        int rows,
        const ScalarSurfaceMaterial& material,
        float valueScale,
        float valueOffset) {
    ScalarSurfaceHeightScales result;
    if (columns < 2 || rows < 2 || values == nullptr || valueCount < columns * rows) {
        return result;
    }

    const int valueTotal = columns * rows;
    std::vector<float> original((size_t) valueTotal);
    VecOps::copy(values, original.data(), valueTotal);
    Buffer<float>(original.data(), valueTotal).mul(valueScale).add(valueOffset).clip(0.f, 1.f);

    result.columns = columns;
    result.rows = rows;
    result.packedValues.resize((size_t) valueTotal * 4);
    packScale(original, result.packedValues, 0);

    std::vector<float> horizontal((size_t) valueTotal);
    std::vector<float> blurred((size_t) valueTotal);
    constexpr std::array<int, 3> minimumRadii { 2, 4, 8 };
    constexpr std::array<int, 3> maximumRadii { 4, 12, 48 };
    const int blurScaleCount = material.relief == ScalarSurfaceRelief::MultiscaleTerrain
            ? 3
            : material.relief == ScalarSurfaceRelief::None ? 0 : 1;
    for (int scale = 0; scale < blurScaleCount; ++scale) {
        const int columnRadius = boundedBlurRadius(
                material.blurRadii[(size_t) scale],
                columns,
                minimumRadii[(size_t) scale],
                maximumRadii[(size_t) scale]);
        const int rowRadius = boundedBlurRadius(
                material.blurRadii[(size_t) scale],
                rows,
                minimumRadii[(size_t) scale],
                maximumRadii[(size_t) scale]);
        blurColumns(original, horizontal, columns, rows, columnRadius);
        blurRows(horizontal, blurred, columns, rows, rowRadius);
        packScale(blurred, result.packedValues, scale + 1);
    }
    if (blurScaleCount == 0) {
        packScale(original, result.packedValues, 1);
        packScale(original, result.packedValues, 2);
        packScale(original, result.packedValues, 3);
    } else if (blurScaleCount == 1) {
        packScale(blurred, result.packedValues, 2);
        packScale(blurred, result.packedValues, 3);
    }
    return result;
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
    const ScalarSurfaceHeightScales scales = createHeightScales(
            values, columns * rows, columns, rows, material);
    return derivativesAt(scales, column, row, material, surfaceAspectRatio);
}

ScalarSurfaceDerivatives ScalarSurfaceMaterialEvaluator::derivativesAt(
        const ScalarSurfaceHeightScales& scales,
        int column,
        int row,
        const ScalarSurfaceMaterial& material,
        float surfaceAspectRatio) {
    if (!scales.isValid()) {
        return {};
    }
    return derivativesFor(
            scales,
            column,
            row,
            material,
            surfaceAspectRatio);
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
    const ScalarSurfaceHeightScales scales = createHeightScales(
            values, valueCount, columns, rows, material);
    juce::Image image(opaque ? juce::Image::RGB : juce::Image::ARGB, columns, rows, true);
    juce::Image::BitmapData bitmap(image, juce::Image::BitmapData::writeOnly);
    const LightGeometry light = lightGeometry(material);
    for (int column = 0; column < columns; ++column) {
        for (int row = 0; row < rows; ++row) {
            const int index = column * rows + row;
            const ScalarSurfaceDerivatives derivatives = derivativesFor(
                    scales, column, row, material, aspect);
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
    const ScalarSurfaceDerivatives flat {};
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
