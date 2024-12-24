#pragma once
#include <Obj/Ref.h>
#include <App/SingletonAccessor.h>
#include <Array/Buffer.h>
#include <Array/ScopedAlloc.h>
#include "AudioEffect.h"

class GuilessEffect;
class DelayUI;

class SpinParams
{
public:
	float pan;
	float startingLevel;
	int inputDelaySamples;
	int spinDelaySamples;

	Buffer<float> buffer;
};

class Delay : public Effect
{
public:
	enum DelayParam { Time, Feedback, SpinIters, Spin, Wet, numDelayParams };

	explicit Delay(SingletonRepo* repo);
	~Delay() override;

	void processBuffer(AudioSampleBuffer& buffer) override;
	[[nodiscard]] bool isEnabled() const override;

	void setWetLevel(double value);
	bool setSpinAmount(double value);
	bool setFeedback(double value);
	bool setSpinIters(double value);
	bool setDelayTime(double value);
	static int calcSpinIters(double value);
	void recalculateWetBuffers(bool print = false);
	void setUI(GuilessEffect* comp);
	void audioThreadUpdate() override;
	double calcDelayTime(double unitValue);

protected:
	bool doParamChange(int param, double value, bool doFurtherUpdate) override;

private:
	static constexpr int delaySize = 65536;

	bool pendingWetBufferUpdate;

	int spinIters;
	int pendingSpinIters;
	long readPosition[2]{};

	double delayTime;
	double feedback;
	double spinAmount;
	double sampleRate;
	double wetLevel;

	Ref<GuilessEffect> ui;
	vector<SpinParams> spinParams[2];
	ScopedAlloc<float> inputBuffer[2];
	ScopedAlloc<float> wetBuffer[2];
};