#include "ScalarSurfaceProgram.h"

#include "Array/Buffer.h"
#include "SurfaceProgramPalette.h"

namespace {

// Reflected, half-sample symmetric padding matches scipy.ndimage mode="reflect".
int reflected(int index, int size) {
    const int period = 2 * size;
    index = (index % period + period) % period;
    return index < size ? index : period - 1 - index;
}

std::vector<float> gaussianY(const std::vector<float>& input, int columns, int rows, float sigma) {
    const int radius = (int) (4.f * sigma + 0.5f);
    const int taps = 2 * radius + 1;
    std::vector<float> weights((size_t) taps);
    for (int tap = 0; tap < taps; ++tap) {
        weights[(size_t) tap] = (float) (tap - radius);
    }
    Buffer<float> kernel(weights.data(), taps);
    kernel.sqr().mul(-0.5f / (sigma * sigma)).exp();
    kernel.mul(1.f / kernel.sum());
    std::vector<float> output(input.size());
    std::vector<float> padded((size_t) rows + 2 * radius);
    for (int column = 0; column < columns; ++column) {
        for (int index = 0; index < (int) padded.size(); ++index) {
            padded[(size_t) index] = input[(size_t) column * rows + reflected(index - radius, rows)];
        }
        Buffer<float> destination(output.data() + column * rows, rows);
        for (int tap = 0; tap < taps; ++tap) {
            destination.addProduct(Buffer<float>(padded.data() + tap, rows), weights[(size_t) tap]);
        }
    }
    return output;
}

std::array<float, 3> palette(float value) {
    const float position = juce::jlimit(0.f, 1.f, value) * 2048.f;
    const int lower = (int) position;
    const int upper = juce::jmin(2048, lower + 1);
    const float amount = position - (float) lower;
    std::array<float, 3> result;
    for (int channel = 0; channel < 3; ++channel) {
        const float start = SurfaceProgramData::icyHot[(size_t) lower][(size_t) channel];
        result[(size_t) channel] = start + amount
                * (SurfaceProgramData::icyHot[(size_t) upper][(size_t) channel] - start);
    }
    return result;
}

void encodeSrgb(std::vector<float>& values) {
    const std::vector<float> linear = values;
    Buffer<float>(values.data(), (int) values.size()).pow(1.f / 2.4f).mul(1.055f).add(-0.055f);
    for (size_t index = 0; index < values.size(); ++index) {
        if (linear[index] <= 0.0031308f) {
            values[index] = linear[index] * 12.92f;
        }
    }
}

}

bool ScalarSurfaceProgram::isProgram(const ScalarSurfaceMaterial& material) {
    return material.relief == ScalarSurfaceRelief::Program13
            || material.relief == ScalarSurfaceRelief::Program14;
}

ScalarSurfaceHeightScales ScalarSurfaceProgram::render(
        const std::vector<float>& unitValues,
        int columns,
        int rows,
        const ScalarSurfaceMaterial& material) {
    const bool program13 = material.relief == ScalarSurfaceRelief::Program13;
    const int count = columns * rows;
    auto band = gaussianY(unitValues, columns, rows, program13 ? 1.25f : 0.25f);
    auto outer = gaussianY(unitValues, columns, rows, 14.5f);
    Buffer<float> detail(band.data(), count);
    detail.sub(Buffer<float>(outer.data(), count)).mul(program13 ? 2.3f : 1.9f)
            .add(program13 ? -0.01f : 0.19f);

    std::vector<float> alpha = band;
    Buffer<float> alphaBuffer(alpha.data(), count);
    alphaBuffer.abs().mul(2.f).clip(0.f, 1.f);
    std::vector<float> denominator = alpha;
    Buffer<float>(denominator.data(), count).mul(8.1f).add(1.f);
    alphaBuffer.mul(9.1f).div(Buffer<float>(denominator.data(), count));
    detail.add(0.5f).clip(0.f, 1.f);

    std::vector<float> base = unitValues;
    Buffer<float>(base.data(), count).add(-0.5f).mul(program13 ? 0.7f : 1.3f)
            .add(program13 ? 0.79f : 0.5f).clip(0.f, 1.f).subCRev(1.f).pow(3.f);
    // White-to-black interpolation in OKLab is a linear L ramp, cubed in linear RGB.
    std::array<std::vector<float>, 3> channels;
    for (auto& channel : channels) {
        channel.resize((size_t) count);
    }
    for (int index = 0; index < count; ++index) {
        const auto colour = palette(band[(size_t) index]);
        for (int channel = 0; channel < 3; ++channel) {
            channels[(size_t) channel][(size_t) index] = colour[(size_t) channel];
        }
    }
    std::vector<float> inverseBase = base;
    Buffer<float>(inverseBase.data(), count).subCRev(1.f);
    for (auto& channel : channels) {
        Buffer<float>(channel.data(), count).mul(alphaBuffer)
                .mul(Buffer<float>(inverseBase.data(), count)).add(Buffer<float>(base.data(), count));
        encodeSrgb(channel);
    }
    ScalarSurfaceHeightScales product;
    product.columns = columns;
    product.rows = rows;
    product.packedValues.resize((size_t) count * 4);
    for (int index = 0; index < count; ++index) {
        for (int channel = 0; channel < 3; ++channel) {
            product.packedValues[(size_t) index * 4 + channel] = channels[(size_t) channel][(size_t) index];
        }
        product.packedValues[(size_t) index * 4 + 3] = 1.f;
    }
    return product;
}

juce::Colour ScalarSurfaceProgram::colourAt(const ScalarSurfaceHeightScales& product, int index) {
    const float* pixel = product.packedValues.data() + (size_t) index * 4;
    return juce::Colour::fromFloatRGBA(pixel[0], pixel[1], pixel[2], pixel[3]);
}
