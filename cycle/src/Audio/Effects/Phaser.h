#ifndef _phaser_h
#define _phaser_h

#include <cmath>
#include "AudioEffect.h"

class Phaser : public Effect
{
public:
	Phaser(SingletonRepo* repo);

	void processBuffer(AudioSampleBuffer& buffer);
	bool isEnabled() const;

	enum { Rate, Feedback, Depth, Min, Max, numPhaserParams };

protected:
	void doParamChange(int index, float value);

private:
	void setMin(float min);
	void setMax(float max);
	void setRate(float rate);
	void setFeedback(float feedback);
	void setDepth(float depth);
	void update(float& inSamp);

	class AllpassDelay
	{
	public:
		AllpassDelay() :
			a1(0.f), zm1(0.f)
		{
		}

		void delay(float delay)
		{
			a1 = (1.f - delay) / (1.f + delay);
		}

		float update(float inSamp)
		{
			float y = inSamp * -a1 + zm1;
			zm1 = y * a1 + inSamp;

			return y;
		}

	private:
		float a1, zm1;
	};

	AllpassDelay allpassDelays[6];

	float dmin, dmax;
	float feedback;
	float lfoPhase;
	float lfoInc;
	float depth;
	float zm1;
	float samplerate;
};


#endif
