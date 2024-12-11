#pragma once
#include "AudioEffect.h"
#include <Obj/Ref.h>
#include <Algo/Algo.h>
#include <Array/ScopedAlloc.h>
#include <Array/RingBuffer.h>
#include <Array/StereoBuffer.h>
#include <Thread/PendingAction.h>

class GuilessEffect;
class SingletonRepo;

class ReverbEffect:
		public Effect
	,	public MultiTimer
{
public:
	enum Action { blockSize, kernelSize, filterAction };
	enum Params { Size, Damp, Width, Highpass, Wet, numReverbParams };

	ReverbEffect(SingletonRepo* repo);
	void processBuffer(AudioSampleBuffer& buffer) override;
	bool doParamChange(int index, double value, bool doFutherUpdate) override;
	bool isEnabled() const override;
	void setPendingAction(int action, int value);
	void createKernel(int size);
	void createVolumeRamp(int i, int numBuffers, int buffSize, Buffer<float> ramp) const;
	void updateKernelSections();
	void audioThreadUpdate() override;
	void resetOutputBuffer();
	void setBlockSize(int size);
	void randomizePhase(Buffer<float> buffer);
	void setUI(GuilessEffect* comp) { ui = comp; }
	void timerCallback(int id) override;

private:
	float roomSize, wetLevel, dryLevel, width, highpass;
	float perBlockDecay, rolloffFactor, feedbackFactor;

	Reverb 				model;
	StereoBuffer		kernel;
	StereoBuffer  		outBuffer;
	Ref<GuilessEffect> 	ui;

	PendingActionValue<int> blockSizeAction;
	PendingActionValue<int> kernelSizeAction;
	PendingAction kernelFilterAction;

	int64 timeSinceLastFilterAction, timeSinceLastResizeAction;

	ConvReverb leftConv, rightConv;

	int delay1, delay2;
	float phaseNoise, magnNoise;
	unsigned int seed;
	ScopedAlloc<float> memory;
	ScopedAlloc<float> kernelMemory;
	ScopedAlloc<float> blockMemory;

	Buffer<float> hannWindow, randBuff, cumeBuff, mergeBuffer, phaseBuffer, noiseArray;
	Buffer<float> overlapBuffer[2];
	ReadWriteBuffer phaseAccumBuffer[2];
	ReadWriteBuffer feedbackBuffer[2];

	Transform fft;
};