#pragma once

#include <ipp.h>
#include <Algo/HermiteResampler.h>
#include <App/SingletonAccessor.h>
#include <Array/RingBuffer.h>
#include <Audio/AudioSourceProcessor.h>
#include "JuceHeader.h"

class WavAudioSource:
        public AudioSourceProcessor
    ,	public SingletonAccessor
{
public:

    explicit WavAudioSource(SingletonRepo* repo) ;
    void allNotesOff();
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void initResampler();
    void releaseResources() override	{}
    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override;
    void positionChanged();
    void reset() override;

private:
    bool updateOffsetFlag;

    class SampleSynth : public Synthesiser
    {
    public:
        void handleController (int midiChannel, int controllerNumber, int controllerValue) override {}
    };

    class SampleSound :
            public SynthesiserSound
        ,	public SingletonAccessor
    {
    public:
        explicit SampleSound(SingletonRepo* repo) : SingletonAccessor(repo, "SampleSound") {}

        bool appliesToNote(int midiNoteNumber) override;
        bool appliesToChannel(int midiChannel) override;
    };

    class SampleVoice :
            public SynthesiserVoice
        ,	public SingletonAccessor
    {
    public:
        SampleVoice(SingletonRepo* repo, WavAudioSource* source);
        void startNote(int midiNoteNumber,
                       float velocity,
                       SynthesiserSound* sound,
                       int currentPitchWheelPosition) override;

        void stop(bool allowTailoff) { stopNote(velocity, allowTailoff); }
        void stopNote(float velocity, bool allowTailOff) override;

        void aftertouchChanged (int newValue) override 	{}
        void pitchWheelMoved(int newValue) override 		{}
        void controllerMoved(int controllerNumber, int newValue) override {}
        bool canPlaySound(SynthesiserSound* sound) override 			{ return true; }
        PitchedSample* getSample() 							{ return sample; }

        void renderNextBlock(AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override;

    private:
        bool isPlaying;
        int offsetSamples, releasePos;
        float velocity;
        int lastNote;
        bool fadingOut;

        WavAudioSource* source;
        HermiteState resampleState[2];
        ScopedAlloc<float> stateMem, resMem;
        PitchedSample* sample;
    };

    int blockSize{};
    Array<SampleVoice*> voices;
    SampleSynth synth;
    ScopedAlloc<Ipp32f> chanMemory;
};