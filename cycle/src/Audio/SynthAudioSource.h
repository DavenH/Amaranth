#pragma once

#include <map>

#include <App/SingletonAccessor.h>
#include <Algo/FFT.h>
#include <Algo/HermiteResampler.h>
#include <Algo/Oversampler.h>
#include <Algo/Resampler.h>
#include <App/Doc/Document.h>
#include <Audio/AudioHub.h>
#include <Audio/AudioSourceProcessor.h>
#include <Audio/SmoothedParameter.h>
#include <Array/RingBuffer.h>
#include "JuceHeader.h"

#include "Synthesizer.h"

#include "../Audio/Effects/Reverb.h"
#include "../Audio/Effects/IrModeller.h"
#include "../Audio/Effects/Delay.h"
#include "../Audio/Effects/Phaser.h"
#include "../Audio/Effects/Chorus.h"
#include "../Audio/Effects/WaveShaper.h"
#include "../Audio/Effects/Equalizer.h"
#include "../Audio/Effects/Unison.h"
#include "../Audio/Voices/SynthesizerVoice.h"
#include "../Curve/EnvRenderContext.h"

using std::map;

class WavAudioSource;
class SynthFilterVoice;

class SynthSound:
        public SynthesiserSound
    ,	public SingletonAccessor
{
public:
    explicit SynthSound(SingletonRepo* repo);
    bool appliesToNote(int midiNoteNumber) override;
    bool appliesToChannel(int midiChannel) override;
};

class SynthAudioSource:
        public AudioSourceProcessor
    ,	public SingletonAccessor
    ,	public Document::Listener
    ,	public AudioHub::SettingListener
{
public:
    explicit SynthAudioSource(SingletonRepo* repo);
    ~SynthAudioSource() override;

    void init() override;
    void allNotesOff();
    void calcDeclickEnvelope(double samplerate);
    void calcFades();
    void controlFreqChanged();
    void enablementChanged();
    void prepNewVoice();
    void qualityChanged();
    void releaseResources() override;
    void paramChanged(int controller, float value);
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages) override;
    void setEnvelopeMeshes(bool lock);
    void setModValue(double value);
    void unisonOrderChanged();

    void documentAboutToLoad() override;
    void documentHasLoaded() override;

    Effect* getDspEffect(int fxEnum);

    float getModValue();
    double getTempoScale() const	{ return tempoScale; 		}
    Delay& getDelay() 				{ return *delay; 			}
    Equalizer& getEqualizer()		{ return *equalizer; 		}
    Waveshaper& getWaveshaper() 	{ return *waveshaper; 		}
    IrModeller& getIrModeller() 	{ return *tubeModel; 		}
    Unison& getUnison() 			{ return *unison; 			}
    ReverbEffect& getReverb() 		{ return *reverb; 			}

    int getNumEnvelopeDims() const 	{ return numEnvelopeDims; 	}

    Buffer<float> getWorkBuffer() 	{ return workBuffer; 		}
    float getLastAudioLevel() const { return lastAudioLevel;	}
    float getLastModLevel() 		{ return modwRouteAction.getValue(); }
    int getControlFreq() 			{ return controlFreqAction.getValue(); }
    int getNumUnisonVoices() 		{ return unisonVoicesAction.getValue(); }

    void setLastBlueLevel(float lvl) { modwRouteAction.setValue(lvl); 	}
    void setPendingInitResampler() 	{ initResamplerAction.trigger(); 	}
    void setPendingGlobalChange() 	{ globalChangeAction.trigger(); 	}
    void setPendingGlobalRaster() 	{ globalRasterAction.trigger(); 	}
    void setPendingModRoute() 		{ modwRouteAction.trigger(); 		}

    void setUnison(Unison* unison)  			{ this->unison = unison; 		 }
    void setReverb(ReverbEffect* reverb) 		{ this->reverb = reverb; 		 }
    void setWaveshaper(Waveshaper* waveshaper) 	{ this->waveshaper = waveshaper; }
    void setDelay(Delay* delay) 				{ this->delay = delay; 			 }
    void setChorus(Chorus* chorus) 				{ this->chorus = chorus; 		 }
    void setEqualizer(Equalizer* equalizer) 	{ this->equalizer = equalizer; 	 }
    void setIrModeller(IrModeller* modeller) 	{ this->tubeModel = modeller; 	 }

    void initResampler();
    void modulationChanged(float value, int voiceIndex, int outputId, int dim);
    Buffer<float> getScratchBuffer(int layerIndex);

    Synthesizer synth;

    Transform& getFFT(int sizePow2)
    {
        return ffts[sizeToIndex[sizePow2]];
    }

    /*
    IppsFFTSpec_R_32f* getFftSpec(int sizePow2)
    {
        if(sizeToIndex.find(sizePow2) == sizeToIndex.end())
            return 0;

        return fftspecs[sizeToIndex[sizePow2]];
    }
    */

    void updateTempoScale();
    void updateGlobality();
    void rasterizeGlobalEnvs();
    void doAudioThreadUpdates() override;

private:
    void convertMidiTo44k(const MidiBuffer& source, MidiBuffer& dest, int numSamples44k);

    enum { numOctaves = 9 };

    enum
    {
        GlobalRasterAction = LastActionEnum
    ,	GlobalChangeAction
    ,	BufferSizeAction
    ,	SampleRateAction
    ,	RareSampleRateAction
    ,	QualityChangeAction
    ,	InitResamplerAction
    ,	UpdateCycleCachesAction
    ,	ControlFreqAction
    ,	UnisonVoicesAction
    ,	ModwRouteAction
    };

    int numEnvelopeDims;

    int64 	samplesProcessed;

    float 	lastAudioLevel;
    float 	lastBlueLevel;
    double 	tempoScale;

    map<int, int> 		sizeToIndex;
    SmoothedParameter 	volumeScale;
    ReadWriteBuffer	 	resampleAccum[2];
    Resampler 			sampleRateConverter[2];
    HermiteState		hermStates[2];
    StereoBuffer 		tempRendBuffer, resampBuff;

    Ref<Unison> 		unison;
    Ref<Waveshaper> 	waveshaper;
    Ref<IrModeller> 	tubeModel;
    Ref<ReverbEffect> 	reverb;
    Ref<Delay> 			delay;
    Ref<Phaser> 		phaser;
    Ref<Chorus> 		chorus;
    Ref<Equalizer> 		equalizer;

    PendingActionValue<bool>  	globalRasterAction;
    PendingActionValue<bool>  	globalChangeAction;
    PendingActionValue<bool>  	qualityChangeAction;
    PendingActionValue<bool>  	initResamplerAction;
    PendingActionValue<bool>  	updateCycleCachesAction;
    PendingActionValue<int>   	controlFreqAction;
    PendingActionValue<int>   	unisonVoicesAction;
    PendingActionValue<float> 	modwRouteAction;

    Array<PendingAction*> 		pendingActions;
    vector<EnvRenderContext>	globalScratch;

    ScopedAlloc<Ipp32f> 		globalRenderMem;
    ScopedAlloc<Ipp32f> 		workBuffer;
    ScopedAlloc<Ipp32f> 		attackDeclick;
    ScopedAlloc<Ipp32f> 		releaseDeclick;
    ScopedAlloc<Ipp32f> 		angleDeltas;
    ScopedAlloc<Ipp32f> 		tempMemory	[2];
    ScopedAlloc<Ipp32f>			resamplingMemory;
    ScopedAlloc<Ipp32f>			fadeMemory;

    Buffer<Ipp32f> 				fadeIns	[numOctaves];
    Buffer<Ipp32f> 				fadeOuts[numOctaves];
    Transform					ffts	[numOctaves];

    Array<Effect*> 				postProcessEffects;
    Array<MidiMessage> 			carryMessages;
    Array<SynthesizerVoice*>	voices;

    friend class CycleBasedVoice;
    friend class SynthesizerVoice;
    friend class SynthFilterVoice;
    friend class WavAudioSource;
};