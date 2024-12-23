#pragma once

#include <ipp.h>
#include <App/MeshLibrary.h>
#include <Array/ScopedAlloc.h>
#include <Algo/Convolver.h>
#include <Curve/MeshRasterizer.h>
#include <Obj/Ref.h>
#include "CycleBasedVoice.h"
#include "VoiceParameterGroup.h"

class Mesh;
class SynthesizerVoice;

class SynthFilterVoice :
	public CycleBasedVoice
{
public:
	SynthFilterVoice(SynthesizerVoice* parent, SingletonRepo* repo);

	~SynthFilterVoice() override = default;

	void calcCycle(VoiceParameterGroup& group) override;
	bool calcTimeDomain(VoiceParameterGroup& group, int samplingSize, int oversampleFactor);
	void calcMagnitudeFilters(Buffer<Ipp32f> fftRamp);
	void calcPhaseDomain(Buffer<float> fftRamp, bool didFwdFFT, bool rightPhasesAreSet, int& channelCount);

	void initialiseNoteExtra(int midiNoteNumber, float velocity) override;
	void testMeshConditions();
	void updateCachedCycles();
	void updateValue(int outputId, int dim, float value) override;

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