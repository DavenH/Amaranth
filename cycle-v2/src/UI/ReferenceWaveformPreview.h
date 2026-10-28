#pragma once

#include <JuceHeader.h>
#include <Array/StereoBuffer.h>
#include <vector>

namespace CycleV2 {

struct ReferenceWaveformResolution {
    size_t crossings {};
    bool useLine {};
};

ReferenceWaveformResolution referenceWaveformResolution(const std::vector<int>& crossings,
        int start, int end, int pixelWidth);

// Presentation-only mono sum, scaled to fill the waveform's signed display range.
juce::AudioBuffer<float> referenceWaveformPreview(const StereoBuffer& audio);
std::vector<float> referenceWaveformDensity(Buffer<float> mono, int width, int height,
        float y = 0.f, float span = 1.f);
juce::Image referenceWaveformHeatmap(const StereoBuffer& audio, int width, int height);
juce::Image referenceWaveformHeatmap(Buffer<float> normalizedMono, int width, int height,
        float y = 0.f, float span = 1.f);
juce::Image referenceWaveformLine(Buffer<float> normalizedMono, int width, int height,
        float y = 0.f, float span = 1.f, juce::Range<float> sampleRange = {});
juce::Image loadReferenceWaveformHeatmap(const juce::File& file);

}
