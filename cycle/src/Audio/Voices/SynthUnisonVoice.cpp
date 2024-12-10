#include <math.h>
#include <Algo/Oversampler.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Test/CsvFile.h>
#include <Thread/LockTracer.h>
#include <Util/Arithmetic.h>

#include "SynthesizerVoice.h"
#include "SynthState.h"
#include "SynthUnisonVoice.h"
#include "../../Audio/Effects/Unison.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../Curve/EnvRenderContext.h"
#include "../../CycleDefs.h"
#include "../../UI/VertexPanels/DeformerPanel.h"
#include "../../UI/VertexPanels/Waveform3D.h"


SynthUnisonVoice::SynthUnisonVoice(SynthesizerVoice* parent, SingletonRepo* repo) :
		CycleBasedVoice(parent, repo)
{
	noteState.isStereo = true;
}


SynthUnisonVoice::~SynthUnisonVoice()
{
}


void SynthUnisonVoice::testMeshConditions()
{
	bool haveAnyValidTimeLayers = false;
	for(int i = 0; i < timeLayers->size(); ++i)
	{
		MeshLibrary::Layer& layer = timeLayers->layers[i];

		if(layer.props->active && layer.mesh->hasEnoughCubesForCrossSection())
		{
			haveAnyValidTimeLayers = true;
			break;
		}
	}

	if(! haveAnyValidTimeLayers)
		parent->stop(false);
}


void SynthUnisonVoice::initialiseNoteExtra(const int midiNoteNumber, const float velocity)
{
	for(int i = 0; i < parent->scratchGroup.size(); ++i)
	{
		EnvRenderContext& rast = parent->scratchGroup[i];
		rast.scratchTime = rast.sampleable ? rast.rast.sampleAt(0) : 0;
	}

	for(int i = 0; i < noteState.numUnisonVoices; ++i)
	{
		VoiceParameterGroup& group = groups[i];
		float oscPhase = unison->getPhase(i);

		if(oscPhase < 0.f)	oscPhase += 1.f;
		if(oscPhase > 1.f)	oscPhase -= 1.f;

		for(int meshIdx = 0; meshIdx < group.layerStates.size(); ++meshIdx)
		{
			CycleState& state = group.layerStates[meshIdx];
			MeshLibrary::Layer layer = parent->meshLib->getLayer(LayerGroups::GroupTime, meshIdx);
//			Waveform3D::LayerProps& props = surface->getPropertiesForLayer(meshIdx);

			state.reset();

			timeRasterizer.setNoiseSeed(random.nextInt(DeformerPanel::tableSize));

//			modMatrix->route(progress, ModMatrixPanel::VoiceTime, parent->voiceIndex);
			MorphPosition pos = layer.props->pos[parent->voiceIndex];
			pos.time = getScratchTime(layer.props->scratchChan, group.cumePos);

			timeRasterizer.setInterceptPadding(group.angleDelta);
			timeRasterizer.setState(&state);
			timeRasterizer.setMorphPosition(pos);

			if(cycleCompositeAlgo == Interpolate)
			{
				timeRasterizer.setWrapsEnds(true);
				timeRasterizer.calcCrossPoints(layer.mesh, oscPhase);
			}
			else
			{
				// prime rasterizer
				timeRasterizer.setWrapsEnds(true);
				timeRasterizer.setMesh(layer.mesh);
				timeRasterizer.calcCrossPointsChaining(oscPhase);
			}
		}
	}
}


void SynthUnisonVoice::calcCycle(VoiceParameterGroup& group)
{
	float totalPhase;
	float pans[2];

	int currentSize 	 = cycleCompositeAlgo == Chain ? group.samplesThisCycle : noteState.nextPow2;
	jassert(currentSize <= rastBuffer.size());

	int oversampleFactor = oversamplers[0]->getOversampleFactor();
	int samplingSize 	 = oversampleFactor * currentSize;
	noteState.isStereo 	 = unison->isStereo();

	layerAccumBuffer[Left].withSize(samplingSize).zero();
	layerAccumBuffer[Right].withSize(samplingSize).zero();

	int layerSize = timeLayers->size();
	onlyBeat(layerSize = 1);

	for(int meshIdx = 0; meshIdx < layerSize; ++meshIdx)
	{
		MeshLibrary::Layer layer = parent->meshLib->getLayer(LayerGroups::GroupTime, meshIdx);
		CycleState& state = group.layerStates[meshIdx];

		if(! layer.props->active || ! layer.mesh->hasEnoughCubesForCrossSection())
			continue;

		totalPhase = unison->getPhase(group.unisonIndex);
		if(totalPhase < 0.f) totalPhase += 1.f;
		if(totalPhase > 1.f) totalPhase -= 1.f;

		timeRasterizer.setState(&state);

		jassert(! timeRasterizer.doesCalcDepthDimensions());

		double delta, spillover;

		MorphPosition pos = layer.props->pos[parent->voiceIndex];
		pos.time = getScratchTime(layer.props->scratchChan, group.cumePos);

		timeRasterizer.setMorphPosition(pos);
		timeRasterizer.setNoiseSeed(random.nextInt(DeformerPanel::tableSize));

		if(cycleCompositeAlgo == Interpolate)
		{
			delta = 1 / (double) samplingSize;
			spillover = 0;
			state.advancement = 0.f;

			timeRasterizer.setInterceptPadding((float) delta);
			timeRasterizer.calcCrossPoints(layer.mesh, totalPhase);
		}
		else
		{
			delta 		= group.angleDelta / double(oversampleFactor);
			spillover 	= state.spillover; //group.samplingSpillover[0];

			timeRasterizer.setMesh(layer.mesh);
			timeRasterizer.setInterceptPadding(jmax(-spillover, delta));
			timeRasterizer.calcCrossPointsChaining(totalPhase);
		}

		Buffer<float> rastBuf(rastBuffer, samplingSize);
		if(timeRasterizer.isSampleable())
		{
			state.spillover = timeRasterizer.sampleWithInterval(rastBuf, delta, spillover);
		}
		else
		{
			rastBuf.zero();
			double& spillover = state.spillover; //group.samplingSpillover[0];
			spillover += samplingSize * delta;

			if(spillover > 0.5)
				spillover -= 1;
		}

		float totalPan = layer.props->pan;

		if(cycleCompositeAlgo == Chain)
			totalPan += unison->getPan(group.unisonIndex) - 0.5f;

		NumberUtils::constrain(totalPan, 0.f, 1.f);
		Arithmetic::getPans(totalPan, pans[Left], pans[Right]);

		noteState.isStereo |= fabsf(totalPan - 0.5f) > 0.02f;

		for(int c = 0; c < 2; ++c)
		{
			layerAccumBuffer[c].withSize(samplingSize).addProduct(rastBuffer, pans[c]);
		}
	}
}


void SynthUnisonVoice::prepNewVoice()
{
	for(int i = 0; i < (int) groups.size(); ++i)
	{
		VoiceParameterGroup& group = groups[i];

		for(int j = 0; j < (int) group.layerStates.size(); ++j)
		{
			CycleState& state = group.layerStates[j];

			for(int k = 0; k < (int) state.backIcpts.size(); ++k)
			{
				state.backIcpts[k].cube = nullptr;
			}

			state.frontA.cube = nullptr;
			state.frontB.cube = nullptr;
			state.frontC.cube = nullptr;
			state.frontD.cube = nullptr;
			state.frontE.cube = nullptr;
		}
	}

	timeRasterizer.orphanOldVerts();
}
