#pragma once

#include "JuceHeader.h"

using namespace juce;

class AudioSourceProcessor : public AudioSource {
protected:
    CriticalSection audioLock;

public:
    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override {
        AudioBuffer section(bufferToFill.buffer->getArrayOfWritePointers(),
                            bufferToFill.startSample, bufferToFill.numSamples);

        MidiBuffer midi;
        processBlock(section, midi);
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override = 0;
    void releaseResources() override = 0;
    virtual void processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages) = 0;

    virtual void doAudioThreadUpdates() {}

    CriticalSection& getLock() { return audioLock; }
};
