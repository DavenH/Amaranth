#include <algorithm>

#include <Array/Buffer.h>

#include "UI/SpyPlaybackGlint.h"

namespace CycleV2 {

namespace {

struct Channels {
    explicit Channels(int height) : red(height), green(height), blue(height) {}
    std::vector<float> red;
    std::vector<float> green;
    std::vector<float> blue;

    void read(const juce::Image::BitmapData& pixels, int x, int height) {
        for (int y = 0; y < height; ++y) {
            const auto colour = pixels.getPixelColour(x, y);
            red[(size_t) y] = colour.getRed();
            green[(size_t) y] = colour.getGreen();
            blue[(size_t) y] = colour.getBlue();
        }
    }
};

void illuminate(Buffer<float> channel, Buffer<float> amount, float tint, Buffer<float> scratch) {
    channel.copyTo(scratch);
    channel.subCRev(tint).mul(amount).add(scratch).clip(0.f, 255.f);
}

std::vector<float> columnWeights(int width, const std::vector<float>& positions) {
    std::vector<float> weights((size_t) width);
    const float radius = width * 0.042f;
    for (const float position : positions) {
        const float centre = position * width;
        const int first = juce::jmax(0, (int) (centre - radius));
        const int last = juce::jmin(width - 1, (int) (centre + radius + 1.f));
        for (int x = first; x <= last; ++x) {
            const float distance = (x - centre) / radius;
            const float falloff = juce::jmax(0.f, 1.f - distance * distance);
            weights[(size_t) x] = juce::jmax(weights[(size_t) x], falloff * falloff * falloff);
        }
    }
    return weights;
}

}

void SpyPlaybackGlint::apply(juce::Image& image, const std::vector<float>& positions) {
    if (!image.isValid() || positions.empty()) {
        return;
    }
    const int height = image.getHeight();
    const int width = image.getWidth();
    const auto weights = columnWeights(width, positions);
    const int first = (int) std::distance(weights.begin(),
            std::find_if(weights.begin(), weights.end(), [](float value) { return value > 0.f; }));
    const int last = width - 1 - (int) std::distance(weights.rbegin(),
            std::find_if(weights.rbegin(), weights.rend(), [](float value) { return value > 0.f; }));
    if (first > last) {
        return;
    }
    const int left = juce::jmax(0, first - 3);
    const int right = juce::jmin(width - 1, last + 3);
    std::vector<float> luminance((size_t) (right - left + 1) * height);
    Channels channels(height);
    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::readWrite);
    for (int x = left; x <= right; ++x) {
        channels.read(pixels, x, height);
        Buffer<float> red(channels.red.data(), height);
        Buffer<float> green(channels.green.data(), height);
        Buffer<float> blue(channels.blue.data(), height);
        red.mul(0.2126f / 255.f).add(green.mul(0.7152f / 255.f)).add(blue.mul(0.0722f / 255.f))
                .copyTo({ luminance.data() + (size_t) (x - left) * height, height });
    }

    std::vector<float> dxValues(height), dyValues(height), lightValues(height), ridgeValues(height), scratchValues(height);
    Buffer<float> dx(dxValues.data(), height);
    Buffer<float> dy(dyValues.data(), height);
    Buffer<float> light(lightValues.data(), height);
    Buffer<float> ridge(ridgeValues.data(), height);
    Buffer<float> scratch(scratchValues.data(), height);
    for (int x = first; x <= last; ++x) {
        const float weight = weights[(size_t) x];
        if (weight == 0.f) {
            continue;
        }
        Buffer<float> column(luminance.data() + (size_t) (x - left) * height, height);
        Buffer<float> previous(luminance.data() + (size_t) (juce::jmax(left, x - 3) - left) * height, height);
        Buffer<float> next(luminance.data() + (size_t) (juce::jmin(right, x + 3) - left) * height, height);
        next.copyTo(dx);
        dx.sub(previous);
        for (int y = 0; y < height; ++y) {
            dy[y] = column[juce::jmin(height - 1, y + 3)];
            scratch[y] = column[juce::jmax(0, y - 3)];
        }
        dy.sub(scratch);
        dx.copyTo(light);
        dy.copyTo(ridge);
        light.sqr().add(ridge.sqr()).sqrt().mul(3.4f).clip(0.f, 1.f);
        column.copyTo(ridge);
        ridge.sub(0.28f).mul(1.f / 0.55f).clip(0.f, 1.f);
        light.mul(0.80f).add(ridge.mul(0.50f)).mul(weight).clip(0.f, 0.90f);
        dx.mul(1.8f).add(0.5f).clip(0.f, 1.f).mul(0.30f).add(0.70f);
        light.mul(dx);
        channels.read(pixels, x, height);
        illuminate({ channels.red.data(), height }, light, 255.f, scratch);
        illuminate({ channels.green.data(), height }, light, 244.8f, scratch);
        illuminate({ channels.blue.data(), height }, light, 219.3f, scratch);
        for (int y = 0; y < height; ++y) {
            pixels.setPixelColour(x, y, juce::Colour((juce::uint8) channels.red[(size_t) y],
                    (juce::uint8) channels.green[(size_t) y], (juce::uint8) channels.blue[(size_t) y]));
        }
    }
}

}
