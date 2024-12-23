#pragma once
#include <vector>

#include "SynthUnisonVoice.h"
#include "SynthFilterVoice.h"
#include "SynthState.h"

#include <App/SingletonAccessor.h>
#include <Array/ScopedAlloc.h>
#include <Audio/SmoothedParameter.h>
#include <Curve/EnvRasterizer.h>
#include "JuceHeader.h"

#include "../../Curve/EnvRenderContext.h"
#include "../../UI/Panels/ModMatrixPanel.h"

using std::vector;

class Mesh;
class SynthAudioSource;
class CycleBasedVoice;
class ModMatrixPanel;
class Unison;
class MeshLibrary;

class SynthesizerVoice:
		public SynthesiserVoice
	,	public SingletonAccessor
{
public:
	SynthesizerVoice(int voiceIndex, SingletonRepo* repo);

	void initCycleBuffers();
	void fetchEnvelopeMeshes();
	void updateCycleCaches();
	void enablementChanged();
	void envGlobalityChanged();
	void prepNewVoice();

	/* Midi */
	void startNote(int midiNoteNumber,
				   float velocity,
				   SynthesiserSound* sound,
				   int currentPitchWheelPosition) override;

	void stop(bool allowTailoff)
	{
		stopNote(velocity, allowTailoff);
	}

	void stopNote(float velocity, bool allowTailOff) override;

	void pitchWheelMoved(int newValue) override;
	void aftertouchChanged(int newValue) override;
	void handleSustainPedal (int midiChannel, bool isDown);

	void controllerMoved(int controllerNumber,
						 int newValue) override;

	/* Rendering */
	void fillRemainingLatencySamples(StereoBuffer& outputBuffer, int numSamples, float mappedVelocity);
	void renderNextBlock(AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override;
	void startLatencyFillingOrStopNote();


	/* Accessors */
//	double 	getBlueMappedValue();
	double	getPitchWheelValueSemitones() 			{ return pitchWheelValue; }
	float 	getModWheelValue()						{ return (float) modValue.getCurrentValue(); }
	int 	getVoiceIndex() const 					{ return voiceIndex; }
	double 	getSampleRatePublic() 					{ return getSampleRate(); }
	bool 	canPlaySound(SynthesiserSound* sound) override 	{ return sound != 0; }
	void 	setModWheelValue(float value) 			{ modValue = value; }
	int 	getCurrentOscillatorLatency();


	/* Misc */
	void updateSmoothedParameters(int deltaSamples);
	void calcDeclickEnvelope(double samplerate);
	void modulationChanged(float value, int outputId, int dim);

private:
	Ref<SynthAudioSource> 	audioSource;
	Ref<Unison> 			unison;
	Ref<ModMatrixPanel>		modMatrix;

	int voiceIndex;
	int octave;
	int attackSamplePos, releaseSamplePos;
	int latencyFillerSamplesLeft;
	int currentLatencySamples;
	int adjustedMidiNote;
	int blockStartOffset;

	float velocity;

	SynthFlag flags;
	SmoothedParameter modValue;
	SmoothedParameter aftertouchValue;
	SmoothedParameter pitchWheelValue;
	SmoothedParameter speedScale;

	MicroTimer timer;
	StereoBuffer renderBuffer;
	EnvRastGroup volumeGroup, pitchGroup, scratchGroup;
	EnvRastGroup* envGroups[3];
	MeshLibrary* meshLib;

	vector<EnvRasterizer*> envRasterizers;

	ScopedAlloc<Ipp32f> chanMemory[2];

	CycleBasedVoice* currentVoice;
	SynthUnisonVoice unisonVoice;
	SynthFilterVoice filterVoice;

	Random random;

	/* ----------------------------------------------------------------------------- */

	friend class SynthFilterVoice;
	friend class SynthUnisonVoice;
	friend class CycleBasedVoice;

	JUCE_LEAK_DETECTOR(SynthesizerVoice)

	void resetNote();
	float normalizeKey(int midiNoteNumber);

	void initialiseEnvMeshes();
	void calcEnvelopeBuffers(int numSamples);
	int getRenderOffset();
	void declickAttack();
	void declickRelease();
	void testMeshConditions();
	float getEffectiveLevel();
};