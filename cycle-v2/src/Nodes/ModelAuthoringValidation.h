#pragma once

#include <JuceHeader.h>

namespace CycleV2::ModelAuthoringValidation {

constexpr int maximumVertices = 16384;
constexpr int maximumCubes = 4096;

bool envelope(const juce::var& value, const juce::String& schema, int version);
bool mesh(const juce::var& value);
bool curve(const juce::var& value, bool isEnvelope);

}
