#include "InternalRateBlockAdapter.h"

void InternalRateBlockAdapter::prepare(double outputSampleRate) {
    jassert(outputSampleRate > 0.0);

    outputToInternalRatio = 44100.0 / juce::jmax(1.0, outputSampleRate);
    outputSamplesProcessed = 0;
    carriedMidi.clearQuick();
}

int InternalRateBlockAdapter::convertBlock(
        int outputSamples,
        const juce::MidiBuffer& sourceMidi,
        juce::MidiBuffer& internalMidi) {
    const auto internalSampleCeiling = [this](juce::int64 outputSample) {
        return juce::int64(outputToInternalRatio * double(outputSample) + 0.999999999);
    };

    juce::int64 nextOutputSample = outputSamplesProcessed + outputSamples;
    int internalSamples = int(internalSampleCeiling(nextOutputSample)
            - internalSampleCeiling(outputSamplesProcessed));
    outputSamplesProcessed = nextOutputSample;
    internalMidi.clear();

    if (internalSamples == 0) {
        for (const juce::MidiMessageMetadata metadata : sourceMidi) {
            carriedMidi.add(metadata.getMessage());
        }

        return 0;
    }

    for (const juce::MidiMessage& message : carriedMidi) {
        internalMidi.addEvent(message, 0);
    }
    carriedMidi.clearQuick();

    for (const juce::MidiMessageMetadata metadata : sourceMidi) {
        int samplePosition = juce::roundToInt(metadata.samplePosition * outputToInternalRatio);
        internalMidi.addEvent(metadata.getMessage(), samplePosition);
    }

    return internalSamples;
}
