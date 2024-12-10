#ifndef _graphictableaudiosource_h
#define _graphictableaudiosource_h

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
	GraphicTableAudioSource(SingletonRepo* repo);
	GraphicTableAudioSource(const GraphicTableAudioSource& GraphicTableAudioSource);

	Synthesiser synth;

	void initVoices();
	void releaseResources();
	void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
	void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages);
};

#endif
