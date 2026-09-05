#include "IrModel.h"

#include <Array/ScopedAlloc.h>

#include <algorithm>
#include <cmath>

namespace CycleDsp {

int irImpulseLength(double normalizedValue) {
    constexpr double boundaryTolerance = 1.0e-5;
    return 1 << int(7. + normalizedValue * 7. + boundaryTolerance);
}

double irImpulseLengthValue(int length) {
    return (std::log(length) / std::log(2.0) - 7.) / 7.;
}

float irPostGain(double normalizedValue) {
    return (float) std::exp(10. * normalizedValue - 5.);
}

float irPostGainDecibels(double normalizedValue) {
    constexpr double decibelsPerNaturalExponent = 20.0 / 2.302585092994046;
    return (float) (decibelsPerNaturalExponent * (10.0 * normalizedValue - 5.0));
}

double irPostGainValueForDecibels(float decibels) {
    constexpr double naturalExponentPerDecibel = 2.302585092994046 / 20.0;
    return std::clamp((naturalExponentPerDecibel * decibels + 5.0) / 10.0, 0.0, 1.0);
}

float irPrefilterAmount(double normalizedValue) {
    return (float) (normalizedValue * normalizedValue * normalizedValue);
}

double irPrefilterValueForAmount(float amount) {
    return std::cbrt(std::clamp(amount, 0.f, 1.f));
}

float irPrefilterFrequency(double normalizedValue, double sampleRate) {
    return irPrefilterAmount(normalizedValue) * (float) std::max(0.0, sampleRate * 0.5);
}

double irPrefilterValueForFrequency(float frequency, double sampleRate) {
    if (sampleRate <= 0.0) {
        return 0.0;
    }
    return irPrefilterValueForAmount((float) (frequency / (sampleRate * 0.5)));
}

int irTrimmedSampleCount(Buffer<float> samples, bool* silent) {
    if (samples.size() < 64) {
        if (silent != nullptr) {
            *silent = false;
        }
        return samples.size();
    }

    ScopedAlloc<float> magnitudeMemory(samples.size());
    Buffer<float> magnitudes = magnitudeMemory.place(samples.size());
    samples.copyTo(magnitudes);
    magnitudes.abs();

    constexpr float trimThreshold = 0.001f;
    float movingAverage {};
    int thresholdIndex = samples.size() - 1;
    for (int offset = 0; offset < samples.size(); ++offset) {
        thresholdIndex = samples.size() - offset - 1;
        movingAverage = 0.95f * movingAverage + 0.05f * magnitudes[thresholdIndex];
        if (movingAverage > trimThreshold) {
            break;
        }
    }

    if (silent != nullptr) {
        *silent = thresholdIndex == 0;
    }
    return std::clamp(thresholdIndex + 1, 64, 16384);
}

void buildIrPrefilterLevels(Buffer<float> levels, double normalizedValue) {
    levels.set(1.f).zero((int) (irPrefilterAmount(normalizedValue) * levels.size()));
}

void rasterizeIrImpulse(
        Rasterization::SamplerView sampler,
        Buffer<float> impulse,
        Oversampler& oversampler,
        double padding) {
    const double delta = (1. - padding) / (double) (impulse.size() - 1);
    const int samplingSize = impulse.size() * oversampler.getOversampleFactor();
    Buffer<float> oversampled = oversampler.getMemoryBuffer(samplingSize);

    (void) sampler.samplePerfectly(
            delta / oversampler.getOversampleFactor(),
            oversampled,
            padding);
    oversampler.sampleDown(oversampled, impulse);
}

void applyIrFrequencyPrefilter(
        Buffer<float> rawImpulse,
        Buffer<float> filteredImpulse,
        Buffer<float> levels,
        bool removeDc,
        Transform& transform) {
    transform.setRemovesOffset(removeDc);
    transform.forward(rawImpulse);
    transform.getMagnitudes().mul(levels);
    transform.inverse(filteredImpulse);
}

}
