#include "EffectParameterMapping.h"

#include <Util/NumberUtils.h>

#include <algorithm>
#include <cmath>

namespace CycleDsp {

namespace {

constexpr float kEqualizerFrequencyTension = 50.f;
constexpr float kEqualizerMaximumFrequency = 16000.f;
constexpr float kEqualizerMinimumFrequency = 40.f;

float unitValue(float value) {
    return std::clamp(value, 0.f, 1.f);
}

}

float equalizerGainDecibels(float value) {
    return 60.f * (unitValue(value) - 0.5f);
}

float equalizerGainUnitValue(float decibels) {
    return unitValue(decibels / 60.f + 0.5f);
}

float equalizerFrequency(float value) {
    const float exponent = unitValue(value) * std::log(kEqualizerFrequencyTension + 1.f);
    const float frequency = kEqualizerMaximumFrequency
            * (std::exp(exponent) - 1.f)
            / kEqualizerFrequencyTension;
    return std::max(kEqualizerMinimumFrequency, frequency);
}

float equalizerFrequencyUnitValue(float frequency) {
    const float normalizedFrequency = std::max(0.f, frequency) / kEqualizerMaximumFrequency;
    return unitValue(
            std::log(kEqualizerFrequencyTension * normalizedFrequency + 1.f)
            / std::log(kEqualizerFrequencyTension + 1.f));
}

size_t reverbKernelLength(float value) {
    const auto requested = static_cast<unsigned>(
            std::pow(2.f, 12.f + 6.f * unitValue(value)));
    return static_cast<size_t>(NumberUtils::nextPower2(requested));
}

double reverbKernelSeconds(float value, double sampleRate) {
    return static_cast<double>(reverbKernelLength(value)) / std::max(1.0, sampleRate);
}

float reverbDamping(float value) {
    return 0.7f * unitValue(value);
}

float reverbWetLevel(float value) {
    return 0.25f * unitValue(value);
}

double delayBeats(float value, int beatsPerMeasure) {
    const double normalizedValue = std::max(0.15, static_cast<double>(unitValue(value)));
    return static_cast<double>(std::max(1, beatsPerMeasure))
            * normalizedValue
            * normalizedValue;
}

float delayUnitValueForBeats(double beats, int beatsPerMeasure) {
    const double normalizedBeats = std::max(0.0, beats);
    const double normalizedMeasure = (double) std::max(1, beatsPerMeasure);
    return unitValue((float) std::sqrt(normalizedBeats / normalizedMeasure));
}

float delaySnappedUnitValue(float value, int beatsPerMeasure) {
    const int normalizedMeasure = std::max(1, beatsPerMeasure);
    const double beats = delayBeats(value, normalizedMeasure);
    const double nearestBeat = std::round(beats);
    constexpr double snapDistanceBeats = 0.1;
    return std::abs(beats - nearestBeat) <= snapDistanceBeats
            ? delayUnitValueForBeats(nearestBeat, normalizedMeasure)
            : unitValue(value);
}

}
