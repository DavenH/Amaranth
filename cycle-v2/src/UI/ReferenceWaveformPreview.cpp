#include "UI/ReferenceWaveformPreview.h"

#include <Algo/Resampling.h>
#include <Audio/PitchedSample.h>
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

juce::AudioBuffer<float> referenceWaveformPreview(const StereoBuffer& audio) {
    juce::AudioBuffer<float> result(1, audio.left.size());
    if (audio.left.empty()) {
        return result;
    }
    Buffer<float> mono(result, 0);
    audio.left.copyTo(mono);
    if (audio.numChannels > 1) {
        mono.add(audio.right);
    }
    const float peak = juce::jmax(mono.max(), -mono.min());
    if (peak > 0.f) {
        mono.mul(1.f / peak);
    }
    return result;
}

std::vector<float> referenceWaveformDensity(Buffer<float> mono, int width, int height) {
    if (mono.empty() || width < 1 || height < 2) {
        return {};
    }
    const int stride = height + 1;
    std::vector<float> differences((size_t) width * (size_t) stride, 0.f);
    std::vector<float> density((size_t) width * (size_t) height, 0.f);
    const float sampleStep = (float) (mono.size() - 1) / (float) width;
    const float yScale = 0.5f * (float) (height - 1);
    for (int x = 0; x < width; ++x) {
        auto* column = differences.data() + (size_t) x * (size_t) stride;
        const float start = (float) x * sampleStep;
        const float end = juce::jmin((float) (mono.size() - 1), (float) (x + 1) * sampleStep);
        float previous = Resampling::lerpC(mono, start / (float) mono.size());
        const auto accumulate = [&](float next) {
            const int first = juce::jlimit(0, height - 1,
                    (int) ((1.f - juce::jmax(previous, next)) * yScale));
            const int last = juce::jlimit(0, height - 1,
                    (int) ((1.f - juce::jmin(previous, next)) * yScale));
            column[first] += 1.f;
            column[last + 1] -= 1.f;
            previous = next;
        };
        for (int index = (int) start + 1; index <= (int) end; ++index) {
            accumulate(mono[index]);
        }
        if (end > (float) (int) end || start == end) {
            accumulate(Resampling::lerpC(mono, end / (float) mono.size()));
        }
        float count = 0.f;
        for (int y = 0; y < height; ++y) {
            count += column[y];
            density[(size_t) x * (size_t) height + (size_t) y] = count;
        }
    }
    return density;
}

juce::Image referenceWaveformHeatmap(const StereoBuffer& audio, int width, int height) {
    if (width < 1 || height < 2) {
        return {};
    }
    juce::Image image(juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
    auto preview = referenceWaveformPreview(audio);
    Buffer<float> mono(preview, 0);
    if (mono.empty() || (mono.max() == 0.f && mono.min() == 0.f)) {
        return image;
    }
    auto density = referenceWaveformDensity(mono, width, height);
    Buffer<float> values(density.data(), (int) density.size());
    values.add(1.f).ln();
    const float peak = values.max();
    if (peak > 0.f) {
        values.mul(1.f / peak);
    }
    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::writeOnly);
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            const float value = density[(size_t) x * (size_t) height + (size_t) y];
            if (value > 0.f) {
                pixels.setPixelColour(x, y, CanvasChromePalette::text.withAlpha(0.15f + 0.85f * value));
            }
        }
    }
    return image;
}

juce::Image loadReferenceWaveformHeatmap(const juce::File& file) {
    PitchedSample sample;
    if (sample.load(file.getFullPathName(), 5.0) < 0) {
        return {};
    }
    return referenceWaveformHeatmap(sample.audio, 512, 96);
}

}
