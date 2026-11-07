#include <algorithm>
#include <cmath>

#include <Array/Buffer.h>

#include "UI/SpyPlaybackRipple.h"

namespace CycleV2 {

namespace {

struct Channels {
    explicit Channels(int height) :
            hue(height)
        ,   saturation(height)
        ,   lightness(height)
        ,   denominator(height) {}

    std::vector<float> hue;
    std::vector<float> saturation;
    std::vector<float> lightness;
    std::vector<float> denominator;

    void read(const juce::Image::BitmapData& pixels, int x) {
        for (int y = 0; y < (int) hue.size(); ++y) {
            pixels.getPixelColour(x, y).getHSL(hue[(size_t) y],
                    saturation[(size_t) y], lightness[(size_t) y]);
        }
    }

    void saturate(float weight) {
        const int height = (int) hue.size();
        Buffer<float> colours(saturation.data(), height);
        Buffer<float> divisors(denominator.data(), height);
        const float gain = 1.f + 2.f * weight;
        colours.copyTo(divisors);
        divisors.mul(gain - 1.f).add(1.f);
        const float shoulders = 1.f - 3.8f * weight * (1.f - weight);
        colours.mul(gain).div(divisors).mul(shoulders);
    }

    void write(juce::Image::BitmapData& pixels, int x) const {
        for (int y = 0; y < (int) hue.size(); ++y) {
            pixels.setPixelColour(x, y, juce::Colour::fromHSL(hue[(size_t) y],
                    saturation[(size_t) y], lightness[(size_t) y], 1.f));
        }
    }
};

std::vector<float> columnWeights(int width, const std::vector<float>& positions) {
    std::vector<float> weights((size_t) width);
    std::vector<float> scratch((size_t) width);
    const float decay = juce::jmax(1.f, (width - 1) * 0.018f);
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
            channels.saturate(weights[(size_t) x]);
            channels.write(pixels, x);
        }
    }
}

}
