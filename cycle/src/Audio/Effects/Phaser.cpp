#include <App/SingletonRepo.h>

#include <cmath>
#include "Phaser.h"
#include "../../UI/Effects/PhaserUI.h"

Phaser::Phaser(SingletonRepo* repo) :
		Effect(repo, "Phaser")
	,	feedback(.7f)
	,	lfoPhase(0.f)
	,	depth(1.f)
	,	zm1(0.f)
	,	samplerate(44100.f) {
	setMin(440);
	setMax(1600);
	setRate(.5f);
}

void Phaser::processBuffer(AudioSampleBuffer& buffer) {
	for(int i = 0; i < buffer.getNumChannels(); ++i) {
		for(int j = 0; j < buffer.getNumSamples(); ++j) {
			update(*buffer.getWritePointer(i, j));
		}
	}
}

void Phaser::doParamChange(int index, float value)
{
	switch(index) {
		case Rate: 		setRate(value);		break;
		case Feedback:	setFeedback(value);	break;
		case Depth: 	setDepth(value);	break;
		case Min:		setMin(value);		break;
		case Max:		setMax(value);		break;
		default: throw std::invalid_argument("Invalid Parameter Index: " + std::to_string(index));
	}
}

void Phaser::setMin(float min) {
	dmin = min / (samplerate / 2.f);
}


void Phaser::setMax(float max) {
	dmax = max / (samplerate / 2.f);
}


void Phaser::setRate(float rate) {
	lfoInc = 2.f * IPP_PI * (rate / samplerate);
}


void Phaser::setFeedback(float feedback) {
	this->feedback = feedback;
}

void Phaser::setDepth(float depth) {
	this->depth = depth;
}

bool Phaser::isEnabled() const {
	return getObj(MainPanel).getPhaserCmpt()->isEffectEnabled();
}

void Phaser::update(float& inSamp) {
	float d = dmin + (dmax - dmin) * ((std::sin(lfoPhase) + 1.f) / 2.f);
	lfoPhase += lfoInc;

	if (lfoPhase >= IPP_PI * 2.f) {
		lfoPhase -= IPP_PI * 2.f;
	}

	for (auto& allpassDelay : allpassDelays) {
		allpassDelay.delay(d);
	}

	float y = allpassDelays[0].update(
				allpassDelays[1].update(
					allpassDelays[2].update(
						allpassDelays[3].update(
							allpassDelays[4].update(
								allpassDelays[5].update(inSamp + zm1 * feedback))))));
	zm1 = y;

	inSamp += y * depth;
}

