#include <cmath>
#include <App/MeshLibrary.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Array/Buffer.h>
#include <Curve/EnvelopeMesh.h>
#include <Util/Arithmetic.h>

#include "CycleBasedVoice.h"
#include "SynthesizerVoice.h"
#include "Algo/Resampling.h"
#include "../SynthAudioSource.h"
#include "../CycleDefs.h"
#include "../../UI/Panels/ModMatrixPanel.h"
#include "../../UI/Panels/OscControlPanel.h"
#include "../../UI/VertexPanels/DeformerPanel.h"
#include "../../UI/VertexPanels/Envelope2D.h"
#include "../../UI/VertexPanels/Spectrum3D.h"
#include "../../UI/VertexPanels/Waveform3D.h"
#include "../../Util/CycleEnums.h"

SynthesizerVoice::SynthesizerVoice(int voiceIndex, SingletonRepo* repo) :
        SingletonAccessor(repo, "SynthesizerVoice")
    , 	filterVoice				(this, repo)
    , 	unisonVoice				(this, repo)
    , 	currentVoice			(&unisonVoice)

    , 	velocity				(1.f)
    , 	octave					(0)
    , 	renderBuffer			(2)
    , 	voiceIndex				(voiceIndex)
    , 	attackSamplePos			(0)
    , 	releaseSamplePos		(0)
    , 	blockStartOffset		(0)
    , 	currentLatencySamples	(0)
    , 	latencyFillerSamplesLeft(0)
    ,	volumeGroup				(LayerGroups::GroupVolume)
    ,	pitchGroup				(LayerGroups::GroupPitch)
    ,	scratchGroup			(LayerGroups::GroupScratch)
{
    audioSource = &getObj(SynthAudioSource);
    unison 		= &audioSource->getUnison();
    modMatrix	= &getObj(ModMatrixPanel);
    meshLib		= &getObj(MeshLibrary);

    envGroups[0] = &volumeGroup;
    envGroups[1] = &pitchGroup;
    envGroups[2] = &scratchGroup;

    auto& deformer = getObj(DeformerPanel);

    for (auto group: envGroups) {
        group->envGroup.emplace_back(EnvRasterizer(&deformer, "EnvRasterizer" + String(group->layerGroup) + "_0"), 0);

        EnvRenderContext& last = group->envGroup.back();
        last.rast.setWantOneSamplePerCycle(true);
        last.rast.setLowresCurves(true);
    }

    EnvRasterizer& volume = volumeGroup.envGroup.front().rast;
    volume.setWantOneSamplePerCycle(false);
    volume.setLowresCurves(false);

    for (auto& i: chanMemory) {
        i.resize(getConstant(MaxBufferSize));
    }

    flags.fadingIn = true;
    flags.fadingOut = false;
    flags.playing = false;

    pitchWheelValue.setSmoothingActivity(false);
}

void SynthesizerVoice::startNote(const int midiNoteNumber,
                                 const float velocityPrm,
                                 SynthesiserSound* sound,
                                 const int currentPitchWheelPosition) {
    adjustedMidiNote = midiNoteNumber + octave * 12;

    Range<int> midiRange(getConstant(LowestMidiNote), getConstant(HighestMidiNote));
    if (!NumberUtils::within(adjustedMidiNote, midiRange)) {
        stop(false);
        return;
    }

    flags.playing = true;
    flags.fadingIn = true;
    flags.fadingOut = false;

    for (auto& envRasterizer: envRasterizers) {
        envRasterizer->ensureParamSize(audioSource->unisonVoicesAction.getValue());
        envRasterizer->setNoteOn();
    }

    velocity = velocityPrm;
    attackSamplePos = 0;
    releaseSamplePos = 0;

    OscControlPanel& oscPanel = getObj(OscControlPanel);

    speedScale = 1. / oscPanel.getLengthInSeconds();
    float key = normalizeKey(adjustedMidiNote);

    modMatrix->route(key, ModMatrixPanel::KeyScale, voiceIndex);
    modMatrix->route(1 - velocity, ModMatrixPanel::Velocity, voiceIndex);

    initialiseEnvMeshes();
    pitchWheelMoved(currentPitchWheelPosition);

    speedScale.updateToTarget();
    aftertouchValue.updateToTarget();
    pitchWheelValue.updateToTarget();

    testMeshConditions();

    if (adjustedMidiNote < 0)
        return;

    currentVoice->initialiseNote(adjustedMidiNote, velocity);
    currentLatencySamples = getCurrentOscillatorLatency();
}

void SynthesizerVoice::stopNote(float velocity, bool allowTailOff) {
    if (allowTailOff) {
        if (flags.sustaining) {
            return;
        }

        for (auto& envRasterizer: envRasterizers) {
            envRasterizer->setNoteOff();
        }

        bool haveReleaseCurve = false;
        bool isVolumeStillActive = false;

        for (int i = 0; i < volumeGroup.size(); ++i) {
            EnvRenderContext& rast = volumeGroup[i];
            haveReleaseCurve |= rast.rast.hasReleaseCurve();

            if (MeshLibrary::EnvProps* props = meshLib->getEnvProps(LayerGroups::GroupVolume, i)) {
                isVolumeStillActive |= props->active;
            }
        }

        if (!haveReleaseCurve || !isVolumeStillActive) {
            if (getDocSetting(Declick)) {
                flags.fadingOut = true;
            } else {
                resetNote();
            }
        }
    } else {
        resetNote();
    }
}

void SynthesizerVoice::renderNextBlock(AudioSampleBuffer& audioBuffer, int startSample, int numSamples) {
    if (numSamples == 0 || (!flags.playing && latencyFillerSamplesLeft == 0)) {
        return;
    }

    blockStartOffset = startSample;
    int originalNumSamples = numSamples;

    if (latencyFillerSamplesLeft > 0) {
        numSamples = jmin(latencyFillerSamplesLeft, numSamples);
    } else if (flags.fadingOut) {
        numSamples = jmin(audioSource->releaseDeclick.size() - releaseSamplePos, numSamples);
    }

    float mappedVelocity = getEffectiveLevel();
    int numChans = audioBuffer.getNumChannels();

    /// buffer prep
    for (int i = 0; i < numChans; ++i) {
        chanMemory[i].ensureSize(numSamples);
        renderBuffer[i] = chanMemory[i].withSize(numSamples);
    }

    renderBuffer.numChannels = numChans;
    StereoBuffer outputBuffer(audioBuffer, startSample, originalNumSamples);

    for (int i = 0; i < numChans; ++i) {
        renderBuffer[i].zero();
    }

    /// latency filling
    if (latencyFillerSamplesLeft > 0) {
        fillRemainingLatencySamples(outputBuffer, numSamples, mappedVelocity);
        return;
    }

    if (!flags.playing) {
        return;
    }

    /// render
    currentVoice->render(renderBuffer);

    /// volume env
    calcEnvelopeBuffers(numSamples);

    if (flags.haveVolume) {
        // TODO needs render graph
        Buffer<float> volBuff = volumeGroup.envGroup.front().rendBuffer.withSize(numSamples);

        volBuff.mul(mappedVelocity);
        renderBuffer.mul(volBuff);
    } else {
        renderBuffer.mul(mappedVelocity);
    }

    /// declick
    if (getDocSetting(Declick)) {
        if (flags.fadingIn) {
            declickAttack();
        } else if (flags.fadingOut) {
            declickRelease();
        }
    }

    for (int i = 0; i < outputBuffer.numChannels; ++i)
        outputBuffer[i].add(renderBuffer[i]);
}

void SynthesizerVoice::fillRemainingLatencySamples(StereoBuffer& outputBuffer, int numSamples, float mappedVelocity) {
    currentVoice->render(renderBuffer);

    for (int c = 0; c < renderBuffer.numChannels; ++c) {
        outputBuffer[c].addProduct(renderBuffer[c], mappedVelocity);
    }

    latencyFillerSamplesLeft -= numSamples;

    if (latencyFillerSamplesLeft == 0) {
        resetNote();
    }
}

void SynthesizerVoice::aftertouchChanged(const int newValue) {
    double unitValue = newValue / double(127.);

    modMatrix->route(unitValue, ModMatrixPanel::Aftertouch, voiceIndex);
}

void SynthesizerVoice::pitchWheelMoved(const int newValue) {
    const int rangeSemis = getDocSetting(PitchBendRange);
    double semis = 2 * rangeSemis * (newValue / double(0x4000) - 0.5);
    pitchWheelValue = semis;
}

void SynthesizerVoice::controllerMoved(const int controllerNumber, const int newValue) {
    double unsaturatedValue = newValue / double(getConstant(ControllerValueSaturation));

    if (controllerNumber == Synthesizer::SpeedParam) {
        speedScale = unsaturatedValue;
    } else if (controllerNumber == Synthesizer::OctaveParam) {
        octave = (int) (unsaturatedValue);
    }
}

float SynthesizerVoice::normalizeKey(int noteNumber) {
    Range midiRange(getConstant(LowestMidiNote), getConstant(HighestMidiNote));
    float value = Arithmetic::getUnitValueForNote(noteNumber, midiRange);
    NumberUtils::constrain<float>(value, 0.f, 1.f);

    return value;
}

void SynthesizerVoice::initialiseEnvMeshes() {
    EnvRastGroup* envGroups[] = { &volumeGroup, &pitchGroup, &scratchGroup };

    for (auto& envGroup: envGroups) {
        EnvRastGroup& rastGroup = *envGroup;

        for (int i = 0; i < rastGroup.envGroup.size(); ++i) {
            EnvRenderContext& rast = rastGroup.envGroup[i];
            MeshLibrary::EnvProps* props = meshLib->getEnvProps(rastGroup.layerGroup, i);
            rast.sampleable = false;

            if (props->active && rast.rast.hasEnoughCubesForCrossSection()) {
                rast.rast.calcCrossPoints();
                rast.sampleable = rast.rast.isSampleable();
            }
        }
    }

    for (auto& envRasterizer: envRasterizers) {
        envRasterizer->setNoiseSeed(random.nextInt());
        envRasterizer->updateOffsetSeeds();
    }
}

void SynthesizerVoice::updateCycleCaches() {
    //	if(currentVoice->getCompositeAlgo() != CycleBasedVoice::Chain)
    //		currentVoice->updateCachedCycles();
}

void SynthesizerVoice::initCycleBuffers() {
    filterVoice.initCycleBuffers();
    unisonVoice.initCycleBuffers();
}

int SynthesizerVoice::getCurrentOscillatorLatency() {
    int oscLatency = 0;
    bool isRealtime = true;

#if PLUGIN_MODE
	isRealtime = ! repo->getPluginProcessor().isNonRealtime();
#endif

    if (isRealtime && getDocSetting(OversampleFactorRend) > 1 ||
        !isRealtime && getDocSetting(OversampleFactorRend) > 1)
        oscLatency = currentVoice->getOscillatorLatencySamples();

    if (getDocSetting(ResamplingAlgoRltm) && (flags.haveFilter || flags.haveFFTPhase))
        oscLatency += getConstant(ResamplerLatency);

    return oscLatency;
}

int SynthesizerVoice::getRenderOffset() {
    int renderOffset =
            currentVoice->getCompositeAlgo() == CycleBasedVoice::Interpolate &&
            currentVoice->getResamplingAlgo() == Resampling::Sinc
                ? Resampler::historyToWindow(getConstant(ResamplerLatency), 1 / currentVoice->getLastRatio())
                : 0;

    return renderOffset;
}

void SynthesizerVoice::declickAttack() {
    int renderOffset = getRenderOffset();
    int attackSize = audioSource->attackDeclick.size();

    int samplesToCopy = jmin(renderBuffer.left.size() - renderOffset,
                             attackSize - attackSamplePos);

    if (samplesToCopy > 0) {
        for (int i = 0; i < renderBuffer.numChannels; ++i) {
            if (attackSamplePos == 0 && renderOffset > 0) {
                renderBuffer[i].withSize(renderOffset).zero();
            }

            renderBuffer[i].section(renderOffset, samplesToCopy).mul(audioSource->attackDeclick + attackSamplePos);
        }

        attackSamplePos += samplesToCopy;
    }

    if (attackSamplePos >= attackSize) {
        flags.fadingIn = false;
    }
}

void SynthesizerVoice::declickRelease() {
    int samplesToCopy = jmin(renderBuffer.left.size(),
                             audioSource->releaseDeclick.size() - releaseSamplePos);

    if (samplesToCopy > 0) {
        for (int i = 0; i < renderBuffer.numChannels; ++i) {
            renderBuffer[i].withSize(samplesToCopy).mul(audioSource->releaseDeclick + releaseSamplePos);
        }

        releaseSamplePos += samplesToCopy;
    }

    if (releaseSamplePos >= audioSource->releaseDeclick.size()) {
        flags.fadingOut = false;
        resetNote();
    }
}

void SynthesizerVoice::startLatencyFillingOrStopNote() {
    if (currentLatencySamples > 0) {
        latencyFillerSamplesLeft = currentLatencySamples;
        flags.latencyFilling = true;
    } else {
        resetNote();
    }
}

void SynthesizerVoice::resetNote() {
    flags.playing = false;
    flags.looping = false;
    flags.latencyFilling = false;

    adjustedMidiNote = -1;

    clearCurrentNote();
}

void SynthesizerVoice::calcEnvelopeBuffers(int numSamples) {
    speedScale.update(numSamples);
    double deltaX = speedScale / 44100.0; //getSampleRate();

    bool anyActive = false;

    for (int i = 0; i < volumeGroup.size(); ++i) {
        EnvRenderContext& context = volumeGroup[i];
        EnvRasterizer& envRast = context.rast;
        MeshLibrary::EnvProps* props = meshLib->getEnvProps(LayerGroups::GroupVolume, context.layerIndex);

        if (props->active && envRast.hasEnoughCubesForCrossSection()) {
            bool stillActive = envRast.renderToBuffer(numSamples, deltaX, EnvRasterizer::headUnisonIndex, *props, 1.f);
            // TODO
            anyActive |= stillActive;
            jassert(envRast.getRenderBuffer().size() >= numSamples);
        }
    }

    if (!anyActive) {
        resetNote();
        return;
    }
}

void SynthesizerVoice::fetchEnvelopeMeshes() {
    envRasterizers.clear();
    auto& deformer = getObj(DeformerPanel);

    for (int groupIndex = 0; groupIndex < numElementsInArray(envGroups); ++groupIndex) {
        EnvRastGroup& group = *envGroups[groupIndex];
        int size = meshLib->getGroup(group.layerGroup).size();

        int numLocalEnvelopes = 0;
        for (int layerIndex = 0; layerIndex < size; ++layerIndex) {
            if (!meshLib->getEnvProps(group.layerGroup, layerIndex)->global)
                ++numLocalEnvelopes;
        }

        if (numLocalEnvelopes != group.size()) {
            group.envGroup.clear();

            for (int layerIndex = 0; layerIndex < size; ++layerIndex) {
                MeshLibrary::Layer layer = meshLib->getLayer(group.layerGroup, layerIndex);

                if (!dynamic_cast<MeshLibrary::EnvProps*>(layer.props)->global) {
                    String name = "EnvRasterizer" + String(groupIndex) + "_" + String(layerIndex);
                    group.envGroup.emplace_back(EnvRasterizer(&deformer, name), layerIndex);

                    EnvRasterizer& rast = group.envGroup.back().rast;
                    envRasterizers.push_back(&rast);
                    rast.setWantOneSamplePerCycle(true);
                    rast.setLowresCurves(true);
                    rast.setMesh(dynamic_cast<EnvelopeMesh*>(layer.mesh));
                }
            }
        } else {
            for (int i = 0; i < group.size(); ++i) {
                envRasterizers.push_back(&group.envGroup[i].rast);
            }
        }
    }

    // the only envelope group that has one output sample per input sample
    for (int i = 0; i < volumeGroup.size(); ++i) {
        volumeGroup.envGroup[i].rast.setWantOneSamplePerCycle(false);
    }

    for (auto rast: envRasterizers) {
        rast->setCalcDepthDimensions(false);
        rast->setToOverrideDim(true);
        rast->calcCrossPoints();
        rast->validateState();
    }
}

void SynthesizerVoice::updateSmoothedParameters(int deltaSamples) {
}

void SynthesizerVoice::enablementChanged() {
    testMeshConditions();

    CycleBasedVoice* oldVoice = currentVoice;

    if (flags.haveFilter || flags.haveFFTPhase) {
        currentVoice = &filterVoice;
    } else {
        currentVoice = &unisonVoice;
    }

    if (currentVoice != oldVoice) {
        // TODO
        //		if(flags.playing && volumeRasterizer.getMode() != EnvRasterizer::Releasing)
        //		{
        //			currentVoice->stealNoteFrom(oldVoice);
        //		}
        //		else
        //		{
        stop(false);
        //		}
    }
}

void SynthesizerVoice::modulationChanged(float value, int outputId, int dim) {
#ifndef BEAT_EDITION
    currentVoice->updateValue(outputId, dim, value);
#endif
}

void SynthesizerVoice::handleSustainPedal(int midiChannel, bool isDown) {
    if (isDown && flags.playing) {
        flags.sustaining = true;
    } else if (!isDown && flags.sustaining) {
        flags.sustaining = false;
        stop(true);
    }
}

float SynthesizerVoice::getEffectiveLevel() {
    float unisonScale = powf(2.f, -(unison->getOrder(true) - 1) * 0.14f);
    //	float subVelocity = getObj(MorphPanel).isCurrentModMappingVelocity() ? 0.7 * velocity + 0.3 * sqrtf(velocity) : velocity;

    return velocity * unisonScale;
}

void SynthesizerVoice::testMeshConditions() {
    Ref surface = &getObj(Waveform3D);
    Ref spectrum3D = &getObj(Spectrum3D);

    flags.haveFFTPhase = true;
    flags.haveFilter = true;
    flags.haveTime = true;

    ScopedLock sl1(surface->getLayerLock());
    ScopedLock sl2(spectrum3D->getLayerLock());

    bool haveAnyValidTimeLayers = meshLib->hasAnyValidLayers((int) LayerGroups::GroupTime);
    bool haveAnyValidFreqLayers = spectrum3D->haveAnyValidLayers(true, haveAnyValidTimeLayers);
    bool haveAnyValidPhaseLayers = spectrum3D->haveAnyValidLayers(false, haveAnyValidTimeLayers);

    flags.haveFFTPhase &= haveAnyValidPhaseLayers;
    flags.haveTime &= haveAnyValidTimeLayers;
    flags.haveFilter &= haveAnyValidFreqLayers;

    if (!haveAnyValidFreqLayers && !haveAnyValidTimeLayers) {
        stop(false);
    }
}

void SynthesizerVoice::prepNewVoice() {
    unisonVoice.prepNewVoice();
}

void SynthesizerVoice::envGlobalityChanged() {
    fetchEnvelopeMeshes();
}
