#pragma once

#include <App/MeshLibrary.h>
#include <Array/ScopedAlloc.h>
#include <Algo/Convolver.h>
#include <Wireframe/OldMeshRasterizer.h>
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
	void calcMagnitudeFilters(Buffer<Float32> fftRamp);
	void calcPhaseDomain(Buffer<float> fftRamp, bool didFwdFFT, bool rightPhasesAreSet, int& channelCount);

	void initialiseNoteExtra(int midiNoteNumber, float velocity) override;
	void testMeshConditions();
	void updateCachedCycles();
	void updateValue(int outputId, int dim, float value) override;

private:
	OldMeshRasterizer freqRasterizer;
	OldMeshRasterizer phaseRasterizer;

	// TODO use stereo buffers, use some workbuffer for allocation
	ScopedAlloc<Float32> magnitudes[2];
	ScopedAlloc<Float32> phases[2];
	ScopedAlloc<Float32> phaseAccumBuffer[2];
	ScopedAlloc<Float32> phaseScaleRamp;
	ScopedAlloc<Float32> latencyMoveBuff;

	Buffer<float> rastBuf;
	Buffer<float> magBufs[2], phaseBufs[2], samplingBufs[2], accumBufs[2];

	Ref<MeshLibrary::LayerGroup> freqLayers;
	Ref<MeshLibrary::LayerGroup> phaseLayers;

	friend class SynthesizerVoice;
};