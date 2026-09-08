#pragma once

#include "JuceHeader.h"

class InternalRateBlockAdapter {
public:
    static constexpr double internalSampleRate = 44100.0;

    void prepare(double outputSampleRate);
    int convertBlock(
            int outputSamples,
            const juce::MidiBuffer& sourceMidi,
            juce::MidiBuffer& internalMidi);

private:
    double outputToInternalRatio { 1.0 };
    juce::int64 outputSamplesProcessed { 0 };
    juce::Array<juce::MidiMessage> carriedMidi;
};
