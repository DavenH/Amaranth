#pragma once

#include <JuceHeader.h>
#include <Array/StereoBuffer.h>
#include <vector>

namespace CycleV2 {

// Presentation-only mono sum, scaled to fill the waveform's signed display range.
juce::AudioBuffer<float> referenceWaveformPreview(const StereoBuffer& audio);
std::vector<float> referenceWaveformDensity(Buffer<float> mono, int width, int height);
juce::Image referenceWaveformHeatmap(const StereoBuffer& audio, int width, int height);
juce::Image loadReferenceWaveformHeatmap(const juce::File& file);

}
