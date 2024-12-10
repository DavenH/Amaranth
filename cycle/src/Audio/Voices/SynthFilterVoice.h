#ifndef _SYNTHFILTERVOICE_H_
#define _SYNTHFILTERVOICE_H_


#include <ippdefs.h>
#include <App/MeshLibrary.h>
#include <Array/ScopedAlloc.h>
#include <Algo/Convolver.h>
#include <Obj/Ref.h>
#include "SynthState.h"
#include "CycleBasedVoice.h"
#include "VoiceParameterGroup.h"

class Mesh;
class SynthesizerVoice;

class SynthFilterVoice :
	public CycleBasedVoice
{
public:
	SynthFilterVoice(SynthesizerVoice* parent, SingletonRepo* repo);
	virtual ~SynthFilterVoice();

	void calcCycle(VoiceParameterGroup& group);
	bool calcTimeDomain(VoiceParameterGroup& group, int samplingSize, int oversampleFactor);
	void calcMagnitudeFilters(Buffer<Ipp32f> fftRamp);
	void calcPhaseDomain(Buffer<float> fftRamp, bool didFwdFFT, bool rightPhasesAreSet, int& channelCount);

	void initialiseNoteExtra(const int midiNoteNumber, const float velocity);
	void testMeshConditions();
	void updateCachedCycles();
	void updateValue(int outputId, int dim, float value);

private:
	MeshRasterizer freqRasterizer;
	MeshRasterizer phaseRasterizer;

	ScopedAlloc<Ipp32f> magnitudes[2];
	ScopedAlloc<Ipp32f> phases[2];
	ScopedAlloc<Ipp32f> phaseAccumBuffer[2];
	ScopedAlloc<Ipp32f> phaseScaleRamp;
	ScopedAlloc<Ipp32f> latencyMoveBuff;

	Buffer<float> rastBuf;
	Buffer<float> magBufs[2], phaseBufs[2], samplingBufs[2], accumBufs[2];

	Ref<MeshLibrary::LayerGroup> freqLayers;
	Ref<MeshLibrary::LayerGroup> phaseLayers;

	friend class SynthesizerVoice;
};

#endif
