#include <App/Doc/Document.h>
#include <App/MeshLibrary.h>
#include <App/SingletonRepo.h>
#include <Util/Arithmetic.h>

#include "SynthAudioSource.h"
#include "AudioSourceRepo.h"
#include "../UI/Effects/DelayUI.h"
#include "../UI/Effects/ReverbUI.h"
#include "../UI/Panels/OscControlPanel.h"
#include "../UI/Panels/ModMatrixPanel.h"
#include "../UI/VertexPanels/DeformerPanel.h"
#include "../Util/CycleEnums.h"

SynthAudioSource::SynthAudioSource(SingletonRepo* repo) :
        SingletonAccessor	(repo, "SynthAudioSource")
    ,	synth				(repo)
    ,	delay				(nullptr)
    ,	unison				(nullptr)
    ,	reverb				(nullptr)
    ,	chorus				(nullptr)
    ,	equalizer			(nullptr)
    ,	tubeModel			(nullptr)
    ,	waveshaper			(nullptr)
    ,	lastAudioLevel		(0.f)
    ,	lastBlueLevel		(0.f)
    , 	tempoScale			(1.)
    , 	samplesProcessed	(-1)
    , 	tempRendBuffer		(2)
    , 	resampBuff			(2)
    , 	numEnvelopeDims		(2)
    ,	globalRasterAction	(GlobalRasterAction)
    , 	globalChangeAction	(GlobalChangeAction)
    ,	qualityChangeAction (QualityChangeAction)
    ,	initResamplerAction	(InitResamplerAction)
    ,	updateCycleCachesAction(UpdateCycleCachesAction)
    ,	controlFreqAction	(ControlFreqAction)
    ,	unisonVoicesAction	(UnisonVoicesAction)
    ,	modwRouteAction		(ModwRouteAction) {
    workBuffer.resize(1 << 17);

    pendingActions.add(&globalRasterAction);
    pendingActions.add(&globalChangeAction);
    pendingActions.add(&qualityChangeAction);
    pendingActions.add(&initResamplerAction);
    pendingActions.add(&updateCycleCachesAction);
    pendingActions.add(&controlFreqAction);
    pendingActions.add(&unisonVoicesAction);
    pendingActions.add(&modwRouteAction);

    unisonVoicesAction.trigger();

    for (int fftOrderIdx = 0; fftOrderIdx < numOctaves; ++fftOrderIdx) {
        int fftOrder 		= fftOrderIdx + 3;
        int size 			= 1 << fftOrder;
        sizeToIndex[size] 	= fftOrderIdx;

        ffts[fftOrderIdx].allocate(size, Transform::ScaleType::DivFwdByN, true);
    }

    getObj(Document).addListener(this);
    getObj(AudioHub).addListener(this);
}

SynthAudioSource::~SynthAudioSource() {
    // for debugging
    unison 		= nullptr;
    waveshaper 	= nullptr;
    tubeModel 	= nullptr;
    reverb 		= nullptr;
    delay 		= nullptr;
    equalizer 	= nullptr;

    workBuffer	  .clear();
    attackDeclick .clear();
    releaseDeclick.clear();
    angleDeltas	  .clear();
    tempMemory[0] .clear();
    tempMemory[1] .clear();
    fadeMemory	  .clear();
    resamplingMemory.clear();
}


void SynthAudioSource::init() {
    postProcessEffects.add(waveshaper);
    postProcessEffects.add(tubeModel);
    postProcessEffects.add(equalizer);
    postProcessEffects.add(delay);
    postProcessEffects.add(reverb);

    for (auto& i : resampleAccum) {
        // 2 = 44100.0/22050.0, the biggest ratio of input to output samplerate.
        i.allocate(getConstant(MaxBufferSize) * 2);
    }

    calcFades();

    for (int i = 0; i < getConstant(MaxNumVoices); ++i) {
        SynthesizerVoice* voice;

        voices.add(voice = new SynthesizerVoice(i, repo));
        synth.addVoice(voice);
    }

    synth.addSound(new SynthSound(repo));
}

void SynthAudioSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    calcDeclickEnvelope(sampleRate);
    synth.setCurrentPlaybackSampleRate(sampleRate);

    for (auto voice: voices) {
        voice->initCycleBuffers();
    }

    updateTempoScale();

    for (auto& scratch: globalScratch) {
        scratch.rast.setNoteOn();
    }
}


void SynthAudioSource::releaseResources() {
}


void SynthAudioSource::processBlock(AudioSampleBuffer &buffer, MidiBuffer &midiMessages) {
    ScopedLock lock(audioLock);

    doAudioThreadUpdates();

    buffer.clear();

    int numSamples 		= buffer.getNumSamples();
    double sampleRate 	= getObj(AudioHub).getSampleRate();
    bool needToResample = sampleRate != 44100.0;
    int numSamples44k   = numSamples;

    if (needToResample) {
        double ratio = 44100.0 / sampleRate;
        numSamples44k = (int) (ratio * (samplesProcessed + numSamples) + 0.999999999) -
                        (int) (ratio * samplesProcessed + 0.999999999);

        for (int i = 0; i < buffer.getNumChannels(); ++i) {
            tempMemory[i].ensureSize(numSamples44k);
            tempRendBuffer[i] = tempMemory[i].withSize(numSamples44k);

            if(numSamples44k > 0) {
                tempRendBuffer[i].zero(numSamples44k);
            }
        }

        tempRendBuffer.numChannels = buffer.getNumChannels();
    }

    StereoBuffer outBuffer(buffer);
    StereoBuffer& rendBuffer = needToResample ? tempRendBuffer : outBuffer;
    auto& meshLib = getObj(MeshLibrary);

    float deltaPerSample = 1.0 / 44100.0 / getObj(OscControlPanel).getLengthInSeconds();
    for(auto& scratchRast : globalScratch) {
        MeshLibrary::EnvProps* props = meshLib.getEnvProps(LayerGroups::GroupScratch, scratchRast.layerIndex);

        if(props->active && scratchRast.sampleable) {
            scratchRast.rast.renderToBuffer(numSamples, deltaPerSample, 0, *props, 1.f);
        }
    }

    float* channels[] = { rendBuffer.left.get(), rendBuffer.right.get() };
    AudioSampleBuffer buffer44k(channels, buffer.getNumChannels(), numSamples44k);

    MidiBuffer* midiBuff = &midiMessages;
    MidiBuffer midi44k;

    if (needToResample) {
        convertMidiTo44k(midiMessages, midi44k, numSamples44k);
        midiBuff = &midi44k;
    }

    // sound processing @ 44100
    if (numSamples44k > 0) {
        synth.renderNextBlock(buffer44k, *midiBuff, 0, numSamples44k);

        waveshaper-> updateSmoothedParameters(numSamples44k);
        tubeModel->	 updateSmoothedParameters(numSamples44k);
        equalizer->	 updateSmoothedParameters(numSamples44k);

        for (auto voice : voices) {
            // todo why is this condition necessary? Makes more sense to invert it.
            if(voice->getCurrentlyPlayingNote() >= 0) {
                voice->updateSmoothedParameters(numSamples44k);
                getObj(MeshLibrary).updateSmoothedParameters(voice->getVoiceIndex(), numSamples44k);
            }
        }

        for(auto postProcessEffect : postProcessEffects) {
            postProcessEffect->process(buffer44k);
        }

        volumeScale.update(numSamples44k);

        for(int i = 0; i < buffer.getNumChannels(); ++i) {
            volumeScale.maybeApplyRamp(
                workBuffer.withSize(numSamples44k),
                Buffer<float>(buffer44k, i)
            );
        }
    }

    if (needToResample) {
        if (numSamples44k > 0) {
            for (int ch = 0; ch < tempRendBuffer.numChannels; ++ch) {
                HermiteState &state = hermStates[ch];
                Buffer<float>& in 	= tempRendBuffer[ch];
                Buffer<float>& out 	= resampBuff[ch];

                int size = state.resample(in, out);

                rendBuffer[ch] = resampBuff[ch].withSize(size);
            }
        }

        samplesProcessed += numSamples;
    }

    for (int ch = 0; ch < rendBuffer.numChannels; ++ch) {
        resampleAccum[ch].write(rendBuffer[ch]);
        resampleAccum[ch].read(numSamples).copyTo(outBuffer[ch]);
    }

    float audioLevel = 0;

    for(int i = 0; i < rendBuffer.numChannels; ++i) {
        audioLevel = jmax(audioLevel, outBuffer[i].max());
    }

    lastAudioLevel = audioLevel;
}

void SynthAudioSource::paramChanged(int controller, float value) {
    if (controller == Synthesizer::VolumeParam) {
        volumeScale = value;
    } else {
        int saturatedValue = roundToInt(getConstant(ControllerValueSaturation) * value);
        synth.handleController(1, controller, saturatedValue);
    }
}

void SynthAudioSource::controlFreqChanged() {
    controlFreqAction.setValueAndTrigger(1 << getDocSetting(ControlFreq));
}

void SynthAudioSource::unisonOrderChanged() {
    unisonVoicesAction.setValueAndTrigger(unison->getOrder(false));
}

void SynthAudioSource::setEnvelopeMeshes(bool lock) {
//	progressMark

    std::unique_ptr<ScopedLock> sl;

    if (lock) {
        sl = std::make_unique<ScopedLock>(audioLock);
    }

    for (auto voice : voices) {
        voice->fetchEnvelopeMeshes();
    }
}

void SynthAudioSource::enablementChanged() {
    ScopedLock sl(audioLock);

    for (auto voice : voices) {
        voice->enablementChanged();
    }
}

void SynthAudioSource::prepNewVoice() {
    ScopedLock sl(audioLock);

    for (auto voice : voices) {
        voice->prepNewVoice();
    }
}

float SynthAudioSource::getModValue() {
    return voices.getUnchecked(0)->getModWheelValue();
}

void SynthAudioSource::setModValue(double value) {
    for (auto voice : voices) {
        voice->setModWheelValue((float) value);
    }
}

void SynthAudioSource::allNotesOff() {
    ScopedLock sl(audioLock);

    for (auto voice : voices) {
        voice->stop(false);
    }
}

Effect* SynthAudioSource::getDspEffect(int fxEnum) {
    switch (fxEnum) {
        case EffectTypes::TypeEqualizer: 	return equalizer;
        case EffectTypes::TypeWaveshaper: 	return waveshaper;
        case EffectTypes::TypeDelay: 		return delay;
        case EffectTypes::TypeUnison: 		return unison;
        case EffectTypes::TypeIrModeller: 	return tubeModel;
        case EffectTypes::TypeReverb: 		return reverb;
        default: throw std::out_of_range("Unsupported effect " + fxEnum);
    }
}

void SynthAudioSource::calcDeclickEnvelope(double /*samplerate*/) {
    const double samplerate = 44100.0;

    int attackLength 	= (int) ceil(0.00145 * samplerate);
    int releaseLength 	= (int) ceil(0.01 * samplerate);

    if(attackDeclick.size() == attackLength) {
        return;
    }

    ScopedAlloc<Float32> buff(attackLength);
    Buffer ramp = buff;

    attackDeclick.resize(attackLength);

    ramp.ramp(2, -15 / float(attackLength - 1)).exp().mul(-1.f).exp();
    ramp.copyTo(attackDeclick);

    buff.resize(releaseLength);
    ramp = buff;

    ramp.ramp(2, -15 / float(releaseLength - 1)).exp().mul(-1.f).exp().subCRev(1.f);

    releaseDeclick.resize(releaseLength);
    ramp.copyTo(releaseDeclick);
}

void SynthAudioSource::calcFades() {
    int totalSize = 0;

    for (int i = 0; i < numOctaves; ++i) {
        totalSize += 8 << i;
    }

    fadeMemory.resize(totalSize);

    const float pi = MathConstants<float>::pi;
    for (int i = 0; i < numOctaves; ++i) {
        int size = 8 << i;
        int half = size / 2;

        fadeIns[i]  = fadeMemory.place(half);
        fadeOuts[i] = fadeMemory.place(half);

        Buffer<float> in  = fadeIns[i];

        in.ramp(-0.5f * pi, pi / float(half - 1)).sin().add(1.f).mul(0.5f);

        VecOps::subCRev(in, 1.f, fadeOuts[i]);
    }

    Range range(getConstant(LowestMidiNote), getConstant(HighestMidiNote));
    angleDeltas.resize(range.getLength());

    for(int i = 0; i < range.getLength(); ++i) {
        angleDeltas[i] = NumberUtils::noteToFrequency(i + getConstant(LowestMidiNote), 0.0);
    }
}

void SynthAudioSource::convertMidiTo44k(const MidiBuffer& source, MidiBuffer& dest, int numSamples44k) {
    if (numSamples44k == 0) {
        carryMessages.clear();
        for (const MidiMessageMetadata md : source) {
            carryMessages.add(md.getMessage());
        }
        return;
    }

    const double srRatio = 44100.0 / getObj(AudioHub).getSampleRate();

    dest.clear();

    for (int i = 0; i < carryMessages.size(); ++i) {
        dest.addEvent(carryMessages.getUnchecked(i), 0);
    }

    for (const MidiMessageMetadata md : source) {
        const int position44k = roundToInt(md.samplePosition * srRatio + 0.5);
        dest.addEvent(md.getMessage(), position44k);
    }
}


void SynthAudioSource::initResampler() {
    double sampleRateReal = getObj(AudioHub).getSampleRate();
    samplesProcessed = 0;

    int totalSize = 0;
    int inRate, outRate;

    Arithmetic::getInputOutputRates(inRate, outRate, sampleRateReal);

    /*
    for(int ch = 0; ch < 2; ++ch)
    {
        Resampler& r = sampleRateConverter[ch];

        r.initFixedWithLength(0.95f, 9.f, inRate, outRate, 10,
                              getObj(AudioSourceRepo).getBufferSize(),
                              sourceSize[ch], destSize[ch]);
        totalSize += sourceSize[ch] + destSize[ch];

    }

    resamplingMemory.ensureSize(totalSize);
    */

    int bufferSize = getObj(AudioHub).getBufferSize();
    int nextPower2 = NumberUtils::nextPower2(bufferSize * 2);
    resamplingMemory.ensureSize((nextPower2 + bufferSize) * 2);

    double srcToDstRatio = (double) inRate / (double) outRate;

    for (int ch = 0; ch < 2; ++ch) {
        /*
        Resampler& r = sampleRateConverter[ch];

        r.source = resamplingMemory.place(sourceSize[ch]);
        r.dest 	 = resamplingMemory.place(destSize[ch]);

        r.reset();
        r.primeWithZeros();
        */

        resampleAccum[ch].reset();
        resampleAccum[ch].write(0.f);

        hermStates[ch].init(resamplingMemory.place(nextPower2));
        hermStates[ch].reset(srcToDstRatio);

        resampBuff[ch] = resamplingMemory.place(bufferSize);
    }
}

void SynthAudioSource::doAudioThreadUpdates() {
    for (auto pendingAction : pendingActions) {
        if (pendingAction->isPending()) {
            switch (pendingAction->getId()) {
                case QualityChangeAction:							break;
                case GlobalRasterAction:	rasterizeGlobalEnvs();	break;
                case GlobalChangeAction:	updateGlobality();		break;
                case InitResamplerAction:	initResampler();		break;

                case ModwRouteAction:
                    getObj(ModMatrixPanel).route(modwRouteAction.getValue(), ModMatrixPanel::MidiController + 1, -1);
                    break;

                case BufferSizeAction: {
                    int newSize = bufferSizeAction.getValue();
                    break;
                }

                case UpdateCycleCachesAction:
                    if(! voices.size() == 0) {
                        voices.getFirst()->updateCycleCaches();
                    }
                    break;

                case SampleRateAction: {
                    int newRate = sampleRateAction.getValue();
                    break;
                }

                default:
                    break;
            }

            pendingAction->dismiss();
        }
    }

    unison->audioThreadUpdate();
}

void SynthAudioSource::modulationChanged(float value, int voiceIndex, int outputId, int dim) {
    if (voiceIndex < 0) {
        for (auto voice : voices) {
            voice->modulationChanged(value, outputId, dim);
        }

        if (outputId >= ModMatrixPanel::ScratchEnvId) {
            int layerIndex = (outputId - ModMatrixPanel::ScratchEnvId) / numEnvelopeDims;

            for (auto& scratchRast : globalScratch) {
                if (scratchRast.layerIndex == layerIndex) {
                    MeshLibrary::EnvProps* props = getObj(MeshLibrary).getEnvProps(LayerGroups::GroupScratch,
                                                                                   scratchRast.layerIndex);
                    EnvRasterizer &rast = scratchRast.rast;

                    rast.updateValue(dim, value);

                    if (props->dynamic) {
                        rast.calcCrossPoints();
                        rast.validateState();

                        scratchRast.sampleable = rast.isSampleable();
                    }
                }
            }
        }
    } else {
        voices[voiceIndex]->modulationChanged(value, outputId, dim);
    }
}

void SynthAudioSource::updateTempoScale() {
    tempoScale = 1.;
    int beatsPerMeasure = 4;
    double beatsPerMin  = 120.;
    double beatsPerSecond = beatsPerMin / 60.;

  #if PLUGIN_MODE
    AudioPlayHead::CurrentPositionInfo info = getObj(PluginProcessor).getCurrentPosition();
    beatsPerSecond 		= info.bpm / 60.;
    beatsPerMeasure 	= info.timeSigNumerator;
  #endif

    double lengthSecs 	= getObj(OscControlPanel).getLengthInSeconds();
    double measureSecs 	= beatsPerMeasure / beatsPerSecond;
    tempoScale			= measureSecs / lengthSecs;
}

void SynthAudioSource::updateGlobality() {
    MeshLibrary::LayerGroup& scratchGroup = getObj(MeshLibrary).getLayerGroup(LayerGroups::GroupScratch);
    IDeformer& deformer                   = getObj(DeformerPanel);

    globalScratch.clear();

    for (int i = 0; i < scratchGroup.size(); ++i) {
        MeshLibrary::Layer &layer = scratchGroup.layers[i];

        if (dynamic_cast<MeshLibrary::EnvProps*>(layer.props)->global) {
            int size = globalScratch.size();

            globalScratch.emplace_back(EnvRasterizer(repo, &deformer, "GlobalScratch" + String(size)), i);
            globalScratch.back().rast.setMesh(layer.mesh);
        }
    }

    rasterizeGlobalEnvs();

    for(auto voice : voices) {
        voice->envGlobalityChanged();
    }
}

Buffer<float> SynthAudioSource::getScratchBuffer(int layerIndex) {
    for (auto& scratchRast : globalScratch) {
        if (scratchRast.layerIndex == layerIndex) {
            return scratchRast.rast.getRenderBuffer();
        }
    }

    return {};
}

void SynthAudioSource::rasterizeGlobalEnvs() {
    for (auto& scratchRast : globalScratch) {
        EnvRasterizer& rast = scratchRast.rast;

        rast.updateValue(Vertex::Red, 0);
        rast.setWantOneSamplePerCycle(false);
        rast.setLowresCurves(true);
        rast.calcCrossPoints();
        rast.validateState();

        scratchRast.sampleable = rast.isSampleable();

        if(scratchRast.sampleable) {
            rast.setNoteOn();
        }
    }
}

void SynthAudioSource::qualityChanged() {
}

SynthSound::SynthSound(SingletonRepo* repo) : SingletonAccessor(repo, "SynthSound") {}

bool SynthSound::appliesToNote(int midiNoteNumber) {
    return NumberUtils::within(midiNoteNumber, getConstant(LowestMidiNote), getConstant(HighestMidiNote));
}

bool SynthSound::appliesToChannel(int midiChannel) {
    return true;
}

void SynthAudioSource::documentAboutToLoad() {
}

void SynthAudioSource::documentHasLoaded() {
    numEnvelopeDims = getObj(Document).getVersionValue() >= 2 ? 3 : 2;
}
