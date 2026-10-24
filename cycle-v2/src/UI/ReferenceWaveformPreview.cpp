#include "UI/ReferenceWaveformPreview.h"

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

}
