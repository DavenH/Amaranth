#include <algorithm>
#include <cmath>

#include <Array/Buffer.h>

#include "UI/SpyPlaybackRipple.h"

namespace CycleV2 {

namespace {

struct Channels {
    explicit Channels(int height) : red(height), green(height), blue(height) {}
    std::vector<float> red;
    std::vector<float> green;
    std::vector<float> blue;

    void read(const juce::Image::BitmapData& pixels, int x) {
        for (int y = 0; y < (int) red.size(); ++y) {
            const auto colour = pixels.getPixelColour(x, y);
            red[(size_t) y] = colour.getRed();
            green[(size_t) y] = colour.getGreen();
            blue[(size_t) y] = colour.getBlue();
        }
    }

    void illuminate(float weight) {
        const int height = (int) red.size();
        const float amount = 0.78f * weight;
        Buffer<float>(red.data(), height).mul(1.f - amount).add(150.f * amount);
        Buffer<float>(green.data(), height).mul(1.f - amount).add(232.f * amount);
        Buffer<float>(blue.data(), height).mul(1.f - amount).add(255.f * amount);
    }

    void write(juce::Image::BitmapData& pixels, int x) const {
        for (int y = 0; y < (int) red.size(); ++y) {
            pixels.setPixelColour(x, y, juce::Colour((juce::uint8) red[(size_t) y],
                    (juce::uint8) green[(size_t) y], (juce::uint8) blue[(size_t) y]));
        }
    }
};

std::vector<float> columnWeights(int width, const std::vector<float>& positions) {
    std::vector<float> weights((size_t) width);
    std::vector<float> scratch((size_t) width);
    const float decay = juce::jmax(1.f, (width - 1) * 0.006f);
    for (const float position : positions) {
        const float centre = juce::jlimit(0.f, 1.f, position) * (width - 1);
        const int first = juce::jmax(0, (int) std::ceil(centre - decay * 6.f));
        const int last = juce::jmin(width - 1, (int) std::floor(centre + decay * 6.f));
        const int length = last - first + 1;
        Buffer<float> ripple(scratch.data() + first, length);
        ripple.ramp(first - centre, 1.f).abs().mul(-1.f / decay).exp();
        Buffer<float>(weights.data() + first, length).sub(ripple).threshLT(0.f).add(ripple);
    }
    return weights;
}

}

void SpyPlaybackRipple::apply(juce::Image& image, const std::vector<float>& positions) {
    if (!image.isValid() || positions.empty()) {
        return;
    }
    const auto weights = columnWeights(image.getWidth(), positions);
    Channels channels(image.getHeight());
    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::readWrite);
    for (int x = 0; x < image.getWidth(); ++x) {
        if (weights[(size_t) x] > 0.f) {
            channels.read(pixels, x);
            channels.illuminate(weights[(size_t) x]);
            channels.write(pixels, x);
        }
    }
}

}
