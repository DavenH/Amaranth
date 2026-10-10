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

std::array<float, 3> palette(float value, const SurfaceProgramData::Palette& colours) {
    const float position = juce::jlimit(0.f, 1.f, value) * 2048.f;
    const int lower = (int) position;
    const int upper = juce::jmin(2048, lower + 1);
    const float amount = position - (float) lower;
    std::array<float, 3> result;
    for (int channel = 0; channel < 3; ++channel) {
        const float start = colours[(size_t) lower][(size_t) channel];
        result[(size_t) channel] = start + amount
                * (colours[(size_t) upper][(size_t) channel] - start);
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

using ColourChannels = std::array<std::vector<float>, 3>;

ColourChannels mapPalette(const std::vector<float>& values, const SurfaceProgramData::Palette& colours) {
    ColourChannels channels;
    for (auto& channel : channels) {
        channel.resize(values.size());
    }
    for (size_t index = 0; index < values.size(); ++index) {
        const auto colour = palette(values[index], colours);
        for (size_t channel = 0; channel < 3; ++channel) {
            channels[channel][index] = colour[channel];
        }
    }
    return channels;
}

ColourChannels screenComposite(
        ColourChannels base,
        ColourChannels detail,
        Buffer<float> alpha) {
    for (size_t channel = 0; channel < 3; ++channel) {
        auto inverseBase = base[channel];
        Buffer<float>(inverseBase.data(), alpha.size()).subCRev(1.f);
        Buffer<float>(detail[channel].data(), alpha.size()).mul(alpha)
                .mul(Buffer<float>(inverseBase.data(), alpha.size()))
                .add(Buffer<float>(base[channel].data(), alpha.size()));
        encodeSrgb(detail[channel]);
    }
    return detail;
}

ScalarSurfaceHeightScales packColours(const ColourChannels& channels, int columns, int rows) {
    ScalarSurfaceHeightScales product;
    product.columns = columns;
    product.rows = rows;
    product.packedValues.resize((size_t) columns * rows * 4);
    for (size_t index = 0; index < channels[0].size(); ++index) {
        for (size_t channel = 0; channel < 3; ++channel) {
            product.packedValues[index * 4 + channel] = channels[channel][index];
        }
        product.packedValues[index * 4 + 3] = 1.f;
    }
    return product;
}

std::vector<float> detailAlpha(const std::vector<float>& centredDetail, float boost) {
    auto alpha = centredDetail;
    const int count = (int) alpha.size();
    Buffer<float> alphaBuffer(alpha.data(), count);
    alphaBuffer.abs().mul(2.f).clip(0.f, 1.f);
    auto denominator = alpha;
    Buffer<float>(denominator.data(), count).mul(boost - 1.f).add(1.f);
    alphaBuffer.mul(boost).div(Buffer<float>(denominator.data(), count));
    return alpha;
}

}

bool ScalarSurfaceProgram::isProgram(const ScalarSurfaceMaterial& material) {
    return material.relief >= ScalarSurfaceRelief::Program13
            && material.relief <= ScalarSurfaceRelief::Program20;
}

ScalarSurfaceHeightScales ScalarSurfaceProgram::render(
        const std::vector<float>& unitValues,
        int columns,
        int rows,
        const ScalarSurfaceMaterial& material) {
    const auto& program = SurfaceProgramData::programs[(size_t) material.relief
            - (size_t) ScalarSurfaceRelief::Program13];
    const int count = columns * rows;
    auto band = program.innerSigma > 0.f
            ? gaussianY(unitValues, columns, rows, program.innerSigma) : unitValues;
    auto outer = gaussianY(unitValues, columns, rows, program.outerSigma);
    Buffer<float> detail(band.data(), count);
    detail.sub(Buffer<float>(outer.data(), count)).mul(program.detailGain)
            .add(program.detailOffset);

    auto alpha = detailAlpha(band, program.alphaBoost);
    detail.add(0.5f).clip(0.f, 1.f);

    std::vector<float> base = unitValues;
    Buffer<float>(base.data(), count).add(-0.5f).mul(program.baseGain)
            .add(0.5f + program.baseOffset).clip(0.f, 1.f);
    auto channels = screenComposite(mapPalette(base, program.base), mapPalette(band, program.detail),
            Buffer<float>(alpha.data(), count));
    return packColours(channels, columns, rows);
}

juce::Colour ScalarSurfaceProgram::colourAt(const ScalarSurfaceHeightScales& product, int index) {
    const float* pixel = product.packedValues.data() + (size_t) index * 4;
    return juce::Colour::fromFloatRGBA(pixel[0], pixel[1], pixel[2], pixel[3]);
}
