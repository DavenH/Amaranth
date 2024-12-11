#pragma once

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
	bool appliesToNote(int midiNoteNumber) override { return true; }
	bool appliesToChannel(int midiChannel) override { return true; }
};

class GraphicSynthVoice :
	public SynthesiserVoice,
	public CycleBasedVoice
{

public:
	explicit GraphicSynthVoice(SingletonRepo* repo);

	void startNote(int midiNoteNumber,
				   float velocity,
				   SynthesiserSound* sound,
				   int currentPitchWheelPosition) override;

	void stop(bool allowTailoff) {
		stopNote(1.f, allowTailoff);
	}

	void stopNote(float velocity, bool allowTailOff) override;
	bool canPlaySound(SynthesiserSound* sound) override;

	void aftertouchChanged (int newValue) override {}
	void pitchWheelMoved(int newValue) override;
	void controllerMoved(int controllerNumber, int newValue) override;
	void renderNextBlock(AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override;
	void calcCycle(VoiceParameterGroup& group) override;

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