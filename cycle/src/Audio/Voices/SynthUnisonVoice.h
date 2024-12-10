#ifndef _SYNTHUNISONVOICE_H_
#define _SYNTHUNISONVOICE_H_

#include "CycleBasedVoice.h"

class SynthesizerVoice;
class Unison;

class SynthUnisonVoice :
	public CycleBasedVoice
{
public:
	SynthUnisonVoice(SynthesizerVoice* parent, SingletonRepo* repo);
	virtual ~SynthUnisonVoice();

	void calcCycle(VoiceParameterGroup& group);
	void testMeshConditions();
	void initialiseNoteExtra(const int midiNoteNumber, const float velocity);
	void prepNewVoice();

private:
	float lastPitchSemis;
};

#endif

