#pragma once

#include <JuceHeader.h>
#include <Array/StereoBuffer.h>

namespace CycleV2 {

// Presentation-only mono sum, scaled to fill the waveform's signed display range.
juce::AudioBuffer<float> referenceWaveformPreview(const StereoBuffer& audio);

}
