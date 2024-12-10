#ifndef _graphicsynthvoice_h
#define _graphicsynthvoice_h

#include <vector>
#include <ippdefs.h>

//#define _DEBUG_IPP 1

#include <Array/ScopedAlloc.h>
#include <Array/StereoBuffer.h>
#include "CycleBasedVoice.h"

class Column;

using std::vector;

class GraphicSynthSound :
	public SynthesiserSound
{
public:
	bool appliesToNote(int midiNoteNumber) { return true; }
	bool appliesToChannel(int midiChannel) { return true; }
};

class GraphicSynthVoice :
	public SynthesiserVoice,
	public CycleBasedVoice
{

public:
	GraphicSynthVoice(SingletonRepo* repo);

	void startNote(int midiNoteNumber,
				   float velocity,
				   SynthesiserSound* sound,
				   int currentPitchWheelPosition);

	void stop(bool allowTailoff)
	{
		stopNote(1.f, allowTailoff);
	}

	void stopNote(float velocity, bool allowTailOff);

	bool canPlaySound(SynthesiserSound* sound);

	void aftertouchChanged (int newValue) {}
	void pitchWheelMoved(int newValue);
	void controllerMoved(int controllerNumber,
						 int newValue);

	void renderNextBlock(AudioSampleBuffer& outputBuffer, int startSample, int numSamples);
	void calcCycle(VoiceParameterGroup& group);
	void incrementCurrentX();
	void updateCycleVariables(int groupIndex, int numSamples);

private:
	const vector<Column>* columns;

	StereoBuffer renderBuffer;
	ScopedAlloc<Ipp32f> leftMemory, rightMemory;

	bool playing;
	float increment;
	double currentAngle;
};

#endif
