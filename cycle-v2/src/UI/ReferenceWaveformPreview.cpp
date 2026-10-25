#include "UI/ReferenceWaveformPreview.h"

#include <Algo/Resampling.h>
#include <Audio/PitchedSample.h>
#include <Array/ScopedAlloc.h>
#include <algorithm>
#include "UI/CanvasChromePalette.h"

namespace CycleV2 {

ReferenceWaveformResolution referenceWaveformResolution(const std::vector<int>& crossings,
        int start, int end, int pixelWidth) {
    const auto first = std::lower_bound(crossings.begin(), crossings.end(), start);
    const auto last = std::lower_bound(first, crossings.end(), end);
    const auto count = (size_t) std::distance(first, last);
    return { count, pixelWidth > 0 && count <= (size_t) pixelWidth };
}

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

std::vector<float> referenceWaveformDensity(Buffer<float> mono, int width, int height,
        float y, float span) {
    if (mono.empty() || width < 1 || height < 2 || span <= 0.f) {
        return {};
    }
    const int stride = height + 1;
    std::vector<float> differences((size_t) width * (size_t) stride, 0.f);
    std::vector<float> density((size_t) width * (size_t) height, 0.f);
    const float sampleStep = (float) (mono.size() - 1) / (float) width;
    const float yScale = (float) (height - 1) / span;
    for (int x = 0; x < width; ++x) {
        auto* column = differences.data() + (size_t) x * (size_t) stride;
        const float start = (float) x * sampleStep;
        const float end = juce::jmin((float) (mono.size() - 1), (float) (x + 1) * sampleStep);
        float previous = Resampling::lerpC(mono, start / (float) mono.size());
        const auto accumulate = [&](float next) {
            const float top = 0.5f * (1.f - juce::jmax(previous, next));
            const float bottom = 0.5f * (1.f - juce::jmin(previous, next));
            if (bottom < y || top > y + span) {
                previous = next;
                return;
            }
            const int first = juce::jlimit(0, height - 1,
                    (int) ((top - y) * yScale));
            const int last = juce::jlimit(0, height - 1,
                    (int) ((bottom - y) * yScale));
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
    auto preview = referenceWaveformPreview(audio);
    return referenceWaveformHeatmap(Buffer<float>(preview, 0), width, height);
}

juce::Image referenceWaveformHeatmap(Buffer<float> mono, int width, int height, float y, float span) {
    if (width < 1 || height < 2) {
        return {};
    }
    juce::Image image(juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
    if (mono.empty() || (mono.max() == 0.f && mono.min() == 0.f)) {
        return image;
    }
    auto density = referenceWaveformDensity(mono, width, height, y, span);
    if (density.empty()) {
        return image;
    }
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

juce::Image referenceWaveformLine(Buffer<float> mono, int width, int height,
        float y, float span, juce::Range<float> sampleRange) {
    if (mono.empty() || width < 1 || height < 2 || span <= 0.f) {
        return {};
    }
    juce::Image image(juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
    ScopedAlloc<float> memory(mono.size() * 2);
    auto horizontal = memory.place(mono.size());
    auto vertical = memory.place(mono.size());
    if (sampleRange.isEmpty()) {
        sampleRange = { 0.f, (float) juce::jmax(1, mono.size() - 1) };
    }
    const float xScale = (float) (width - 1) / sampleRange.getLength();
    horizontal.ramp(-sampleRange.getStart() * xScale, xScale);
    mono.copyTo(vertical);
    vertical.mul(-0.5f).add(0.5f - y).mul((float) (height - 1) / span);
    juce::Path trace;
    trace.preallocateSpace(mono.size() * 3);
    trace.startNewSubPath(horizontal[0], vertical[0]);
    for (int index = 1; index < mono.size(); ++index) {
        trace.lineTo(horizontal[index], vertical[index]);
    }
    if (sampleRange.getEnd() > (float) (mono.size() - 1)) {
        trace.lineTo((float) (width - 1), vertical.back());
    }
    juce::Graphics graphics(image);
    graphics.setColour(CanvasChromePalette::text);
    graphics.strokePath(trace, juce::PathStrokeType(1.f));
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
