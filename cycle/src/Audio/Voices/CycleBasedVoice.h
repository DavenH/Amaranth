#pragma once

#include <vector>
#include <App/MeshLibrary.h>
#include <Array/ScopedAlloc.h>
#include <Array/StereoBuffer.h>
#include <Algo/Resampler.h>
#include <Algo/Oversampler.h>
#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>

#include "JuceHeader.h"
#include "VoiceParameterGroup.h"
#include "../../Curve/VoiceMeshRasterizer.h"

using std::vector;

class SynthesizerVoice;
class SynthAudioSource;
class Waveform3D;
class Spectrum3D;
class Unison;
class ModMatrixPanel;
class Envelope2D;

class CycleBasedVoice :	public SingletonAccessor
{
public:
    enum { Left, Right };
    enum CompositeAlgo {
        // compositing via chaining has perfect contiguity between cycles, but can only
        // be currently done by
        Chain,

        // Interpolate
        Interpolate,
        Overlap
    };

    CycleBasedVoice(SynthesizerVoice* voice, SingletonRepo*);

    void initCycleBuffers();
    virtual void testNumLayersChanged();

    void testIfOversamplingChanged();
    void testIfResamplingQualityChanged();

    /* Rendering */
    void initialiseNote(int midiNoteNumber, float velocity);
    virtual void initialiseNoteExtra(const int midiNoteNumber, const float velocity) {}
    virtual void calcCycle(VoiceParameterGroup& group) = 0;
    void fillLatency(StereoBuffer& channelPair);
    void render(StereoBuffer& channelPair);
    void commitCycleBufferAndWrap(StereoBuffer& channelPair);
    void stealNoteFrom(CycleBasedVoice* oldVoice);
    void ensureOversampleBufferSize(int numSamples);
    void unisonVoiceCountChanged();

//	void setInterpolateCycles(bool doInterpolate) 	{ interpolatingCycles = doInterpolate; 	}
//	bool doesInterpolateCycles() 					{ return interpolatingCycles; 			}

    /* Accessors */
    double getAngleDelta(int midiNumber, float detuneCents, double pitchEnvVal);
    int getOscillatorLatencySamples() 				{ return oversamplers[0]->getLatencySamples(); 	}
    int getCompositeAlgo() const					{ return cycleCompositeAlgo; 					}
    int getResamplingAlgo()	const 					{ return resamplingAlgo;						}
    float getLastRatio() const 						{ return noteState.lastPow2Ratio;				}
    virtual void updateValue(int outputId, int dim, float value);

    // Buffer<float> getScratchBuffer(int layerIndex);

protected:
    struct NoteState
    {
        NoteState() :
                totalSamplesPlayed(0),
                stride(1), numUnisonVoices(1),
                nextPow2(0), lastPow2(0), lastNoteNumber(60),
                isStereo(false), lastPow2Ratio(1.f), absVoiceTime(0.f),
                scratchVoiceTime(0.f), timePerOutputSample(0.f)
        {}

        bool isStereo;
        int stride;
        int nextPow2;
        int lastPow2;
        int numHarmonics;
        int lastNoteNumber;
        int numUnisonVoices;
        long totalSamplesPlayed;
        float absVoiceTime;
        float lastPow2Ratio;
        float scratchVoiceTime;
        double timePerOutputSample;

        void reset()
        {
            totalSamplesPlayed 	= 0;
            absVoiceTime 		= 0.f;
            scratchVoiceTime 	= 0.f;
        }
    };

    struct RenderFrame
    {
        RenderFrame() : cumePos(0), period(0), frontier(0), cycleCount(0) {}

        void reset()
        {
            cumePos 	= 0;
            frontier 	= 0;
            cycleCount 	= 0;
        }

        double cumePos;
        double period;

        long cycleCount;
        long frontier;
    };

    void renderInterpolatedCycles(int numSamples);
    void renderOverlappedCycles(int numSamples);
    void renderChainedCycles(int numSamples);
    void updateChainAngleDelta(VoiceParameterGroup& group, bool unisonEnabled, bool useFirstEnvelopeIndex = false);
    void updateEnvelopes(int unisonIdx, int deltaSamples);
    float getScratchTime(int layerIndex, double cumePos);

    int resamplingAlgo;
    int cycleCompositeAlgo;

    bool skipCalcNextPass;
    long updatePosition;
    long updateThreshSamples;

    float timePerOutputSample;

    Ref<Unison> 			unison;
    Ref<SynthesizerVoice> 	parent;
    Ref<ModMatrixPanel>		modMatrix;
    Ref<Waveform3D> 		waveform3D;
    Ref<Spectrum3D> 		spectrum3D;
    Ref<Envelope2D>			envelope2D;
    Ref<SynthAudioSource> 	audioSource;

    Random random;
    RenderFrame futureFrame, frame;
    NoteState noteState;

    vector<float> scratchTimes;
    OwnedArray<Oversampler> oversamplers;

    StereoBuffer oversampleAccumBuf;
    ScopedAlloc<Float32> emergencyBuffer;
    ScopedAlloc<Float32> rastBuffer;
    ScopedAlloc<Float32> tempBuffer;
    ScopedAlloc<Float32> cycleBufferMemory;
    ScopedAlloc<Float32> resamplingMemory;

    ScopedAlloc<Float32> layerAccumBuffer[2];	// (when interpolating) translationally symmetrical (far) future cycle
    ScopedAlloc<Float32> pastCycle[2];			// translationally symmetrical past cycle
    ScopedAlloc<Float32> biasedCycle[2];			// fades smoothly into immediate future cycle

    ScopedAlloc<Float32> lerpMemory;
    ScopedAlloc<Float32> halfBuf;				// copy buffer

    MeshLibrary::LayerGroup* timeLayers;

    VoiceMeshRasterizer timeRasterizer;
    vector<VoiceParameterGroup> groups;
};
