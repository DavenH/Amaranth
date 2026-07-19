#pragma once

#include <cstddef>

namespace CycleDsp {

constexpr int equalizerBandCount = 5;
constexpr int reverbSizeStepCount = 7;

float equalizerGainDecibels(float unitValue);
float equalizerGainUnitValue(float decibels);
float equalizerFrequency(float unitValue);
float equalizerFrequencyUnitValue(float frequency);

size_t reverbKernelLength(float unitValue);
float reverbSizeUnitValueForStep(int step);
double reverbKernelSeconds(float unitValue, double sampleRate);
float reverbDamping(float unitValue);
float reverbWetLevel(float unitValue);

double delayBeats(float unitValue, int beatsPerMeasure);
float delayUnitValueForBeats(double beats, int beatsPerMeasure);
float delaySnappedUnitValue(
        float unitValue,
        int beatsPerMeasure,
        float sliderWidthPixels,
        float snapDistancePixels = 10.f);

}
