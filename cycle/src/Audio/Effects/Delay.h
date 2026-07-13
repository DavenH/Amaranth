#pragma once

#include <App/SingletonAccessor.h>
#include <Audio/CycleDsp/CycleDelay.h>
#include <Obj/Ref.h>

#include "AudioEffect.h"

class GuilessEffect;
class DelayUI;

class CycDelay : public Effect
{
public:
	enum DelayParam { Time, Feedback, SpinIters, Spin, Wet, numDelayParams };

	explicit CycDelay(SingletonRepo* repo);
	~CycDelay() override;

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
	bool pendingWetBufferUpdate;

	int spinIters;
	int pendingSpinIters;

	double delayTime;
	double feedback;
	double spinAmount;
	double sampleRate;
	double wetLevel;

	Ref<GuilessEffect> ui;
	CycleDsp::CycleDelay delays[2];
};
