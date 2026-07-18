#include "EqualizerCore.h"

#include <Algo/IIR.h>
#include <Audio/Filters/Butterworth.h>

#include <algorithm>
#include <cmath>

namespace CycleDsp {

class EqualizerCore::Band {
public:
    Band(int channelCount, int index) : iir(channelCount), index(index) {}

    void configure(double sampleRate, float frequency, float gainDecibels) {
        if (index == 0) {
            lowShelf.setup(1, sampleRate, frequency, gainDecibels);
            cascade = &lowShelf;
        } else if (index == bandCount - 1) {
            highShelf.setup(1, sampleRate, frequency, gainDecibels);
            cascade = &highShelf;
        } else {
            bandShelf.setup(1, sampleRate, frequency, frequency * 0.7f, gainDecibels);
            cascade = &bandShelf;
        }

        iir.updateFromCascade(*cascade);
    }

    float responseMagnitude(double normalizedFrequency) const {
        return cascade != nullptr
                ? static_cast<float>(std::abs(cascade->response(normalizedFrequency)))
                : 1.f;
    }

    Dsp::SimpleFilter<Dsp::Butterworth::LowShelf<2>> lowShelf;
    Dsp::SimpleFilter<Dsp::Butterworth::BandShelf<2>> bandShelf;
    Dsp::SimpleFilter<Dsp::Butterworth::HighShelf<2>> highShelf;
    Dsp::Cascade* cascade {};
    IIR iir;
    int index;
};

EqualizerCore::EqualizerCore(int channels) : channelCount(std::max(1, channels)) {
    bands.reserve(bandCount);
    for (int index = 0; index < bandCount; ++index) {
        bands.push_back(std::make_unique<Band>(channelCount, index));
    }
}

EqualizerCore::~EqualizerCore() = default;

void EqualizerCore::configureBand(
        int bandIndex,
        double nextSampleRate,
        float frequency,
        float gainDecibels) {
    if (bandIndex < 0 || bandIndex >= bandCount) {
        return;
    }

    sampleRate = std::max(1.0, nextSampleRate);
    const float limitedFrequency = std::clamp(
            frequency,
            20.f,
            static_cast<float>(sampleRate * 0.45));
    bands[(size_t) bandIndex]->configure(
            sampleRate,
            limitedFrequency,
            std::clamp(gainDecibels, -30.f, 30.f));
}

void EqualizerCore::process(int channel, Buffer<float> samples) {
    if (channel < 0 || channel >= channelCount) {
        return;
    }

    for (const auto& band : bands) {
        band->iir.process(channel, samples);
    }
}

void EqualizerCore::clear() {
    for (const auto& band : bands) {
        band->iir.clear();
    }
}

float EqualizerCore::responseDecibels(double frequency) const {
    const double normalizedFrequency = std::clamp(frequency / sampleRate, 0.0, 0.5);
    double magnitude = 1.0;
    for (const auto& band : bands) {
        magnitude *= band->responseMagnitude(normalizedFrequency);
    }

    return static_cast<float>(20.0 * std::log10(std::max(1.0e-9, magnitude)));
}

}
