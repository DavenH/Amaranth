#pragma once

#include "CycleBasedVoice.h"

class SynthesizerVoice;
class Unison;

class SynthUnisonVoice :
    public CycleBasedVoice
{
public:
    SynthUnisonVoice(SynthesizerVoice* parent, SingletonRepo* repo);
    ~SynthUnisonVoice() override = default;

    void calcCycle(VoiceParameterGroup& group) override;
    void testMeshConditions();
    void initialiseNoteExtra(int midiNoteNumber, float velocity) override;
    void prepNewVoice();

private:
    float lastPitchSemis;
};
