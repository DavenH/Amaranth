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

float waveshaperGainDecibels(float value) {
    return 90.f * (unitValue(value) - 0.5f);
}

float waveshaperGainUnitValue(float decibels) {
    return unitValue(decibels / 90.f + 0.5f);
}

float equalizerGainDecibels(float value) {
    return 60.f * (unitValue(value) - 0.5f);
}

float equalizerGainUnitValue(float decibels) {
    return unitValue(decibels / 60.f + 0.5f);
}

float equalizerGainSnappedUnitValue(
        float value,
        float sliderWidthPixels,
        float snapDistancePixels) {
    const float normalized = unitValue(value);
    const float distance = std::abs(normalized - 0.5f) * std::max(1.f, sliderWidthPixels);
    return distance <= std::max(0.f, snapDistancePixels) ? 0.5f : normalized;
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

float reverbSizeUnitValueForStep(int step) {
    const int clampedStep = std::clamp(step, 0, reverbSizeStepCount - 1);
    if (clampedStep == 0 || clampedStep == reverbSizeStepCount - 1) {
        return (float) clampedStep / (float) (reverbSizeStepCount - 1);
    }
    const float boundary = (float) clampedStep / (float) (reverbSizeStepCount - 1);
    constexpr float sliderInterval = 0.0001f;
    return (std::ceil(boundary / sliderInterval) - 1.f) * sliderInterval;
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

float delaySnappedUnitValue(
        float value,
        int beatsPerMeasure,
        float sliderWidthPixels,
        float snapDistancePixels) {
    const int normalizedMeasure = std::max(1, beatsPerMeasure);
    const float normalizedValue = unitValue(value);
    const float normalizedWidth = std::max(1.f, sliderWidthPixels);
    const float normalizedSnapDistance = std::max(0.f, snapDistancePixels);
    float closestValue = normalizedValue;
    float closestDistance = normalizedSnapDistance + 1.f;

    const float halfBeatValue = delayUnitValueForBeats(0.5, normalizedMeasure);
    const float halfBeatDistance = std::abs(halfBeatValue - normalizedValue) * normalizedWidth;
    if (halfBeatDistance < closestDistance) {
        closestValue = halfBeatValue;
        closestDistance = halfBeatDistance;
    }

    for (int beat = 0; beat <= normalizedMeasure; ++beat) {
        const float beatValue = delayUnitValueForBeats((double) beat, normalizedMeasure);
        const float distance = std::abs(beatValue - normalizedValue) * normalizedWidth;
        if (distance < closestDistance) {
            closestValue = beatValue;
            closestDistance = distance;
        }
    }

    return closestDistance <= normalizedSnapDistance ? closestValue : normalizedValue;
}

double voiceLengthSeconds(float unitValue) {
    return std::exp(8.0 * std::clamp(unitValue, 0.f, 1.f) - 3.0);
}

float voiceLengthUnitValue(double seconds) {
    constexpr double minimumSeconds = 0.049787068367863944;
    const double clamped = std::max(minimumSeconds, seconds);
    return std::clamp((float) ((std::log(clamped) + 3.0) / 8.0), 0.f, 1.f);
}

}
