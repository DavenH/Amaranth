#include "Audio/CycleDsp/InternalRateBlockAdapter.h"

namespace CycleDsp {

void InternalRateBlockAdapter::prepare(double outputSampleRate) {
    jassert(outputSampleRate > 0.0);

    outputToInternalRatio = internalSampleRate / juce::jmax(1.0, outputSampleRate);
    outputSamplesProcessed = 0;
    carriedMidi.clearQuick();
}

int InternalRateBlockAdapter::convertBlockSize(int outputSamples) {
    const auto internalSampleCeiling = [this](juce::int64 outputSample) {
        return juce::int64(outputToInternalRatio * double(outputSample) + 0.999999999);
    };

    const juce::int64 nextOutputSample = outputSamplesProcessed + outputSamples;
    const int internalSamples = int(internalSampleCeiling(nextOutputSample)
            - internalSampleCeiling(outputSamplesProcessed));
    outputSamplesProcessed = nextOutputSample;
    return internalSamples;
}

int InternalRateBlockAdapter::convertSampleOffset(int outputSampleOffset) const {
    return juce::roundToInt(outputSampleOffset * outputToInternalRatio);
}

int InternalRateBlockAdapter::convertBlock(
        int outputSamples,
        const juce::MidiBuffer& sourceMidi,
        juce::MidiBuffer& internalMidi) {
    const int internalSamples = convertBlockSize(outputSamples);
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
        internalMidi.addEvent(
                metadata.getMessage(),
                convertSampleOffset(metadata.samplePosition));
    }

    return internalSamples;
}

}
