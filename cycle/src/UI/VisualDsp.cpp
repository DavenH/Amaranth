#include <Algo/AutoModeller.h>
#include <App/AppConstants.h>
#include <App/MeshLibrary.h>
#include <Array/StereoBuffer.h>
#include <Audio/Multisample.h>
#include <Audio/PitchedSample.h>
#include <Curve/EnvelopeMesh.h>
#include <Definitions.h>
#include <Design/Updating/Updater.h>
#include <Thread/LockTracer.h>
#include <Util/Arithmetic.h>
#include <Util/LogRegions.h>
#include <Util/NumberUtils.h>
#include <Util/Util.h>

#include "VisualDsp.h"
#include "Algo/Resampling.h"
#include "../App/Initializer.h"
#include "../Audio/AudioSourceRepo.h"
#include "../Audio/SynthAudioSource.h"
#include "../Curve/ScratchContext.h"
#include "../CycleDefs.h"
#include "../UI/Effects/UnisonUI.h"
#include "../UI/Panels/Morphing/MorphPanel.h"
#include "../UI/Panels/OscControlPanel.h"
#include "../UI/VertexPanels/DeformerPanel.h"
#include "../UI/VertexPanels/Envelope2D.h"
#include "../UI/VertexPanels/Spectrum3D.h"
#include "../UI/VertexPanels/Waveform3D.h"
#include "../UI/Widgets/HSlider.h"
#include "../Util/CycleEnums.h"

VisualDsp::VisualDsp(SingletonRepo* repo) :
		SingletonAccessor(repo, "VisualDsp")
	,	timeProcessor	(this, repo, TimeStage)
	,	envProcessor 	(this, repo, EnvStage)
	,	fftProcessor 	(this, repo, FFTStage)
	,	fxProcessor	 	(this, repo, FXStage)
	,	scratchEnvPanel	(Panel::linestripRes)
	,	random			(Time::currentTimeMillis())
{
	for(int fftOrderIdx = 0; fftOrderIdx < numFFTOrders; ++fftOrderIdx)
	{
		int size = 8 << fftOrderIdx;
		sizeToIndex[size] = fftOrderIdx;
		ffts[fftOrderIdx].setFFTScaleType(IPP_FFT_DIV_FWD_BY_N);
		ffts[fftOrderIdx].allocate(size, true);
//		status(ippsFFTInitAlloc_R_32f(&fftspecs[fftOrderIdx], fftOrderIdx + 3, IPP_FFT_DIV_FWD_BY_N, ippAlgHintFast));
	}

	int largestSize = 8 << (numFFTOrders - 1);

//	fftBuffer 	.resize(largestSize + 2);
//	magnitudes 	.resize(largestSize / 2);
//	phases 		.resize(largestSize / 2);
//
//	fftBuffer.zero();
}


VisualDsp::~VisualDsp()
{
//	for(int fftOrderIdx = 0; fftOrderIdx < numFFTOrders; ++fftOrderIdx)
//		status(ippsFFTFree_R_32f(fftspecs[fftOrderIdx]));
}


void VisualDsp::init()
{
	spectRasterizer = &getObj(SpectRasterizer);
	phaseRasterizer = &getObj(PhaseRasterizer);
	timeRasterizer 	= &getObj(TimeRasterizer);
	spectrum3D 		= &getObj(Spectrum3D);
	surface 		= &getObj(Waveform3D);
	unison 			= &getObj(SynthAudioSource).getUnison();
	meshLib			= &getObj(MeshLibrary);
}


CriticalSection& VisualDsp::getCalculationLock()
{
    return calculationLock;
}


void VisualDsp::rasterizeEnv(Buffer<Ipp32f> env,
							 Buffer<Ipp32f> zoomArray,
							 int layerGroup,
							 EnvRasterizer& rasterizer,
							 bool doRestore)
{
	MeshLibrary::EnvProps* props = meshLib->getCurrentEnvProps(layerGroup);
	Envelope2D& envPanel = getObj(Envelope2D);

	if(props == nullptr)
		return;

	int dim = getSetting(CurrentMorphAxis);

	if(dim == Vertex::Time)
	{
		if(props->active)
		{
			// degrade curve for surface rendering (accuracy not important)
			rasterizer.updateOffsetSeeds();
			rasterizer.setNoiseSeed(random.nextInt(DeformerPanel::tableSize));
			rasterizer.setLowresCurves(layerGroup != ScratchType && layerGroup != ScratchPanelType);
			rasterizer.setCalcDepthDimensions(false);
			rasterizer.setMode(EnvRasterizer::NormalState);
			rasterizer.calcCrossPoints();

			if(rasterizer.isSampleable())
			{
				double delta = 1 / float(env.size());

				float tempoScale = 1.0; // TODO
				rasterizer.resetGraphicParams();
				rasterizer.renderToBuffer(env.size(), delta, EnvRasterizer::graphicIndex, *props, tempoScale);

				jassert(rasterizer.getRenderBuffer().size() == env.size());

				rasterizer.getRenderBuffer().copyTo(env);
				ippsThreshold_LTValGTVal_32f_I(env, env.size(), 0.f, 0.f, 1.f, 1.f);
			}
		}
		else
		{
			if(layerGroup == ScratchType)
			{
				zoomArray.copyTo(env);
			}
			else if(layerGroup == ScratchPanelType)
			{
				env.ramp(0, 1.f / float(Panel::linestripRes));
			}
			else
			{
				env.set((layerGroup == PitchType) ? 0.5f : 1.f);
			}
		}

		// restore curve for viewing
		if(doRestore)
		{
			rasterizer.setMode(EnvRasterizer::NormalState);
			rasterizer.setLowresCurves(false);
			rasterizer.setCalcDepthDimensions(true);
			rasterizer.update(UpdateType::Update);
		}
	}
	else
	{
		float& dimVar 		= rasterizer.getPrimaryDimensionVar();
		float originalPos 	= dimVar;
		float time 			= getObj(MorphPanel).getValue(Vertex::Time);

		if(props->active)
		{
			rasterizer.setLowresCurves(true);
			rasterizer.setCalcDepthDimensions(false);
			rasterizer.setMode(EnvRasterizer::NormalState);

			int index = 0;
			for(int i = 0; i < (int) env.size(); ++i)
			{
				dimVar = zoomArray[i];
				rasterizer.calcCrossPoints();

				env[i] = rasterizer.isSampleableAt(time) ?
						 rasterizer.sampleAt(time, index) : zoomArray[i];
			}

			ippsThreshold_LTValGTVal_32f_I(env, env.size(), 0.f, 0.f, 1.f, 1.f);

			// reset the rasterizer
			dimVar = originalPos;

			if(doRestore)
			{
				rasterizer.setLowresCurves(false);
				rasterizer.setCalcDepthDimensions(true);
				rasterizer.update(UpdateType::Update);
			}
		}
	}
}


void VisualDsp::rasterizeAllEnvs(int numColumns)
{
	if(numColumns < 1)
		return;

	zoomProgress.resize(numColumns);
	zoomProgress.ramp();

	int types[] = { LayerGroups::GroupVolume, LayerGroups::GroupPitch, LayerGroups::GroupScratch };

	for(int i = 0; i < numElementsInArray(types); ++i)
		rasterizeEnv(types[i], numColumns);
}


void VisualDsp::rasterizeEnv(int envEnum, int numColumns)
{
//	dout << "Rasterizing envelope " << envEnum << " with size: " << numColumns << "\n";

	if(envEnum == LayerGroups::GroupWavePitch)
		return;

	ScopedAlloc<Ipp32f>* buff;
	EnvRasterizer* rast;
	EnvType type;

	if(envEnum == LayerGroups::GroupVolume)
	{
		volumeEnv.resize(numColumns);
		rasterizeEnv(volumeEnv, zoomProgress, LayerGroups::GroupVolume, getObj(EnvVolumeRast));
		MeshLibrary::EnvProps* volumeProps = meshLib->getCurrentEnvProps(LayerGroups::GroupVolume);

		if(volumeProps->logarithmic)
			Arithmetic::applyInvLogMapping(volumeEnv, 30);
	}

	else if(envEnum == LayerGroups::GroupPitch)
	{
		rast = &getObj(EnvPitchRast);

		// calculates graphic curve (update() makes a copy)
		rast->setNoteOn();
		rast->updateOffsetSeeds();
		rast->setWantOneSamplePerCycle(false);
		rast->setCalcDepthDimensions(true);
		rast->setDecoupleComponentDfrm(false);
		rast->setLowresCurves(false);
		rast->update(UpdateType::Update);
	}

	else if(envEnum == LayerGroups::GroupScratch)
	{
		rast = &getObj(EnvScratchRast);
		type = ScratchType;

		MeshLibrary& meshLib = getObj(MeshLibrary);
		MeshLibrary::LayerGroup& scratchGroup = meshLib.getGroup(LayerGroups::GroupScratch);

		int numScratchLayers = scratchGroup.size();
		scratchEnv.ensureSize(numScratchLayers * numColumns);
		ScopedLock sl(graphicEnvLock);

		scratchEnvPanel.ensureSize(numScratchLayers * Panel::linestripRes);

		for(int i = 0; i < numScratchLayers; ++i)
		{
			rast->setMesh(dynamic_cast<EnvelopeMesh*>(scratchGroup.layers[i].mesh));
			rasterizeEnv(scratchEnv.section(i * numColumns, numColumns), zoomProgress, LayerGroups::GroupScratch, *rast, false);
		}

		// rasterize the current env last to preserve state
		rast->setMesh(meshLib.getCurrentEnvMesh(LayerGroups::GroupScratch));
		rasterizeEnv(scratchEnv.section(scratchGroup.current * numColumns, numColumns), zoomProgress, LayerGroups::GroupScratch, *rast);

		Resampling::linResample(scratchEnv.withSize(numScratchLayers * numColumns),
								scratchEnvPanel.withSize(numScratchLayers * Panel::linestripRes));

		updateScratchContexts(numColumns);
	}
}


void VisualDsp::copyArrayOrParts(const vector<Column>& srcColumns, vector<Column>& destColumns)
{
	int numDstColumns = destColumns.size();
	int numSrcColumns = srcColumns.size();

	if(numSrcColumns == 0 || numDstColumns == 0)
		return;

	int minLength = jmin(numDstColumns, numSrcColumns);
	int srcColInc = (int) (numSrcColumns / double(minLength) + 0.5);
	int dstColInc = (int) (numDstColumns / double(minLength) + 0.5);

  #ifdef JUCE_DEBUG
	int srcSize = srcColumns.front().size();
	int dstSize = destColumns.front().size();
	jassert(srcSize == dstSize);
  #endif

	for(int i = 0, dstColIdx = 0, srcColIdx = 0;
			i < minLength; ++i, dstColIdx += dstColInc, srcColIdx += srcColInc)
	{
		NumberUtils::constrain(srcColIdx, 0, numSrcColumns - 1);
		NumberUtils::constrain(dstColIdx, 0, numDstColumns - 1);

		const Column& source = srcColumns[srcColIdx];
		Column& dest 		 = destColumns[dstColIdx];

		if(source.size() != dest.size())
		{
			float sizeRatio 	= source.size() / float(dest.size());
			int intRatio 		= int(sizeRatio + 0.5f);
			bool isCloseDivisor = fabsf(sizeRatio - (float) intRatio) < 0.00001f;

			if(isCloseDivisor)
			{
				if(source.size() > dest.size())
				{
					dest.downsampleFrom(source, intRatio);
				}
				else
				{
					dest.upsampleFrom(source, intRatio);
				}
			}
			else
			{
				source.copyTo(dest);
			}
		}
		else
		{
			source.copyTo(dest);
		}
	}
}


void VisualDsp::calcTimeDomain(int numColumns)
{
	checkTimeColumns(numColumns);

	MeshLibrary::LayerGroup& timeGroup = meshLib->getGroup(LayerGroups::GroupTime);
	MorphPanel& morphPanel = getObj(MorphPanel);

	int nextPow2 		= preEnvCols.front().size();
	int memorySize 		= nextPow2 * (timeGroup.size() + 2);
	double delta		= 1.0 / double(nextPow2);

	float modTime		= morphPanel.getValue(Vertex::Time);
	double modPan 		= morphPanel.getPanSlider()->getValue();

	int stage			= getSetting(ViewStage);
	int reductionFactor = getSetting(ReductionFactor);
	int primeDim		= getSetting(CurrentMorphAxis);

	ScopedAlloc<Ipp32f> memory(memorySize);

	Buffer<float> timeBuffer = memory.place(timeGroup.size() * nextPow2);
	Buffer<float> sumBuffer  = memory.place(nextPow2);
	Buffer<float> timeRamp 	 = memory.place(nextPow2);
	timeRamp.ramp();

	int timeInc	= timeProcessor.isDetailReduced() ? reductionFactor : 1;

	MeshRasterizer::RenderState timeState;
	MeshRasterizer::ScopedRenderState scopedState(timeRasterizer, &timeState);

	MeshRasterizer::RenderState batchState(true, true, false, MeshRasterizer::HalfBipolar, timeState.pos);
	batchState.pos.time = 0;

	timeRasterizer->restoreStateFrom(batchState);
	timeRasterizer->updateOffsetSeeds();

	MeshRasterizer& rasterizer = *timeRasterizer;

	int numActiveLayers = surface->getNumActiveLayers();

	if(numActiveLayers == 0)
	{
		ScopedLock sl(timeColumnLock);
		preEnvCols.clear();
		return;
	}

	int timeColIdx = 0;
	for(int colIdx = 0; colIdx < numColumns; ++colIdx)
	{
		timeColIdx = jmin(zoomProgress.size() - 1, colIdx * timeInc);

		Column& column = preEnvCols[colIdx];

		if(primeDim == Vertex::Red)
		{
			nextPow2 	= column.size();
			delta 		= 1.0 / double(nextPow2);
		}

		timeRasterizer->getPrimaryDimensionVar() = zoomProgress[timeColIdx];

		if(numActiveLayers > 1)
			sumBuffer.zero();

		for(int i = 0; i < (int) timeGroup.size(); ++i)
		{
			Buffer<float> localBuffer 	= numActiveLayers == 1 ? sumBuffer.withSize(nextPow2) :
																 Buffer<float>(timeBuffer + i * nextPow2, nextPow2);

			MeshLibrary::Layer& layer = timeGroup.layers[i];
			Mesh* mesh = layer.mesh;
			MeshLibrary::Properties* props = layer.props;

			if(! props->active || ! mesh->hasEnoughCubesForCrossSection())
				continue;

			float scratchTime =
					primeDim != Vertex::Time ? modTime :
					props->scratchChan == CommonEnums::Null || stage < ViewStages::PostEnvelopes ?
					zoomProgress[timeColIdx] : scratchContexts[props->scratchChan].gridBuffer[timeColIdx];

			rasterizer.setNoiseSeed(colIdx * 6197);
			rasterizer.setYellow(scratchTime);
			rasterizer.calcCrossPoints(mesh);

			if(! rasterizer.isSampleable())
			{
				localBuffer.zero();
				continue;
			}

			rasterizer.sampleWithInterval(localBuffer, delta, 0.0);

			float relativePan = Arithmetic::getRelativePan(props->pan, modPan);

			localBuffer.mul(relativePan);

			if(numActiveLayers > 1)
				sumBuffer.add(localBuffer);
		}

		sumBuffer.copyTo(column);
	}
}


void VisualDsp::calcSpectrogram(int numColumns)
{
	checkFFTColumns(numColumns);

	static const float invSqrtHalf = 1 / sqrtf(0.5f);

	int stage = getSetting(ViewStage);
	int reductionFactor = getSetting(ReductionFactor);

	vector<Column>& timeColumns = stage == ViewStages::PreProcessing ? preEnvCols : postEnvCols;
	MorphPanel& morphPanel 		= getObj(MorphPanel);

	int nextPow2, numHarmonics;

	int numPixels 				= surface->getWindowWidthPixels();
	int fftProcInc				= fftProcessor.isDetailReduced() ? reductionFactor : 1;
	int spectInc				= spectRasterizer->isDetailReduced() ? reductionFactor : 1;
	int phaseInc				= phaseRasterizer->isDetailReduced() ? reductionFactor : 1;
	int magSize 				= numPixels / spectInc;
	int phaseSize 				= numPixels / phaseInc;

	MeshLibrary& meshLib 		= getObj(MeshLibrary);

	int numTimeColumns 			= timeColumns.size();
	int timeColInc 				= (int) (numTimeColumns / double(numColumns) + 0.5);
	int colMagRatio				= (int) jmax(1., numColumns / double(magSize));
	int colPhaseRatio			= (int) jmax(1., numColumns / double(phaseSize));
	int primeDim				= getSetting(CurrentMorphAxis);
	int midiKey 				= primeDim == Vertex::Red ? getConstant(LowestMidiNote) : morphPanel.getCurrentMidiKey();

	getNumHarmonicsAndNextPower(numHarmonics, nextPow2, midiKey);

	int	halfPow2		 		= nextPow2 / 2;
	int sizeIndex				= sizeToIndex[nextPow2];
	float modTime				= morphPanel.getValue(Vertex::Time);
	double modPan 				= morphPanel.getPanSlider()->getValue();
	float additiveScale 		= Arithmetic::calcAdditiveScaling(numHarmonics);

	Transform& fft				= ffts[sizeIndex];
	Buffer<Ipp32f> fftRamp 		= getObj(LogRegions).getRegion(midiKey);

	MeshLibrary::LayerGroup& magnGroup = meshLib.getGroup(LayerGroups::GroupSpect);
	MeshLibrary::LayerGroup& phaseGroup = meshLib.getGroup(LayerGroups::GroupPhase);

	bool doForwardFFT			= getSetting(TimeEnabled) && surface->getNumActiveLayers() > 0;
	bool isFilterEnabled 		= getSetting(FilterEnabled);
	bool isPhaseEnabled  		= getSetting(PhaseEnabled);
	bool doInverseFFT 			= stage >= ViewStages::PostSpectrum && ! timeColumns.empty();

//	onlyBeat(isPhaseEnabled = false);
//	jassert(fftBuffer.size() >= nextPow2 + 2);
	jassert(volumeEnv.size() >= numColumns);

	Ipp64u end;
	int memorySize = numHarmonics * int(2 + magnGroup.size() + phaseGroup.size()) + numColumns;
	ScopedAlloc<Ipp32f> memory(memorySize);

	Buffer<float> localMaxima 	 = memory.place(numColumns);
	Buffer<float> workBuffer 	 = memory.place(numHarmonics);
	Buffer<float> phaseScaleRamp = memory.place(numHarmonics);
	Buffer<float> freqBuffer	 = memory.place(magnGroup.size() * numHarmonics);
	Buffer<float> phaseBuffer	 = memory.place(phaseGroup.size() * numHarmonics);

	phaseScaleRamp.ramp(1.f, 1.f).sqrt();

	MeshRasterizer::RenderState freqState, phaseState;
	MeshRasterizer::ScopedRenderState freqStateScoped(spectRasterizer, &freqState);
	MeshRasterizer::ScopedRenderState phaseStateScoped(phaseRasterizer, &phaseState);
	MeshRasterizer::RenderState batchState(true, true, false, MeshRasterizer::Bipolar, freqState.pos);

	batchState.pos.time = 0;
	phaseRasterizer->restoreStateFrom(batchState);

	batchState.scalingType = MeshRasterizer::Unipolar;
	spectRasterizer->restoreStateFrom(batchState);
	phaseRasterizer->updateOffsetSeeds();
	spectRasterizer->updateOffsetSeeds();

//	Buffer<float> fftBuf(fftBuffer, nextPow2);

	Buffer<float> magBuf(fft.getMagnitudes(), numHarmonics);
	Buffer<float> phaseBuf(fft.getPhases(), numHarmonics);

	for (int colIdx = 0, timeColIdx = 0; colIdx < numColumns; ++colIdx, timeColIdx += timeColInc)
	{
		if(primeDim == Vertex::Red)
		{
			int lastNumHarmonics = numHarmonics;
			int lastPow2		= nextPow2;

			midiKey 		= fftPreFXCols[colIdx].midiKey;
			numHarmonics 	= fftPreFXCols[colIdx].size();
			fftRamp 		= getObj(LogRegions).getRegion(midiKey);
			nextPow2 		= timeColumns.empty() ? NumberUtils::nextPower2(numHarmonics * 2) : timeColumns[timeColIdx].size();
			sizeIndex 		= sizeToIndex[nextPow2];
			halfPow2		= nextPow2 / 2;

			if(lastNumHarmonics != numHarmonics)
			{
				additiveScale 	= Arithmetic::calcAdditiveScaling(numHarmonics);
				magBuf 			= ffts[sizeIndex].getMagnitudes().withSize(numHarmonics);
				phaseBuf 		= ffts[sizeIndex].getPhases().withSize(numHarmonics);

				workBuffer		= workBuffer	.withSize(numHarmonics);
				freqBuffer		= freqBuffer	.withSize(magnGroup.size() * numHarmonics);
				phaseBuffer		= phaseBuffer	.withSize(phaseGroup.size() * numHarmonics);
				phaseScaleRamp	= phaseScaleRamp.withSize(numHarmonics);
			}

//			if(lastPow2 != nextPow2)
//			{
//				fftBuf = fftBuffer.withSize(nextPow2);
//			}
		}

		if(timeColIdx >= numTimeColumns)
			timeColIdx = numTimeColumns - 1;

		if(doForwardFFT)
		{
//			timeColumns[timeColIdx].copyTo(fftBuf);
//			fftBuf.mul(2.f);

			ffts[sizeIndex].forward(timeColumns[timeColIdx]);
//			ippsFFTFwd_RToCCS_32f_I(fftBuf, fftspecs[sizeIndex], 0);
//			ippsCartToPolar_32fc((Ipp32fc*)fftBuf.get() + 1, magBuf, phaseBuf, numHarmonics);

			phaseBuf.add((float) IPP_PI2);
		}
		else
		{
			magBuf.zero();
			phaseBuf.zero();
		}

		if (isFilterEnabled)
		{
			int fftIdx = jmin(zoomProgress.size() - 1, colIdx * fftProcInc);

			MeshRasterizer& rasterizer = *spectRasterizer;
			spectRasterizer->getPrimaryDimensionVar() = zoomProgress[fftIdx];

			for(int i = 0; i < (int) magnGroup.size(); ++i)
			{
				Buffer<float> localBuffer(freqBuffer + i * numHarmonics, numHarmonics);
				MeshLibrary::Layer& spectLayer = magnGroup[i];

				if(! spectLayer.props->active || ! spectLayer.mesh->hasEnoughCubesForCrossSection())
					continue;

				float scratchTime = primeDim != Vertex::Time ? modTime :
									spectLayer.props->scratchChan == CommonEnums::Null || stage < ViewStages::PostEnvelopes ?
									zoomProgress[colIdx * fftProcInc] :
									scratchContexts[spectLayer.props->scratchChan].gridBuffer[fftIdx];

				float relativePan = Arithmetic::getRelativePan(spectLayer.props->pan, modPan);

				if(relativePan == 0.f)
					continue;

				if(colIdx % colMagRatio == 0)
				{
					rasterizer.setNoiseSeed(colIdx * 1997);
					rasterizer.setYellow(scratchTime);
					rasterizer.calcCrossPoints(spectLayer.mesh);

					if(! rasterizer.isSampleable())
					{
						localBuffer.zero();
						continue;
					}

					rasterizer.sampleAtIntervals(fftRamp, localBuffer);

					float dynamicRange = Spectrum3D::calcDynamicRangeScale(spectLayer.props->range);
					float multiplicand = relativePan;
					ippsSqrt_32f_A11(&dynamicRange, &dynamicRange, 1);

					float thresh = powf(1e-19f, 1.f / dynamicRange);
					localBuffer.threshLT(thresh).pow(dynamicRange);

					multiplicand *= powf(2.f, dynamicRange);

					if(spectLayer.props->mode == Spectrum3D::Additive)
						multiplicand *= additiveScale * volumeEnv[colIdx * fftProcInc];

					localBuffer.mul(multiplicand);
				}

				// need to specify size because with varying-size columns and
				// sparsity mismatches, localbuffer can be larger than magbuf
				Buffer<float> localBuffer2(localBuffer, numHarmonics);

				if(spectLayer.props->mode == Spectrum3D::Additive)
					magBuf.add(localBuffer2);
				else
				{
					// todo relative panning oddity here with magnitudes
					if(relativePan < 1.f)
						localBuffer2.add(-1.f).mul(relativePan).add(1.f);

					magBuf.mul(localBuffer2);
				}
			}
		}

		if (isPhaseEnabled)
		{
			int fftIdx = jmin(zoomProgress.size() - 1, colIdx * fftProcInc);

			phaseRasterizer->getPrimaryDimensionVar() = zoomProgress[fftIdx];

			MeshRasterizer& rasterizer = *phaseRasterizer;
			workBuffer.zero();

			for(int i = 0; i < (int) phaseGroup.size(); ++i)
			{
				Buffer<float> localBuffer(phaseBuffer + i * numHarmonics, numHarmonics);

				MeshLibrary::Layer& layer = phaseGroup[i];

				if(! layer.props->active || ! layer.mesh->hasEnoughCubesForCrossSection())
					continue;

				float scratchTime = primeDim != Vertex::Time ? modTime :
									layer.props->scratchChan == CommonEnums::Null || stage < ViewStages::PostEnvelopes ?
									zoomProgress[colIdx * fftProcInc] :
									scratchContexts[layer.props->scratchChan].gridBuffer[fftIdx];

				if(colIdx % colPhaseRatio == 0)
				{
					rasterizer.setNoiseSeed(colIdx * 671);
					rasterizer.setYellow(scratchTime);
					rasterizer.calcCrossPoints(layer.mesh);

					if(rasterizer.isSampleable())
					{
						rasterizer.sampleAtIntervals(fftRamp, localBuffer);

						double relativePan 	= Arithmetic::getRelativePan(layer.props->pan, modPan);
						float phaseAmpScale = Spectrum3D::calcPhaseOffsetScale(layer.props->range);
						float multiplicand 	= relativePan * phaseAmpScale * IPP_2PI;

						localBuffer.mul(multiplicand);
					}
					else
					{
						localBuffer.zero();
					}
				}

				workBuffer.add(localBuffer);
			}

			workBuffer.mul(phaseScaleRamp);
			phaseBuf.add(workBuffer);
		}

		// remove dc offset

		Buffer<float> fftCol = fftPreFXCols[colIdx];

		// todo do we need to store this data if stage >= postfx ? ? ?
		magBuf.copyTo(fftCol);
		fftCol.mul(2.f);
		Arithmetic::applyLogMapping(fftCol, getConstant(LogTension));

		fftCol.threshGT(1.f);

		phaseBuf.copyTo(phasePreFXCols[colIdx]);

		if(doInverseFFT)
		{
			phaseBuf.add((float) -IPP_PI2);
			ffts[sizeIndex].inverse(timeColumns[timeColIdx]);

//			ippsZero_32fc((Ipp32fc*)fftBuffer.get() + numHarmonics + 1, nextPow2 / 2 - numHarmonics);
//			status(ippsPolarToCart_32fc(magBuf, phases, (Ipp32fc*)fftBuffer.get() + 1, numHarmonics));
//			status(ippsFFTInv_CCSToR_32f_I(fftBuffer, fftspecs[sizeIndex], 0));
//			fftBuffer.copyTo(timeColumns[timeColIdx]);

			jassert(timeColumns[timeColIdx].front() == timeColumns[timeColIdx].front());
		}
	}

	bool processUnison = stage >= ViewStages::PostFX && unison->getOrder(false) > 1;

	processFrequency(timeColumns, processUnison);

	end = ippGetCpuClocks();

//  #ifndef BEAT_EDITION
	// phases will be unwrapped if processed through fx
	if(getSetting(ViewStage) < ViewStages::PostFX)
	{
		if(primeDim == Vertex::Time)
			unwrapPhaseColumns(phasePreFXCols);
		else
		{
			phasePreFXArray.mul((float) IPP_RPI * 0.5f).add(0.5f);
		}
	}
//  #endif
}


void VisualDsp::updateScratchContexts(int numColumns)
{
	ScopedLock sl(graphicEnvLock);
	scratchContexts.clear();

	int numScratchLayers = meshLib->getGroup(LayerGroups::GroupScratch).size();

	for(int i = 0; i < numScratchLayers; ++i)
	{
		ScratchContext context;

		context.gridBuffer 	= scratchEnv	 .section(i * numColumns, numColumns);
		context.panelBuffer = scratchEnvPanel.section(i * Panel::linestripRes, Panel::linestripRes);
		Buffer<float>& buf 	= context.gridBuffer;

		if(buf.size() < 2)
			continue;

		bool isAscending 	= buf[0] < buf[1];
		bool wasAscending 	= isAscending;

		ScratchInflection first;
		first.startIndex = 0;
		context.inflections.push_back(first);

		for(int j = 0; j < buf.size() - 1; ++j)
		{
			isAscending = buf[j + 1] > buf[j];

			if(Util::assignAndWereDifferent(wasAscending, isAscending))
			{
				ScratchInflection inflection;
				inflection.startIndex = j;

				context.inflections.push_back(inflection);
			}
		}

		if(context.inflections.back().startIndex < buf.size() - 1)
		{
			ScratchInflection last;
			last.startIndex = buf.size() - 1;
			context.inflections.push_back(last);
		}

		for(int j = 0; j < (int) context.inflections.size() - 1; ++j)
		{
			ScratchInflection& curr = context.inflections[j];

			int idxA = curr.startIndex;
			int idxB = context.inflections[j + 1].startIndex;

			curr.range = Range<float>(buf[idxA], buf[idxB]);
		}

		scratchContexts.push_back(context);
	}
}


void VisualDsp::calcWaveSpectrogram(int numColumns)
{
	SampleWrapper* wav = getObj(Multisample).getCurrentSample();

	if (wav == nullptr || wav->periods.empty())
		return;

	int numHarmonics, nextPow2;
	int wavLength 		= wav->size();
	int midiNote 		= wav->fundNote;
	bool wrapCycles		= getSetting(WrapWaveCycles) == 1;
	bool interpCycles	= getSetting(InterpWaveCycles) == 1;

	getNumHarmonicsAndNextPower(numHarmonics, nextPow2, midiNote);

	int quarter = nextPow2 / 4;
	ScopedAlloc<Ipp32f> fadeInMem(nextPow2);
	Buffer<float> fadeInUp 		= fadeInMem.place(quarter);
	Buffer<float> fadeInDown 	= fadeInMem.place(quarter);
	Buffer<float> fadeOutUp 	= fadeInMem.place(quarter);
	Buffer<float> fadeOutDown 	= fadeInMem.place(quarter);

	ScopedAlloc<float> waveMemory(nextPow2 * 5 + 3 * quarter + wavLength);
	Buffer<float> waveRamp 		= waveMemory.place(nextPow2);
	Buffer<float> paddedCyc 	= waveMemory.place(nextPow2);
	Buffer<float> resampledNext = waveMemory.place(nextPow2 + quarter);
	Buffer<float> resampledPrev = waveMemory.place(nextPow2 + quarter);
	Buffer<float> wavCombined 	= waveMemory.place(wavLength);
	Buffer<float> veryFirstCycle= waveMemory.place(nextPow2);
	Buffer<float> quarterBuf	= waveMemory.place(quarter);


	double modPan = getObj(MorphPanel).getPanSlider()->getValue();
	float pans[2];

	if(wav->audio.numChannels == 1)
		pans[0] = 1;
	else
	{
		pans[0]	= (1 - modPan);
		pans[1]	= modPan;
	}

	wavCombined.mul(wav->audio.left, pans[0]);

	if(wav->audio.numChannels > 1)
		wavCombined.addProduct(wav->audio.right, pans[1]);

	fadeOutUp	.ramp().sqr();
	fadeOutDown	.subCRev(1.f, fadeOutUp);
	fadeInUp	.flip(fadeOutDown);
	fadeInDown	.subCRev(1.f, fadeInUp);

	jassert(getSetting(ViewStage) >= ViewStages::PostEnvelopes);

	checkEnvWavColumns(numColumns, nextPow2, 	 midiNote);
	checkFFTWavColumns(numColumns, numHarmonics, midiNote);

	if (wav->periods.size() < 3)
		return;

	int longestSample	= getObj(Multisample).getGreatestLengthSeconds() * wav->samplerate;
	float xInc 			= 1 / float(numColumns - 1);
	float invWavLength 	= 1 / (float) longestSample;
	float prevPosX 		= 0.f;
	float position		= 0.f;

	int nextCycle 		= 1;
	int nextOffset 		= 0;
	int prevOffset 		= 0;
	int prevLength 		= 0;
	int nextLength 		= 0;
	int oldCycle 		= 0;

	int offsetSamples	= wav->phaseOffset * wav->getAveragePeriod();

	vector<PitchFrame>& periods = wav->periods;

	for(int i = 0; i < numColumns; ++i)
	{
		position = i * xInc;

		while(nextCycle < (int) periods.size() - 1 && (periods[nextCycle].sampleOffset - offsetSamples) * invWavLength < position)
			++nextCycle;

		PitchFrame& nextFrame = periods[nextCycle];

		if(nextCycle < periods.size() && nextFrame.period < 10)
		{
			postEnvCols[i] 	 .zero();
			fftPreFXCols[i]	 .zero();
			phasePreFXCols[i].zero();

			continue;
		}

		if(oldCycle != nextCycle)
		{
			int prevCycle = jmax(0, nextCycle - 1);
			PitchFrame& prevFrame = periods[prevCycle];

			prevLength 	= roundToInt(prevFrame.period * 5.f / 4.f);
			nextLength 	= roundToInt(nextFrame.period * 5.f / 4.f);
			prevOffset 	= jmax(0, (int) prevFrame.sampleOffset - offsetSamples);
			nextOffset 	= jmax(0, (int) nextFrame.sampleOffset - offsetSamples);

			prevPosX	= jmin(1.f, prevOffset * invWavLength);

			if(oldCycle == 0)
			{
				if(nextOffset + nextLength < wavLength)
				{
					Buffer<float> wavPrev(wavCombined + prevOffset, prevLength);
					Buffer<float> wavNext(wavCombined + nextOffset, nextLength);

					Resampling::linResample(wavPrev, resampledPrev);
					Resampling::linResample(wavNext, resampledNext);

					// copy first cycle to preserve attack
					resampledPrev.copyTo(veryFirstCycle);

					// wrap quarter at end to front of this cycle to eliminate click
					if(wrapCycles)
					{
						resampledPrev.mul(fadeInUp);
						quarterBuf.mul(resampledPrev + nextPow2, fadeInDown);
						resampledPrev.add(quarterBuf);
					}
				}
				else
				{
					resampledPrev.zero(nextPow2);
					resampledNext.zero(nextPow2);
				}
			}
			else
			{
				resampledNext.copyTo(resampledPrev.withSize(nextPow2));
			}

			Buffer<float> wavSection = wavCombined.sectionAtMost(nextOffset, nextLength);

			if(! wavSection.empty())
			{
				Buffer<float> source = wavSection;
				if(wavSection.size() < nextLength)
				{
					paddedCyc.zero();
					wavSection.copyTo(paddedCyc);
					source = paddedCyc;
				}

				// resample to 512 + qtr size
				Resampling::linResample(source, resampledNext);

				// wrap quarter at end to front of this cycle to eliminate click
				if(wrapCycles)
				{
					resampledNext.mul(fadeInUp);
					quarterBuf.mul(resampledNext + nextPow2, fadeInDown);
					resampledNext.add(quarterBuf);
				}
			}
		}

		oldCycle = nextCycle;

		float diffX 	= position - prevPosX;
		float diffCol 	= jmin(1.f, nextOffset * invWavLength) - prevPosX;
		float portion 	= jmin(1.f, diffX / diffCol);

//		Ipp32f* timePtr = postEnvCols[i];
		Buffer<float> col = postEnvCols[i];

		if(nextOffset + nextLength >= wavLength)
		{
			col.zero();
//			resampledPrev.copyTo(col);
		}
		else
		{
			// attack-preserved first cycle
			if(i == 0)
			{
				veryFirstCycle.copyTo(col);
			}
			else
			{
				if(interpCycles)
				{
					if(portion == 0.f)
						resampledPrev.copyTo(col);
					else
						col.mul(resampledPrev, 1 - portion).addProduct(resampledNext, portion);
				}
				else
				{
					if(portion < 0.9)
						resampledPrev.copyTo(col);
					else
						resampledNext.copyTo(col);
				}
			}
		}
	}

	int sizeIndex = sizeToIndex[nextPow2];
	Transform& fft = ffts[nextPow2];

	ScopedAlloc<Ipp32f> magBuffer(nextPow2);

	for(int i = 0; i < numColumns; ++i)
	{
		Buffer<float> col = postEnvCols[i];

		Column prefxMag(fftPreFXCols[i]);
		Column prefxPhs(phasePreFXCols[i]);

//		col.copyTo(magBuffer);
//		col.mul(0.5f);

//		ippsFFTFwd_RToCCS_32f_I(fftBuffer, fftspecs[sizeIndex], 0);
//		ippsCartToPolar_32fc((Ipp32fc*)fftBuffer.get() + 1, magBuf, phaseBuf, numHarmonics);

		ffts[sizeIndex].forward(col);
		Buffer<float> magBuf(fft.getMagnitudes(), numHarmonics);
		Buffer<float> phaseBuf(fft.getPhases(), numHarmonics);

		Arithmetic::applyLogMapping(fft.getMagnitudes(), getConstant(FFTLogTensionAmp));

		magBuf.threshGT(1.f);
		phaseBuf.add((float) IPP_PI2);

		magBuf.copyTo(prefxMag);
		prefxMag.offset(numHarmonics - 1).zero();

		phaseBuf.copyTo(prefxPhs);
		prefxPhs.offset(numHarmonics - 1).zero();
	}

	if(getSetting(CurrentMorphAxis) == Vertex::Time)
		unwrapPhaseColumns(phasePreFXCols);
	else
		phasePreFXArray.mul((float) IPP_RPI * 0.5f).add(0.5f);

}


void VisualDsp::unwrapPhaseColumns(vector<Column>& phaseColumns)
{
	if(getSetting(MagnitudeDrawMode)
			&& (fftProcessor.isDetailReduced() || fxProcessor.isDetailReduced()))
	{
		return;
	}

	int numHarmonics = phaseColumns.front().size();
	ScopedAlloc<Ipp32f> memory(phaseColumns.size() + 2 * numHarmonics);

	Buffer<Ipp32f> unwrapped = memory.place(phaseColumns.size());
	Buffer<Ipp32f> maxima 	 = memory.place(numHarmonics);
	Buffer<Ipp32f> minima	 = memory.place(numHarmonics);

	const float invConst = IPP_RPI * 0.5f;

	float phaseMin, phaseMax;

	for (int harmIdx = 0; harmIdx < numHarmonics; ++harmIdx)
	{
		for (int col = 0; col < unwrapped.size(); ++col)
			unwrapped[col] = phaseColumns[col][harmIdx];

		unwrapped.minmax(phaseMin, phaseMax);

		float diff;
		for (int col = 1; col < unwrapped.size(); ++col)
		{
			diff = unwrapped[col] - unwrapped[col - 1];
			diff *= invConst;

			if(diff > 0.50001f)
				unwrapped[col] -= IPP_2PI * int(diff + 0.499989999f);

			if(diff < -0.50001f)
				unwrapped[col] += IPP_2PI * int(-diff + 0.499989999f);
		}

		float scaling = 1 / sqrtf(jmax(0.f, float(harmIdx + 1)));
		unwrapped.mul(scaling);

		for (int col = 0; col < unwrapped.size(); ++col)
			phaseColumns[col][harmIdx] = unwrapped[col];

		unwrapped.minmax(minima[harmIdx], maxima[harmIdx]);
	}

	float maximum 		= maxima.max();
	float minimum 		= minima.min();
	float realMax 		= jmax(fabsf(maximum), fabsf(minimum));

	int& scaleFactor  	= spectrum3D->getScaleFactor();
	scaleFactor 		= ceilf(logf(realMax) / logf(2.f) + 0.5f);
	float invPhaseScale = powf(2, -scaleFactor);

	for (int i = 0; i < (int) phaseColumns.size(); ++i)
	{
		Buffer<float> buf = phaseColumns[i];
		buf.mul(invPhaseScale).add(0.5f);
	}
}


void VisualDsp::processThroughEnvelopes(int numColumns)
{
	checkEnvelopeColumns(numColumns);

	// do not clear postEnvCols becase it's used as dest time array in
	// spectrogram calculation
	if(preEnvCols.empty() || postEnvCols.empty())
		return;

	int stage = getSetting(ViewStage);
	ScopedAlloc<Ipp32f> phaseMoveMem(postEnvCols.front().size());
	Buffer<Ipp32f> phaseMoveBuffer(phaseMoveMem);

	// volume changes appear as if it occurred after all processing
	bool areBeforeFX = stage < ViewStages::PostFX;

	// we want to include the fft stuff in the unison if it exists
//	bool haveUnison = stage >= ViewStages::PostFX && unison->getOrder(Unison::graphicParamIndex) > 1
//			&& ! (getSetting(FilterEnabled) || getSetting(PhaseEnabled));

	int reductionFactor = getSetting(ReductionFactor);
	int envInc = envProcessor.isDetailReduced() ? reductionFactor : 1;

	// volume needs to appear to be applied at the end of the signal chain
	if(meshLib->getCurrentEnvProps(LayerGroups::GroupVolume)->active)
	{
		// sample the attack part of the volume envelope with intra-column resolution
//		int intraRastEnd = Arithmetic::binarySearch(getConstant(intraColumnEnvAttackThresh), zoomProgress);

		// FIXME
//		if(getObj(EnvVolumeRast).getMode() != EnvRasterizer::ReleaseMode)
//		{
//			float volumeScale = 1.f; //getObj(OscControlPanel).getValue(OscControlPanel::VolumeSlider);
//			if(intraRastEnd > 0)
//			{
//				ScopedAlloc<Ipp32f> envBuffer(nextPow2);
//				float delta = (zoomProgress[1] - zoomProgress[0]) / float(nextPow2);
//
//				for(int i = 0; i < intraRastEnd; ++i)
//				{
//					jassert(postEnvCols[i].size() == nextPow2);
//					getObj(EnvVolumeRast).sampleWithInterval(delta, envBuffer,
//																	envBuffer.size(), zoomProgress[0]);
//
//					ippsMulC_32f(envBuffer, volumeScale, postEnvCols[i], nextPow2);
//				}
//			}
//
//			for(int colIdx = intraRastEnd; colIdx < numColumns; ++colIdx)
//				ippsMulC_32f_I(volumeEnv[colIdx], postEnvCols[colIdx], postEnvCols[colIdx].size());
//		}
//		else
		{
			float volumeScale = areBeforeFX ? getObj(OscControlPanel).getVolumeScale() : 1;
			int start = 0;

			EnvRasterizer& volRast = getObj(EnvVolumeRast);

			if(volRast.isSampleable() && getSetting(CurrentMorphAxis) == Vertex::Time)
			{
				int size = postEnvCols[0].size();
				double delta = 1 / (double) volumeEnv.size() / double(size);
				volRast.sampleWithInterval(phaseMoveBuffer, delta, 0.);

				phaseMoveBuffer.mul(volumeScale);
				postEnvCols[0].mul(phaseMoveBuffer);
				start = 1;
			}

			for(int colIdx = start; colIdx < numColumns; ++colIdx)
			{
				int idx = envInc * colIdx;
				float envScale = idx >= volumeEnv.size() ? volumeEnv.back() : volumeEnv[idx];

				postEnvCols[colIdx].mul(volumeScale * envScale);
			}
		}
	}
	else
	{
		float volumeScale = areBeforeFX ? getObj(OscControlPanel).getVolumeScale() : 1;

		for(int colIdx = 0; colIdx < numColumns; ++colIdx)
			postEnvCols[colIdx].mul(volumeScale);
	}

	if(stage == ViewStages::PostEnvelopes)
		processFrequency(postEnvCols, false);

//	processOscPitch(postEnvCols);
}


float VisualDsp::getVoiceFrequencyCents(int unisonIndex)
{
	// possibly updated by parameter smoothing
	double pitchEnvVal 	= 0.5;

	if(meshLib->getCurrentProps(LayerGroups::GroupPitch))
	{
		float y = getObj(EnvPitchRast).getSustainLevel(EnvRasterizer::headUnisonIndex + unisonIndex);

		NumberUtils::constrain(y, 0.01f, 0.99f);
		pitchEnvVal = y;
	}

	return NumberUtils::unitPitchToSemis(pitchEnvVal) * 100;
}


void VisualDsp::processFrequency(vector<Column>& columns, bool processUnison)
{
	if(columns.size() < 2)
		return;

	static const float invSqrtHalf = 1 / sqrtf(0.5f);

	int columnSize = 8 << (numFFTOrders - 1);

	ScopedAlloc<Ipp32f> memBuf(columnSize * 3);
	Buffer<float> columnBuf, phaseMoveBuffer, phaseMoveBuffer2;

	processUnison &= unison->isEnabled();

	float xVal;
	float lengthSecs 	 = getObj(OscControlPanel).getLengthInSeconds();
	float modPan 	 	 = getObj(MorphPanel).getPanSlider()->getValue();
	float unitKey 	 	 = getObj(MorphPanel).getValue(Vertex::Red);

	bool isTimeDimension = getSetting(CurrentMorphAxis) == Vertex::Time;
	bool interpolate 	 = ! timeRasterizer->isDetailReduced();
	float time 			 = getObj(MorphPanel).getValue(Vertex::Time);
	float unisonScale 	 = powf(2.f, -(unison->getOrder(false) - 1) * 0.14f);
	int unisonOrder 	 = processUnison ? unison->getOrder(false) : 1;

	vector<MeshRasterizer::DeformContext> contexts(unisonOrder);

	ScopedAlloc<double> cumePhases(unisonOrder);
	cumePhases.zero();

	EnvRasterizer& pitch = getObj(EnvPitchRast);

	// calculates curve for deferred sampleAt calls in processing
	// do this second to preserve deform contexts.
	pitch.updateOffsetSeeds();
	pitch.ensureParamSize(unisonOrder);
	pitch.setCalcDepthDimensions(false);
	pitch.setWantOneSamplePerCycle(true);
	pitch.setMode(EnvRasterizer::NormalState);
	pitch.setLowresCurves(true);
	pitch.calcCrossPoints();
	pitch.setNoteOn();

	Range<int> midiRange(getConstant(LowestMidiNote), getConstant(HighestMidiNote));

	// pitch envelope scales with time
	float timePerColEnv	= 1.f / float(columns.size() - 1);
	float timePerColUni	= timePerColEnv * lengthSecs;

	int samplesPerCol	= int(44100.f * timePerColEnv + 0.5);
	double unitPortionPerSample= timePerColEnv / (float) samplesPerCol;

	for(int colIdx = 0; colIdx < columns.size(); ++colIdx)
	{
		Column& col = columns[colIdx];

		columnSize 	= col.size();
		unitKey 	= Arithmetic::getUnitValueForGraphicNote(col.midiKey, midiRange);
		xVal 		= isTimeDimension ? col.x : time;

		memBuf.resetPlacement();
		columnBuf 			= memBuf.place(columnSize);
		phaseMoveBuffer 	= memBuf.place(columnSize);
		phaseMoveBuffer2 	= memBuf.place(columnSize);

		jassert(! (columnSize & (columnSize - 1)));	// gotta be a power of 2

		col.copyTo(columnBuf);
		col.zero();

		for(int i = 0; i < unisonOrder; ++i)
		{
			MeshLibrary::EnvProps* pitchProps = meshLib->getCurrentEnvProps(LayerGroups::GroupPitch);

			if(pitchProps == nullptr)
				continue;

			if(pitchProps->active)
				getObj(EnvPitchRast).renderToBuffer(samplesPerCol, unitPortionPerSample,
				                                    EnvRasterizer::headUnisonIndex + i, *pitchProps, 1.f);

			float relativePan = 1.f, unisonCents = 0, uniPhase = 0;

			if(processUnison)
			{
				relativePan 	= Arithmetic::getRelativePan(unison->getPan(i, false), modPan) * unisonScale;
				unisonCents		= unison->getDetune(i, false);
				uniPhase 		= unison->getPhase(i, false);
			}

			float envCents		= getVoiceFrequencyCents(i);
			float envHzAbove 	= Arithmetic::centsToFrequencyGraphic(unitKey, envCents, midiRange);
			float uniHzAbove 	= Arithmetic::centsToFrequencyGraphic(unitKey, unisonCents, midiRange);
			float phaseOffset 	= timePerColEnv * envHzAbove + timePerColUni * uniHzAbove;

			double totalPhase	= cumePhases[i] + uniPhase;
			double scaledPhase	= columnSize * (totalPhase + 10000);
			long truncPhase 	= (long) scaledPhase;
			double remainder	= scaledPhase - truncPhase;

			int phase = truncPhase & (columnSize - 1);

			cumePhases[i] += phaseOffset;

			jassert(remainder >= 0 && remainder <= 1);

			if(interpolate)
			{
				if(phase != 0 || remainder != 0)
				{
					if(phase != 0)
					{
						columnBuf.offset(phase).copyTo(phaseMoveBuffer);
						columnBuf.copyTo(phaseMoveBuffer + (int) (columnSize - phase));
					}
					else
					{
						columnBuf.copyTo(phaseMoveBuffer);
					}


					phaseMoveBuffer2.mul(phaseMoveBuffer, 1 - remainder).addProduct(phaseMoveBuffer + 1, remainder);
					phaseMoveBuffer2[columnSize - 1] = (1 - remainder) * phaseMoveBuffer[columnSize - 1] + remainder * phaseMoveBuffer[0];

					col.addProduct(phaseMoveBuffer2, relativePan);
				}
				else
				{
					col.addProduct(columnBuf, relativePan);
				}
			}

			else
			{
				if(phase != 0)
				{
					columnBuf.offset(phase).copyTo(phaseMoveBuffer);
					columnBuf.copyTo(phaseMoveBuffer.offset(columnSize - (int) phase));

					col.addProduct(phaseMoveBuffer, relativePan);
				}
				else
				{
					col.addProduct(columnBuf, relativePan);
				}
			}
		}
	}
}


void VisualDsp::trackWavePhaseEnvelope()
{
/*
	vector<Vertex2> path;
	int length = phasePreFXCols.size();

	const int harmonicsToAverage = 1;
	ScopedAlloc<Ipp32f> averagingKernel(harmonicsToAverage);
	ScopedAlloc<Ipp32f> tempPhases(harmonicsToAverage);
	ScopedAlloc<Ipp32f> averagePhase(length);
	ScopedAlloc<Ipp32f> modWhole(length);
	ScopedAlloc<Ipp32f> modPart(length);

	for(int i = 0; i < harmonicsToAverage; ++i)
		averagingKernel[i] = i + 1;

	float scale = powf(2, getObj(Spectrum3D).getScaleFactor());
	status(ippsDivCRev_32f_I(1.f, averagingKernel, harmonicsToAverage));

	float invNumToAvg;
	status(ippsSum_32f(averagingKernel, harmonicsToAverage, &invNumToAvg, ippAlgHintFast));
	status(ippsMulC_32f_I(1 / invNumToAvg, averagingKernel, harmonicsToAverage));

	for(int i = 0; i < length; ++i)
	{
		status(ippsCopy_32f(phasePreFXCols[i], tempPhases, harmonicsToAverage));
		status(ippsAddC_32f_I(-0.5f, tempPhases, harmonicsToAverage));
		status(ippsMulC_32f_I(-1, tempPhases, harmonicsToAverage));
		status(ippsDotProd_32f(tempPhases, averagingKernel, harmonicsToAverage, averagePhase.get() + i));
	}

	status(ippsAddC_32f_I(1000.f, averagePhase, length));		// want to have same polarity for modf
	status(ippsModf_32f(averagePhase, modWhole, modPart, length));

	bool didSplit;

	for(int i = 0; i < length; ++i)
	{
		didSplit = i > 0 && modWhole[i - 1] != modWhole[i];

		if(didSplit)
		{
			float x = phasePreFXCols[i].x - 0.001f;
			path.push_back(Vertex2(x, modPart[i - 1]));

			x += 0.002f;
			path.push_back(Vertex2(x, modPart[i]));
		}
		else
		{
			path.push_back(Vertex2(phasePreFXCols[i].x, modPart[i]));
		}
	}

//	getObj(AutoModeller).reducePath<Vertex2>(path);

	Mesh* wavePhase = getObj(LayerManager).getEnvMesh(LayerSources::GroupWavePhase);
	wavePhase->destroy();

	for (int i = 0; i < (int) path.size(); ++i)
	{
		Vertex* y0r0b0 = new Vertex(0.f, path[i].x, path[i].y, 0.f, 0.f);
		Vertex* y1r0b0 = new Vertex(*y0r0b0);
		y1r0b0->values[Vertex::Time] = 1;

		VertCube* cube = new VertCube(y0r0b0, y1r0b0, wavePhase);
		wavePhase->verts.push_back(y0r0b0);
		wavePhase->verts.push_back(y1r0b0);
		wavePhase->lines.push_back(cube);

		float sharpness = 0.3f;
		if(i > 0 && path[i].x - path[i - 1].x < 0.003f)
			sharpness = 1.f;

		for (int j = 0; j < VertCube::numVerts; ++j)
			cube->getVertex(j)->values[Vertex::Curve] = sharpness;
	}

	getObj(OscPhaseRasterizer).setMesh(wavePhase);
//	getObj(OscPhaseRasterizer).calcCrossPoints();
*/
}


Buffer<float> VisualDsp::getFreqColumn(float position, bool isMags)
{
	bool postFX = getSetting(ViewStage) >= ViewStages::PostFX;

	vector<Column>& cols = isMags ?
			postFX ? fftPostFXCols : fftPreFXCols :
			postFX ? phasePostFXCols : phasePreFXCols;

	int index = jmin((int) cols.size() - 1, int(position * (cols.size() - 0.5)));

	if(index < 0)
		return Buffer<float>();

	jassert(isPositiveAndBelow(index, (int) cols.size()));

	return cols[index];
}


const vector<Column>& VisualDsp::getFreqColumns()
{
	bool postFX = getSetting(ViewStage) >= ViewStages::PostFX;

	if(postFX)
	{
		return getSetting(MagnitudeDrawMode) ? fftPostFXCols : phasePostFXCols;
	}
	else
	{
		return getSetting(MagnitudeDrawMode) ? fftPreFXCols : phasePreFXCols;
	}
}


const vector<Column>& VisualDsp::getTimeColumns()
{
	int stage = getSetting(ViewStage);

	if(stage == ViewStages::PostFX)
	{
		return postFXCols;
	}
	else if(stage == ViewStages::PreProcessing)
	{
		return preEnvCols;
	}
	else
	{
		return postEnvCols;
	}
}


Buffer<Ipp32f> VisualDsp::getTimeArray()
{
	int stage = getSetting(ViewStage);

	return stage == ViewStages::PostFX ? postFXArray :
		   stage == ViewStages::PreProcessing ? preEnvArray : postEnvArray;
}


Buffer<Ipp32f> VisualDsp::getFreqArray()
{
	bool postFX = getSetting(ViewStage) >= ViewStages::PostFX;

	return getSetting(MagnitudeDrawMode) ?
			(postFX ? fftPostFXArray : fftPreFXArray) :
			(postFX ? phasePostFXArray : phasePreFXArray);
}


void VisualDsp::processThroughEffects(int numColumns)
{
	jassert(getSetting(ViewStage) >= ViewStages::PostFX);

	if(getSetting(DrawWave))
	{
		int nextPow2 	= postEnvCols.front().size();
		int key 		= postEnvCols.front().midiKey;

		checkEffectsWavColumns(numColumns, nextPow2, fftPreFXCols.front().size(), key);
		copyArrayOrParts(fftPreFXCols, fftPostFXCols);

//	  #ifndef BEAT_EDITION
		copyArrayOrParts(phasePreFXCols, phasePostFXCols);
//	  #endif

		return;
	}
	else
	{
		checkEffectsColumns(numColumns);
	}

	SynthAudioSource& synth = getObj(SynthAudioSource);
	Waveshaper& waveshaper 	= synth.getWaveshaper();
	IrModeller& tubeModel 	= synth.getIrModeller();
	Equalizer& equalizer 	= synth.getEqualizer();

	bool haveWaveshaper 	= waveshaper.isEnabled();
	bool haveIrModeller 	= tubeModel.willBeEnabled();
	bool haveEqualizer		= equalizer.isEnabled();
	bool haveUnison 		= unison->getOrder(false) > 1;
	float volumeScale 		= getObj(OscControlPanel).getVolumeScale();

	onlyBeat(haveIrModeller = false);

	for (int i = 0; i < (int) postFXCols.size(); ++i)
		postFXCols[i].latency = 0;

	ScopedLock sl(fxColumnLock);

	if(haveWaveshaper)
	{
		waveshaper.clearGraphicDelayLine();

		int latency = waveshaper.doesGraphicOversample() ? waveshaper.getLatencySamples() : 0;

		for (int i = 0; i < (int) postFXCols.size(); ++i)
		{
			Column& col = postFXCols[i];
			waveshaper.processVertexBuffer(col);

			col.latency += latency;
		}
	}

	if(haveIrModeller)
	{
		tubeModel.clearGraphicOverlaps();

		int currSize = postFXCols.front().size();
		int lastSize = currSize;

		tubeModel.updateGraphicConvState(currSize, false);

		for (int i = 0; i < (int) postFXCols.size(); ++i)
		{
			Column& col = postFXCols[i];

			currSize = col.size();
			if(currSize != lastSize)
				tubeModel.updateGraphicConvState(currSize, false);

			tubeModel.processVertexBuffer(col);

			lastSize = currSize;
		}
	}

	if(haveEqualizer)
	{
		equalizer.clearGraphicBuffer();

		for(int i = 0; i < (int) postFXCols.size(); ++i)
			equalizer.processVertexBuffer(postFXCols[i]);
	}

	postFXArray.mul(volumeScale);

	if(! haveWaveshaper && ! haveIrModeller && ! haveUnison && ! haveEqualizer)
	{
		// prefXcols are copied in checkColumns()

		copyArrayOrParts(fftPreFXCols, fftPostFXCols);
		copyArrayOrParts(phasePreFXCols, phasePostFXCols);

		int tension = getConstant(LogTension);

		// need to first unmap amplitudes to apply volume scaling appropriately
		for(int i = 0; i < (int) fftPostFXCols.size(); ++i)
		{
			Arithmetic::applyInvLogMapping(fftPostFXCols[i], tension);
			fftPostFXCols[i].mul(volumeScale);
			Arithmetic::applyLogMapping(fftPostFXCols[i], tension);
		}
	}
	else
	{
		int numHarmonics;

		ScopedAlloc<float> mem(postFXCols.front().size());
		Buffer<float> moveBuffer(mem);

		for (int i = 0; i < (int) postFXCols.size(); ++i)
		{
			Column& col 	= postFXCols[i];
			numHarmonics 	= fftPostFXCols[i].size();
			int sizeIndex 	= sizeToIndex[col.size()];
			Transform& fft 	= ffts[sizeIndex];
//			Buffer<float> phaseBuf(phases, numHarmonics);

			col.latency &= (col.size() - 1);
			if(col.latency > 0)
			{
				ippsCopy_32f(col + col.latency, moveBuffer, col.size() - col.latency);
				ippsCopy_32f(col, moveBuffer.offset(col.size() - col.latency),  col.latency);
				moveBuffer.copyTo(col);
				col.latency = 0;
			}

			fft.forward(col);
//			col.copyTo(fftBuffer);
//			status(ippsFFTFwd_RToCCS_32f_I(fftBuffer, fftspecs[sizeIndex], 0));
//			status(ippsCartToPolar_32fc((Ipp32fc*)fftBuffer.get() + 1, magBuf, phaseBuf, numHarmonics));

			// undenormalize
			Buffer<float> magBuf(fft.getMagnitudes(), numHarmonics);
			magBuf.add(1e-11f);

			Arithmetic::applyLogMapping(magBuf, getConstant(FFTLogTensionAmp));

			magBuf.copyTo(fftPostFXCols[i]);
			fft.getPhases().copyTo(phasePostFXCols[i]);
		}

		phasePostFXArray.add((float) IPP_PI2);
	}

	if(getSetting(CurrentMorphAxis) == Vertex::Time)
	{
		unwrapPhaseColumns(phasePostFXCols);
	}
	else
	{
		phasePostFXArray.mul((float) IPP_RPI * 0.5f).add(0.5f);
	}
}


void VisualDsp::reset()
{
	destroyArrays();

	timeRasterizer->reset();
	spectRasterizer->reset();
	phaseRasterizer->reset();
}


void VisualDsp::destroyArrays()
{
	ScopedLock sl1(timeColumnLock);
	ScopedLock sl2(envColumnLock);
	ScopedLock sl3(fftColumnLock);
	ScopedLock sl4(fxColumnLock);

	fftPreFXArray	.clear();
	fftPostFXArray	.clear();
	postEnvArray	.clear();
	postFXArray		.clear();
	phasePreFXArray	.clear();
	phasePostFXArray.clear();
	preEnvArray		.clear();

	preEnvCols		.clear();
	postEnvCols		.clear();
	postFXCols		.clear();
	fftPreFXCols	.clear();
	fftPostFXCols	.clear();
	phasePreFXCols	.clear();
	phasePostFXCols	.clear();
}


VisualDsp::GraphicProcessor::GraphicProcessor(VisualDsp* processor, SingletonRepo* repo, VisualDsp::StageType stage)
		: SingletonAccessor(repo, "GraphicProcessor")
	,	processor(processor)
	,	stage(stage)
{
}


void VisualDsp::GraphicProcessor::performUpdate(int update)
{
	if(update == UpdateType::Update)
	{
		ScopedLock lock(processor->calculationLock);

		int reductionFactor = getSetting(ReductionFactor);
		int increment = isDetailReduced() ? reductionFactor : 1;
		int numColumns = processor->surface->getWindowWidthPixels() / increment;

		jassert(numColumns > 0);

		bool drawWave 	= getSetting(DrawWave) == 1;
		bool waveLoaded = getSetting(WaveLoaded) == 1;

		switch(stage)
		{
			case TimeStage:	processor->calcTimeDomain(numColumns); break;
			case EnvStage:	processor->processThroughEnvelopes(numColumns);	break;
			case FFTStage:
				if(drawWave)
				{
					if(waveLoaded)
						processor->calcWaveSpectrogram(numColumns);
				}
				else
				{
					processor->calcSpectrogram(numColumns);
				}
				break;

			case FXStage:
				processor->processThroughEffects(numColumns); break;
		}
	}
}


void VisualDsp::checkTimeColumns(int numColumns)
{
	ScopedLock sl(timeColumnLock);

	ResizeParams params(numColumns, &preEnvArray, &preEnvCols, 0, 0, 0, 0, 0);
	resizeArrays(params);
}

void VisualDsp::checkEnvelopeColumns(int numColumns)
{
	ScopedLock sl(envColumnLock);

	ResizeParams params(numColumns, &postEnvArray, &postEnvCols, 0, 0, 0, 0, &preEnvCols);
	resizeArrays(params);
}

void VisualDsp::checkFFTColumns(int numColumns)
{
	ScopedLock sl(fftColumnLock);

	ResizeParams params(numColumns, 0, 0, &fftPreFXArray, &fftPreFXCols,
						&phasePreFXArray, &phasePreFXCols, 0);

	resizeArrays(params);
}

void VisualDsp::checkEffectsColumns(int numColumns)
{
	ScopedLock sl(fxColumnLock);

	ResizeParams params(numColumns, &postFXArray, &postFXCols, &fftPostFXArray,
						&fftPostFXCols, &phasePostFXArray, &phasePostFXCols, &postEnvCols);
	resizeArrays(params);
}


/// Wave arrays
void VisualDsp::checkEnvWavColumns(int numColumns, int nextPow2, int overrideKey)
{
	ScopedLock sl(envColumnLock);

	ResizeParams params(numColumns, &postEnvArray, &postEnvCols, 0, 0, 0, 0, 0);
	params.setExtraParams(nextPow2, -1, overrideKey, false);
	resizeArrays(params);
}


void VisualDsp::checkFFTWavColumns(int numColumns, int numHarms, int overrideKey)
{
	ScopedLock sl(fftColumnLock);

	ResizeParams params(numColumns, 0, 0, &fftPreFXArray, &fftPreFXCols, &phasePreFXArray, &phasePreFXCols, 0);

	params.setExtraParams(-1, numHarms, overrideKey, false);
	resizeArrays(params);
}


void VisualDsp::checkEffectsWavColumns(int numColumns, int nextPow2, int numHarms, int overrideKey)
{
	ScopedLock sl(fxColumnLock);

	ResizeParams params(numColumns, &postFXArray, &postFXCols, &fftPostFXArray,
						&fftPostFXCols, &phasePostFXArray, &phasePostFXCols, &postEnvCols);

	params.setExtraParams(nextPow2, numHarms, overrideKey, false);
	resizeArrays(params);
}


void VisualDsp::resizeArrays(const ResizeParams& params)
{
	bool adjustColumnSizes = ! params.isEnvelope && ! getSetting(DrawWave) && getSetting(CurrentMorphAxis) == Vertex::Red;
	int numHarmonics = 0, nextPow2 = 0;
	int fftGridSize = 0, fullGridSize = 0;
	int numColumns = params.numColumns;

	if(! adjustColumnSizes)
	{
		if(params.overridePow2 > 0)
			nextPow2 = params.overridePow2;

		if(params.overrideHarms > 0)
			numHarmonics = params.overrideHarms;

		if(params.overridePow2 < 0 && params.overrideHarms < 0)
		{
			getNumHarmonicsAndNextPower(numHarmonics, nextPow2);
			jassert(numHarmonics > 0 && nextPow2 > 0);
		}

		fftGridSize 	= numColumns * numHarmonics;
		fullGridSize 	= numColumns * nextPow2;
	}

	if(params.freqColumns) 	params.freqColumns->resize(numColumns);
	if(params.timeColumns) 	params.timeColumns->resize(numColumns);
	if(params.phaseColumns) params.phaseColumns->resize(numColumns);

	ScopedAlloc<int> mem(3 * numColumns);
	Buffer<int> timeSizes = mem.place(numColumns);
	Buffer<int> freqSizes = mem.place(numColumns);
	Buffer<int> keys 	  = mem.place(numColumns);

	int key = params.overrideKey >= 0 ? params.overrideKey : getObj(MorphPanel).getCurrentMidiKey();

	if(adjustColumnSizes)
	{
		fullGridSize = 0;
		fftGridSize = 0;

		for(int i = 0; i < numColumns; ++i)
		{
			key = i * (getConstant(HighestMidiNote) - getConstant(LowestMidiNote)) / numColumns + getConstant(LowestMidiNote);
			getNumHarmonicsAndNextPower(numHarmonics, nextPow2, key);

			timeSizes[i] 	= nextPow2;
			freqSizes[i] 	= numHarmonics;
			keys[i] 		= key;

			fullGridSize += nextPow2;
			fftGridSize += numHarmonics;
		}
	}

	// for e3 panel's benefit -- we still want the key values to span the range without adjusting column sizes
	else if(params.isEnvelope)
	{
		for(int i = 0; i < numColumns; ++i)
			keys[i] = i * (getConstant(HighestMidiNote) - getConstant(LowestMidiNote)) / numColumns + getConstant(LowestMidiNote);
	}

	else
	{
		keys.set(key);
	}

	if(params.freqArray) 	params.freqArray->resize(fftGridSize);
	if(params.timeArray) 	params.timeArray->resize(fullGridSize);
	if(params.phaseArray)
	{
		params.phaseArray->resize(fftGridSize);
		params.phaseArray->withSize(fftGridSize).zero();
	}

	int timeOffset = 0, freqOffset = 0;

	for(int i = 0; i < numColumns; ++i)
	{
		if(adjustColumnSizes)
		{
			nextPow2 	= timeSizes[i];
			numHarmonics= freqSizes[i];
		}

		key 	= keys[i];
		float x = i / float(numColumns - 1);

		if(params.freqColumns)
			(*params.freqColumns)[i] 	= Column(*params.freqArray + freqOffset, numHarmonics, x, key);

		if(params.phaseColumns)
			(*params.phaseColumns)[i] 	= Column(*params.phaseArray + freqOffset, numHarmonics, x, key);

		if(params.timeColumns)
			(*params.timeColumns)[i] 	= Column(*params.timeArray + timeOffset, nextPow2, x, key);

		timeOffset 		+= nextPow2;
		freqOffset 		+= numHarmonics;
	}

	if(params.timeColumnsToCopy != 0 && params.timeColumns != 0)
		copyArrayOrParts((*params.timeColumnsToCopy), (*params.timeColumns));
}


void VisualDsp::getNumHarmonicsAndNextPower(int& numHarmonics, int& nextPow2, int key)
{
	int midiKey 	= key >= 0 ? key : getObj(MorphPanel).getCurrentMidiKey();
	numHarmonics	= getObj(LogRegions).getRegion(midiKey).size();
	nextPow2 		= NumberUtils::nextPower2(numHarmonics * 2);

	jassert(midiKey > 0);
	jassert(numHarmonics > 0);
	jassert(nextPow2 >= 2 * numHarmonics);
	jassert(nextPow2 <= 4096);
}


int VisualDsp::getTimeSamplingResolution()
{
	int numHarmonics, nextPow2;
	getNumHarmonicsAndNextPower(numHarmonics, nextPow2);

	return nextPow2;
}


void VisualDsp::surfaceResized()
{
	stopTimer();
	startTimer(200);
}


CriticalSection& VisualDsp::getColumnLock(int type)
{
	switch(type)
	{
		case TimeColType: 	return timeColumnLock;
		case EnvColType: 	return envColumnLock;
		case FreqColType: 	return fftColumnLock;
		case FXColType:		return fxColumnLock;

		default:
			return dummyLock;
	}
}


bool VisualDsp::areAnyFXActive()
{
	bool active = false;

	SynthAudioSource& synth = getObj(SynthAudioSource);
	active |= synth.getWaveshaper().isEnabled();
	active |= synth.getIrModeller().willBeEnabled();
	active |= synth.getEqualizer().isEnabled();
	active |= unison->getOrder(false) > 1;

	return active;
}


void VisualDsp::timerCallback()
{
	int widthPixels = surface->getWindowWidthPixels();

	if(widthPixels == volumeEnv.size())
	{
		stopTimer();
	}
	else
	{
		doUpdate(SourceMorph);
	}
}


const ScratchContext& VisualDsp::getScratchContext(int scratchChannel)
{
	if(scratchChannel == CommonEnums::Null || scratchChannel >= (int) scratchContexts.size())
	{
		return defaultScratchContext;
	}

	return scratchContexts[scratchChannel];
}


float VisualDsp::getScratchPosition(int scratchChannel)
{
	float unitPos = getObj(MorphPanel).getValue(getSetting(CurrentMorphAxis));

	const ScratchContext& context = getScratchContext(scratchChannel);
	Buffer<float> buffer = context.gridBuffer;

	if(buffer.empty() || scratchChannel == CommonEnums::Null)
	{
		return unitPos;
	}

	EnvelopeMesh* scratchMesh = meshLib->getEnvMesh(LayerGroups::GroupScratch, scratchChannel);

	if(scratchChannel >= (int) scratchContexts.size())
		return unitPos;

	jassert(scratchChannel < (int) scratchContexts.size());

	MeshLibrary::Properties* props = meshLib->getEnvProps(LayerGroups::GroupScratch, scratchChannel);
	bool haveScratch = props->active && scratchMesh->hasEnoughCubesForCrossSection();
	return haveScratch ? Resampling::lerpC(buffer, unitPos) : unitPos;
}
