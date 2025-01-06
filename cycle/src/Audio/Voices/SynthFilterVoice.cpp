#include <Algo/Oversampler.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Definitions.h>
#include <Util/LogRegions.h>

#include "SynthesizerVoice.h"
#include "SynthFilterVoice.h"
#include "../../Util/CycleEnums.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../UI/Panels/ModMatrixPanel.h"
#include "../../UI/VertexPanels/DeformerPanel.h"
#include "../../UI/VertexPanels/Spectrum3D.h"
#include "../../UI/VertexPanels/Waveform3D.h"


SynthFilterVoice::SynthFilterVoice(SynthesizerVoice* parent, SingletonRepo* repo) :
        CycleBasedVoice(parent, repo)
{
    timeLayers	= &getObj(MeshLibrary).getLayerGroup(LayerGroups::GroupTime);
    freqLayers	= &getObj(MeshLibrary).getLayerGroup(LayerGroups::GroupSpect);
    phaseLayers	= &getObj(MeshLibrary).getLayerGroup(LayerGroups::GroupPhase);

    const int maxPartials    = getConstant(MaxCyclePeriod) / 2;
    const double spectMargin = getRealConstant(SpectralMargin);

    phaseScaleRamp.resize(maxPartials);
//	latencyMoveBuff.resize(getConstant(oscLatencySamples));

    phases[Left]			.resize(maxPartials);
    phases[Right]			.resize(maxPartials);
    magnitudes[Left]		.resize(maxPartials);
    magnitudes[Right]		.resize(maxPartials);
    phaseAccumBuffer[Left]	.resize(maxPartials);
    phaseAccumBuffer[Right]	.resize(maxPartials);

    auto& deformerPanel = getObj(DeformerPanel);

    freqRasterizer.setWrapsEnds(false);
    freqRasterizer.setDeformer(&deformerPanel);
    freqRasterizer.setCalcDepthDimensions(false);
    freqRasterizer.setLimits(-spectMargin, 1 + spectMargin);

    phaseRasterizer.setWrapsEnds(false);
    phaseRasterizer.setScalingMode(MeshRasterizer::Bipolar);
    phaseRasterizer.setDeformer(&deformerPanel);
    phaseRasterizer.setCalcDepthDimensions(false);
    phaseRasterizer.setInterpolatesCurves(true);
    phaseRasterizer.setLimits(-spectMargin, 1 + spectMargin);

    cycleCompositeAlgo = Interpolate;
}

void SynthFilterVoice::initialiseNoteExtra(const int midiNoteNumber, const float velocity) {

    const bool smooth = getDocSetting(ParameterSmoothing);
    auto* props = getObj(MeshLibrary).getProps(LayerGroups::GroupTime, parent->voiceIndex);
    jassert(props != nullptr);

    if(props != nullptr) {
        props->updateParameterSmoothing(smooth);
    }

    // TODO
    freqRasterizer.updateOffsetSeeds(1, DeformerPanel::tableSize);
    phaseRasterizer.updateOffsetSeeds(1, DeformerPanel::tableSize);

    if(parent->flags.haveFFTPhase) {
        phaseScaleRamp.withSize(noteState.numHarmonics).ramp(1.f, 1.f).sqrt();
    }
}

void SynthFilterVoice::calcCycle(VoiceParameterGroup& group) {
    bool doFwdFFT 		= false;
    int oversampleFactor= oversamplers[0]->getOversampleFactor();
    int samplingSize 	= oversampleFactor * noteState.nextPow2;
    int channelCount	= 1;

    noteState.isStereo  = false;
    rastBuf		 	 	= rastBuffer.withSize(noteState.nextPow2);

    for (int i = 0; i < 2; ++i) {
        samplingBufs[i] = layerAccumBuffer[i].withSize(samplingSize);
        accumBufs[i] 	= layerAccumBuffer[i].withSize(noteState.nextPow2);
        magBufs[i] 	 	= magnitudes[i].withSize(noteState.numHarmonics);
        phaseBufs[i]  	= phases[i].withSize(noteState.numHarmonics);

        samplingBufs[i].zero();
    }

    if (parent->flags.haveTime) {
        doFwdFFT = calcTimeDomain(group, samplingSize, oversampleFactor);
        channelCount = noteState.isStereo ? 2 : 1;

        if (doFwdFFT && oversampleFactor > 1) {
            for (int i = 0; i < channelCount; ++i) {
                // FFT voices are cyclical, so only continuous with self, so instead of
                // previous cycle's tail we need to wrap our own
                oversamplers[i]->resetDelayLine();
                oversamplers[i]->sampleDown(samplingBufs[i], rastBuf, true);

                rastBuf.copyTo(accumBufs[i]);
            }
        }
    }

    bool rightPhasesAreSet = false;

    // forward fft for time-domain cycle
    if (doFwdFFT) {
        for (int c = 0; c < channelCount; ++c) {
            Transform& fft = audioSource->getFFT(noteState.nextPow2);
            fft.forward(accumBufs[c]);
            fft.getMagnitudes().copyTo(magBufs[c]);
            fft.getPhases().copyTo(phaseBufs[c]);
        }

        rightPhasesAreSet = noteState.isStereo;
    } else {
        for (int i = 0; i < 2; ++i) {
            magBufs[i].zero();
            phaseBufs[i].zero();
        }

        rightPhasesAreSet = true;
    }

    Buffer<Float32> fftRamp = getObj(LogRegions).getRegion(noteState.lastNoteNumber);

    calcMagnitudeFilters(fftRamp);
    calcPhaseDomain(fftRamp, doFwdFFT, rightPhasesAreSet, channelCount);

    // inverse FFT
    for(int c = 0; c < channelCount; ++c) {
        Transform& fft = audioSource->getFFT(noteState.nextPow2);

        magBufs[c].copyTo(fft.getMagnitudes());
        phaseBufs[c].copyTo(fft.getPhases());
        fft.inverse(accumBufs[c]);
    }

    if(! noteState.isStereo) {
        accumBufs[Left].copyTo(accumBufs[Right]);
    }

    jassert(fabsf(accumBufs[0].front()) < 1000);
    jassert(fabsf(accumBufs[1].front()) < 1000);
}

bool SynthFilterVoice::calcTimeDomain(VoiceParameterGroup& group, int samplingSize, int oversampleFactor) {
    double deltaPow2 = 1 / double(noteState.nextPow2);
    bool requireFwdFFT = false;

    Buffer timeBuf(rastBuffer, samplingSize);

    int layerSize = timeLayers->size();

    for (int layerIdx = 0; layerIdx < layerSize; ++layerIdx) {
        MeshLibrary::Layer& layer = parent->meshLib->getLayer(LayerGroups::GroupTime, layerIdx);
        MeshLibrary::Properties& props = *layer.props;

        if (!props.active || !layer.mesh->hasEnoughCubesForCrossSection()) {
            continue;
        }

        double samplingDelta = oversampleFactor == 1 ? deltaPow2 : deltaPow2 / double(oversampleFactor);

        MorphPosition position = props.pos[parent->voiceIndex];
        position.time = getScratchTime(props.scratchChan, frame.frontier);

        timeRasterizer.setMorphPosition(position);
        timeRasterizer.setNoiseSeed(random.nextInt(DeformerPanel::tableSize));
        timeRasterizer.setInterceptPadding((float) samplingDelta * 2);
        timeRasterizer.calcCrossPoints(layer.mesh, 0.f);

        if (timeRasterizer.isSampleable()) {
            timeRasterizer.doesIntegralSampling() ?
                    timeRasterizer.samplePerfectly(samplingDelta, timeBuf, 0.) :
                    timeRasterizer.sampleWithInterval(timeBuf, samplingDelta, 0.);

            float layerPan = props.pan;
            noteState.isStereo |= fabsf(layerPan - 0.5) > 0.03;

            float leftPan, rightPan;
            Arithmetic::getPans(layerPan, leftPan, rightPan);

            samplingBufs[Left].addProduct(timeBuf, leftPan);
            samplingBufs[Right].addProduct(timeBuf, rightPan);

            requireFwdFFT = true;
        }
    }

    return requireFwdFFT;
}

void SynthFilterVoice::calcMagnitudeFilters(Buffer<Float32> fftRamp) {
    bool wasStereoBeforeLayer 	= noteState.isStereo;
    float additiveScale 		= Arithmetic::calcAdditiveScaling(noteState.numHarmonics);

    Buffer harmRast(rastBuffer.withSize(noteState.numHarmonics));

    for(auto& layer : freqLayers->layers) {
        MeshLibrary::Properties& props = *layer.props;

        if (!props.active || !layer.mesh->hasEnoughCubesForCrossSection()) {
            continue;
        }

        float progress = getScratchTime(props.scratchChan, frame.frontier);

        freqRasterizer.setMorphPosition(props.pos[parent->voiceIndex].withTime(progress));
        freqRasterizer.setNoiseSeed(random.nextInt(DeformerPanel::tableSize));
        freqRasterizer.calcCrossPoints(layer.mesh, 0.f);

        if (freqRasterizer.isSampleable()) {
            freqRasterizer.sampleAtIntervals(fftRamp, harmRast);

            wasStereoBeforeLayer |= noteState.isStereo;

            float layerPan = props.pan;
            noteState.isStereo |= (fabsf(layerPan - 0.5f) > 0.03f);

            // sound is newly stereo, copy left to the empty right buffer
            if (noteState.isStereo && !wasStereoBeforeLayer) {
                magBufs[Left].copyTo(magBufs[Right]);
                wasStereoBeforeLayer = true;
            }

            float leftPan, rightPan;
            Arithmetic::getPans(layerPan, leftPan, rightPan);

            float dynamicRange = Spectrum3D::calcDynamicRangeScale(props.range);
            dynamicRange = std::sqrt(dynamicRange);

            float multiplicand = powf(2.f, dynamicRange);
            float thresh = powf(1e-19f, 1.f / dynamicRange);

            // denorm prevention
            harmRast.threshLT(thresh).pow(dynamicRange);

            if (props.mode == Spectrum3D::Additive) {
                multiplicand *= additiveScale;
            }

            harmRast.mul(multiplicand);

            if (props.mode == Spectrum3D::Subtractive) {
                Buffer rightBuffer(phaseAccumBuffer[Left].withSize(noteState.numHarmonics));

                if (noteState.isStereo) {
                    harmRast.copyTo(rightBuffer);

                    if (leftPan > 0.f) {
                        if (leftPan != 1.f) {
                            harmRast.sub(1.f).mul(leftPan).add(1.f);
                        }

                        magBufs[Left].mul(harmRast);
                    }

                    if (rightPan > 0.f) {
                        if (rightPan != 1.f) {
                            rightBuffer.sub(1.f).mul(rightPan).add(1.f);
                        }

                        magBufs[Right].mul(rightBuffer);
                    }
                } else {
                    magBufs[Left].mul(harmRast);
                }
            } else {
                magBufs[Left].addProduct(harmRast, leftPan);

                if (noteState.isStereo) {
                    magBufs[Right].addProduct(harmRast, rightPan);
                }
            }
        }
    }
}

void SynthFilterVoice::calcPhaseDomain(Buffer<float> fftRamp,
                                       bool didFwdFFT,
                                       bool rightPhasesAreSet,
                                       int& channelCount)
{
    bool wasStereo = noteState.isStereo;

    Buffer<float> phaseAccBufs[] = {
        // stereo buffer?
        phaseAccumBuffer[Left].withSize(noteState.numHarmonics),
        phaseAccumBuffer[Right].withSize(noteState.numHarmonics)
    };
    Buffer harmRast(rastBuffer.withSize(noteState.numHarmonics));

    if (!phaseLayers->size() == 0) {
        phaseAccBufs[Left].zero();
        phaseAccBufs[Right].zero();
    }

    bool haveAnyValidPhaseLayers = false;

    for(auto& layer : phaseLayers->layers) {
        MeshLibrary::Properties& props = *layer.props;

        if (props.active && layer.mesh->hasEnoughCubesForCrossSection()) {
            float layerPan 			= props.pan;
            noteState.isStereo     |= (fabsf(layerPan - 0.5f) > 0.03f);
            haveAnyValidPhaseLayers = true;
        }
    }

    channelCount = noteState.isStereo ? 2 : 1;

    if (haveAnyValidPhaseLayers && parent->flags.haveFFTPhase) {
        // need to copy some stuff if we've got stereo phases up ahead and not already stereo
        if (noteState.isStereo != wasStereo) {
            magBufs[Left].copyTo(magBufs[Right]);

            if (didFwdFFT) {
                phaseBufs[Left].copyTo(phaseBufs[Right]);
                rightPhasesAreSet = true;
            }
        }

        if (!rightPhasesAreSet) {
            phaseBufs[Left].copyTo(phaseBufs[Right]);
        }

        int layerSize = phaseLayers->size();

        for (int i = 0; i < layerSize; ++i) {
            MeshLibrary::Layer& layer 		= phaseLayers->layers[i];
            MeshLibrary::Properties& props 	= *layer.props;

            if(! props.active || ! layer.mesh->hasEnoughCubesForCrossSection()) {
                continue;
            }

            float progress = getScratchTime(props.scratchChan, frame.frontier);

            phaseRasterizer.setMorphPosition(props.pos[parent->voiceIndex].withTime(progress));
            phaseRasterizer.setNoiseSeed(random.nextInt(DeformerPanel::tableSize));
            phaseRasterizer.calcCrossPoints(layer.mesh, 0.f);

            if (phaseRasterizer.isSampleable()) {
                phaseRasterizer.sampleAtIntervals(fftRamp, harmRast);

                float pans[2];
                Arithmetic::getPans(props.pan, pans[0], pans[1]);

                float phaseAmpScale = Spectrum3D::calcPhaseOffsetScale(props.range);

                harmRast.mul(phaseAmpScale * MathConstants<float>::twoPi);

                for(int c = 0; c < channelCount; ++c) {
                    phaseAccBufs[c].addProduct(harmRast, pans[c]);
                }
            }
        }

        for(int c = 0; c < channelCount; ++c) {
            phaseAccBufs[c].mul(phaseScaleRamp.withSize(noteState.numHarmonics));
            phases[c].add(phaseAccBufs[c]);		// phases[1] has to be copied or zeroed at this point!
        }
    }
}

void SynthFilterVoice::updateValue(int outputId, int dim, float value) {
    CycleBasedVoice::updateValue(outputId, dim, value);
    MeshLibrary::GroupLayerPair pair = getObj(ModMatrixPanel).toLayerIndex(outputId);

    if (pair.isNotNull()) {
        parent->meshLib->getProps(pair.groupId, pair.layerIdx)->setDimValue(parent->voiceIndex, dim, value);
    }
}
