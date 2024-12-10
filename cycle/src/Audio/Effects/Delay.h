#ifndef _delay_h
#define _delay_h

#include <ipp.h>
#include <Obj/Ref.h>
#include <App/SingletonAccessor.h>

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

	Buffer<Ipp32f> buffer;
};

class Delay : public Effect
{
public:
	enum DelayParam { Time, Feedback, SpinIters, Spin, Wet, numDelayParams };

	Delay(SingletonRepo* repo);
	~Delay();

	void processBuffer(AudioSampleBuffer& buffer);
	bool isEnabled() const;

	void setWetLevel(double value);
	bool setSpinAmount(double value);
	bool setFeedback(double value);
	bool setSpinIters(double value);
	bool setDelayTime(double value);
	static int calcSpinIters(double value);
	void recalculateWetBuffers(bool print = false);
	void setUI(GuilessEffect* comp);
	void audioThreadUpdate();
	double calcDelayTime(double unitValue);

protected:
	bool doParamChange(int param, double value, bool doFurtherUpdate);

private:
	static const int delaySize = 65536;

	bool pendingWetBufferUpdate;

	int spinIters;
	int pendingSpinIters;
	long readPosition[2];

	double delayTime;
	double feedback;
	double spinAmount;
	double sampleRate;
	double wetLevel;

	Ref<GuilessEffect> ui;
	vector<SpinParams> spinParams[2];
	ScopedAlloc<Ipp32f> inputBuffer[2];
	ScopedAlloc<Ipp32f> wetBuffer[2];
};

#endif
