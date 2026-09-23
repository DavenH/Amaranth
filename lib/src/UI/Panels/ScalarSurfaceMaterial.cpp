#include <array>
#include <cmath>

#include <Binary/Gradients.h>

#include "Array/Buffer.h"
#include "Array/VecOps.h"
#include "ScalarSurfaceMaterial.h"

namespace {

constexpr float minimumIllumination = 0.16f;
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
    const float scale = 1.f / (float) (2 * radius + 1);
    for (int column = 0; column < columns; ++column) {
        const int offset = column * rows;
        float sum = 0.f;
        for (int tap = -radius; tap <= radius; ++tap) {
            sum += source[(size_t) offset + (size_t) juce::jlimit(0, rows - 1, tap)];
        }
        for (int row = 0; row < rows; ++row) {
            destination[(size_t) offset + (size_t) row] = sum * scale;
            const int outgoing = juce::jlimit(0, rows - 1, row - radius);
            const int incoming = juce::jlimit(0, rows - 1, row + radius + 1);
            sum += source[(size_t) offset + (size_t) incoming]
                    - source[(size_t) offset + (size_t) outgoing];
        }
    }
}

void blurColumns(
        const std::vector<float>& source,
        std::vector<float>& destination,
        int columns,
        int rows,
        int radius) {
    const float scale = 1.f / (float) (2 * radius + 1);
    for (int row = 0; row < rows; ++row) {
        float sum = 0.f;
        for (int tap = -radius; tap <= radius; ++tap) {
            sum += source[(size_t) juce::jlimit(0, columns - 1, tap) * rows + row];
        }
        for (int column = 0; column < columns; ++column) {
            destination[(size_t) column * rows + row] = sum * scale;
            const int outgoing = juce::jlimit(0, columns - 1, column - radius);
            const int incoming = juce::jlimit(0, columns - 1, column + radius + 1);
            sum += source[(size_t) incoming * rows + row]
                    - source[(size_t) outgoing * rows + row];
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
    for (int scale = 0; scale < 4; ++scale) {
        result.slopeX[(size_t) scale] = (
                heightAt(scales, rightColumn, row, scale)
                - heightAt(scales, leftColumn, row, scale)) / xDistance;
        result.slopeY[(size_t) scale] = (
                heightAt(scales, column, upperRow, scale)
                - heightAt(scales, column, lowerRow, scale)) / yDistance;
    }

    const float original = heightAt(scales, column, row, 0);
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

juce::Colour evaluateColour(
        float value,
        const ScalarSurfaceDerivatives& derivatives,
    const ScalarSurfaceMaterial& material,
    const LightGeometry& light) {
    const float unitValue = juce::jlimit(0.f, 1.f, value);
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
    material.signedPaletteStops = {
        juce::Colour(0xff06142c),
        juce::Colour(0xff0b2855),
        juce::Colour(0xff174989),
        juce::Colour(0xff316db4),
        juce::Colour(0xff5f91d2),
        juce::Colour(0xff94b9e7),
        juce::Colour(0xffbfd6f2),
        juce::Colour(0xffe0ecfa),
        juce::Colour(0xfff7fbff)
    };
    material.signedPalettePositions = {
        0.f, 0.14f, 0.28f, 0.42f, 0.56f, 0.70f, 0.82f, 0.92f, 1.f
    };
    material.negativeAnchor = material.signedPaletteStops.front();
    material.neutralAnchor = material.signedPaletteStops[4];
    material.positiveAnchor = material.signedPaletteStops.back();
    material.negativePearlTint = juce::Colour(0xffd5e3ff);
    material.neutralPearlTint = juce::Colour(0xffffd8b8);
    material.positivePearlTint = juce::Colour(0xffffe4c9);
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
    constexpr std::array<int, 3> maximumRadii { 3, 12, 48 };
    for (int scale = 0; scale < 3; ++scale) {
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
