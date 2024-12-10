#ifndef _wavaudiosource_h
#define _wavaudiosource_h

#include <ipp.h>
#include <iostream>
#include <Algo/HermiteResampler.h>
#include <Algo/Resampler.h>
#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include <Array/RingBuffer.h>
#include <Audio/AudioSourceProcessor.h>
#include <Audio/SampleWrapper.h>
#include <Obj/Ref.h>
#include <Util/Arithmetic.h>
#include "JuceHeader.h"


class WavAudioSource:
		public AudioSourceProcessor
	,	public SingletonAccessor
{
public:

	WavAudioSource(SingletonRepo* repo) ;
	void allNotesOff();
	void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
	void initResampler();
	void releaseResources()	{}
	void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages);
	void positionChanged();
	void reset();

private:
	bool updateOffsetFlag;

	class SampleSynth : public Synthesiser
	{
	public:
		void handleController (int midiChannel, int controllerNumber, int controllerValue) {}
	};

	class SampleSound :
			public SynthesiserSound
		,	public SingletonAccessor
	{
	public:
		SampleSound(SingletonRepo* repo) : SingletonAccessor(repo, "SampleSound") {}

		bool appliesToNote(int midiNoteNumber);
		bool appliesToChannel(int midiChannel);
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
				   	   int currentPitchWheelPosition);

		void stop(bool allowTailoff) { stopNote(velocity, allowTailoff); }
		void stopNote(float velocity, bool allowTailOff);

		void aftertouchChanged (int newValue) 	{}
		void pitchWheelMoved(int newValue) 		{}
		void controllerMoved(int controllerNumber, int newValue) {}
		bool canPlaySound(SynthesiserSound* sound) 			{ return true; }
		SampleWrapper* getSample() 							{ return sample; }

		void renderNextBlock(AudioSampleBuffer& outputBuffer, int startSample, int numSamples);

	private:
		bool isPlaying;
		int offsetSamples, releasePos;
		float velocity;
		int lastNote;
		bool fadingOut;

		WavAudioSource* source;
		HermiteState resampleState[2];
		ScopedAlloc<float> stateMem, resMem;
		SampleWrapper* sample;
	};

	int blockSize;
	Array<SampleVoice*> voices;
	SampleSynth synth;
	ScopedAlloc<Ipp32f> chanMemory;
};

#endif
