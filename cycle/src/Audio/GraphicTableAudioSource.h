#pragma once

#include "JuceHeader.h"
#include <Audio/AudioSourceProcessor.h>
#include <App/SingletonAccessor.h>
#include "Effects/AudioEffect.h"

class GraphicTableAudioSource :
        public AudioSourceProcessor
    ,	public SingletonAccessor
{
private:
    Effect* delay;
    Effect* reverb;

    enum { numTableVoices = 1 };

public:
    explicit GraphicTableAudioSource(SingletonRepo* repo);
    GraphicTableAudioSource(const GraphicTableAudioSource& GraphicTableAudioSource);

    Synthesiser synth;

    void initVoices();
    void releaseResources() override;
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override;
};