#pragma once

#include <cstddef>

namespace CycleDsp {

constexpr int equalizerBandCount = 5;

float equalizerGainDecibels(float unitValue);
float equalizerGainUnitValue(float decibels);
float equalizerFrequency(float unitValue);
float equalizerFrequencyUnitValue(float frequency);

size_t reverbKernelLength(float unitValue);
double reverbKernelSeconds(float unitValue, double sampleRate);
float reverbDamping(float unitValue);
float reverbWetLevel(float unitValue);

double delayBeats(float unitValue, int beatsPerMeasure);

}
