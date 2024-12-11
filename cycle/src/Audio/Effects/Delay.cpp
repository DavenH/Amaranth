#include <App/SingletonRepo.h>
#include <Util/Util.h>

#include "Delay.h"
#include "../../UI/Effects/GuilessEffect.h"
#include "../../UI/Effects/DelayUI.h"

Delay::Delay(SingletonRepo* repo) :
		Effect		(repo, "Delay")
	,	delayTime	(0.5f)
	,	feedback	(0.5f)
	,	spinAmount	(1.f)
	,	wetLevel	(0.9f)
	,	sampleRate	(44100)
	,	spinIters	(1)
	,	pendingSpinIters(1)
	,	pendingWetBufferUpdate(false)
{
	for(int i = 0; i < 2; ++i) {
		inputBuffer[i].resize(delaySize);
		readPosition[i] = 0;
	}

	recalculateWetBuffers();
}

Delay::~Delay() {
	for (int i = 0; i < 2; ++i) {
		spinParams[i].clear();
		inputBuffer[i].clear();
		wetBuffer[i].clear();
	}
}


void Delay::audioThreadUpdate() {
	if (pendingWetBufferUpdate) {
		recalculateWetBuffers();
		pendingWetBufferUpdate = false;
	}
}


void Delay::processBuffer(AudioSampleBuffer& buffer)
{
//	int delaySamples = delayTime * sampleRate;
//	int spinSamples = spinTime * sampleRate;
//
//	float added;
//	float destSample;
//	float destSpinSample;
//
//	for (int i = 0; i < jmin(2, buffer.getNumChannels()); ++i)
//	{
//		for (int j = 0; j < buffer.getNumSamples(); ++j)
//		{
//			float& sample 				= *buffer.getWritePointer(i, j);
//			dryBuffer[i][indices[i]] 	= sample;
//
//			destSample 					= buffers[i][(indices[i] - delaySamples) & (delaySize - 1)];
//			destSpinSample 				= buffers[i][(indices[i] - spinSamples) & (delaySize - 1)];
//			added 						= feedback * (destSample - destSpinSample * spinBuffer[i][indices[i]]);
//			buffers[i][indices[i]] 		= sample + added;
//			sample 						+= added * pan[i];
//			indices[i] 					= (indices[i] + 1) & delaySize - 1;
//		}
//	}
//
//	jassert(! _isnan(buffers[0][0]));

	jassert(! spinParams[0].empty());

	if(spinParams[0].empty() || buffer.getNumSamples() == 0)
		return;

	int readPos;
	int selfReadPos;
	int bufferReadPos;
	int inputModulo = inputBuffer[0].size() - 1;
	float wetSum, spinDelayedSelf;
	int modulo = spinParams[0].front().buffer.size() - 1;

	jassert(modulo > 0);

	if(modulo < 1)
		return;

	// feedback exponentiated by spin delay : delay ratio
	float pownFeedback = powf(feedback, (int) spinParams[0].size()) + 1e-17f;

	jassert(! (modulo & (modulo + 1)));
	jassert(! (inputModulo & (inputModulo + 1)));

	for (int ch = 0; ch < jmin(2, buffer.getNumChannels()); ++ch)
	{
		int numSamples = buffer.getNumSamples();

		for (int i = 0; i < numSamples; ++i)
		{
			wetSum 		  = 0;
			readPos 	  = readPosition[ch] & modulo;
			float& sample = *buffer.getWritePointer(ch, i);

			inputBuffer[ch][int(readPosition[ch]) & inputModulo] = sample;

			for(int k = 0; k < (int) spinParams[ch].size(); ++k)
			{
				SpinParams* params = &spinParams[ch][k];
				float& readSample = params->buffer[readPos];

				// read input, delayed by input delay
				bufferReadPos = (readPosition[ch] - params->inputDelaySamples) & inputModulo;

				// read self, delayed by spin delay
				selfReadPos = (readPosition[ch] - params->spinDelaySamples) & modulo;

				spinDelayedSelf = pownFeedback * params->buffer[selfReadPos] + 1e-17f;

				// combined delayed input with spin-delayed self
				readSample = inputBuffer[ch][bufferReadPos] + spinDelayedSelf;

				// pan the wet sample AFTER it's recursed, add to sum
				wetSum += params->pan * params->startingLevel * readSample;

#ifdef JUCE_DEBUG
//				if(wetSum < 1e-24f && wetSum > -1e-24f)
//				{
//					jassertfalse;
//				}
#endif

//				if(i == 0)
//					dout << "idx:\t" << readPosition[i] << "\tbuffer pos\t" << bufferReadPos << "\tself pos\t" << selfReadPos << "\tdly input\t" << inputBuffer[i][bufferReadPos] <<
//						"\tdly self\t" << params->buffer[selfReadPos] << "\tcurr self\t" << readSample << "\wetsum\t" << wetSum << "\n";
			}

//			if(i == 0)
//				dout << "\n";

			sample = sample + wetLevel * wetSum + 1e-19f;

#ifdef JUCE_DEBUG
//			if(sample < 1e-24f && sample > -1e-24f)
//			{
//				jassertfalse;
//			}
#endif

			++readPosition[ch];
		}
	}
}


int Delay::calcSpinIters(double value)
{
	int iters = jmax(1, int(12 * value * value));
	return iters;
}


bool Delay::isEnabled() const
{
	return ui->isEffectEnabled();
}


void Delay::setWetLevel(double value)
{
	wetLevel = value;
}


double Delay::calcDelayTime(double unitValue)
{
	unitValue = jmax(0.15, unitValue);

	double secondsPerBeat = 60.0 / 120.0;
	int beatsPerMeasure = 4;

  #if PLUGIN_MODE
	AudioPlayHead::CurrentPositionInfo info = getObj(PluginProcessor).getCurrentPosition();

	if(info.bpm > 0.0 && info.timeSigNumerator > 0)
	{
		secondsPerBeat = 60.0 / info.bpm;
		beatsPerMeasure = info.timeSigNumerator;
	}
  #endif

	return beatsPerMeasure * unitValue * unitValue * secondsPerBeat;
}


bool Delay::doParamChange(int param, double value, bool doFurtherUpdate)
{
	bool changed = false;

	switch(param)
	{
		case Time: 		changed = setDelayTime(value); 					break;
		case Feedback: 	changed = setFeedback(value);					break;
		case SpinIters:	changed = setSpinIters(value); 					break;
		case Spin:		changed = setSpinAmount(value); 				break;
		case Wet:		changed = doFurtherUpdate; setWetLevel(value);	break;
	}

	if(doFurtherUpdate && changed)
		pendingWetBufferUpdate = true;

	return false;
}


bool Delay::setDelayTime(double value)
{
	double newValue = calcDelayTime(value);

	return Util::assignAndWereDifferent(delayTime, newValue);
}


bool Delay::setFeedback(double value)
{
	return Util::assignAndWereDifferent(feedback, value);
}

bool Delay::setSpinAmount(double value)
{
	return Util::assignAndWereDifferent(spinAmount, value);
}


bool Delay::setSpinIters(double value)
{
	int oldSpinIters = spinIters;
	pendingSpinIters = calcSpinIters(value);

	if(oldSpinIters == pendingSpinIters)
		return false;

	return true;
}


//void Delay::setSampleRate(double value)
//{
//	if(sampleRate == value)
//		return;
//
//	sampleRate = value;
//
//	pendingWetBufferUpdate = true;
//}


void Delay::setUI(GuilessEffect* comp)
{
	ui = comp;
}


void Delay::recalculateWetBuffers(bool print)
{
	spinIters = pendingSpinIters;

	if(print)
	{
		dout << "Wet buffers" << "\n";
		dout << "original spin iters: " << spinIters << "\n";
	}

	int singleSize 		= spinIters * delayTime * sampleRate;
	int pow2BufferSize 	= NumberUtils::nextPower2(singleSize);
	int wetBufferSize 	= spinIters * pow2BufferSize;

	if(print)
	{
		dout << "delay time: " << delayTime << "\n";
		dout << "delay samples: " << int(delayTime * sampleRate) << "\n";
		dout << "spin iters : " << spinIters << "\n";
		dout << "single size: " << singleSize << "\n";
		dout << "pow2 size: " << pow2BufferSize << "\n";
		dout << "totalsize: " << wetBufferSize << "\n";
	}

	for(int ch = 0; ch < 2; ++ch)
	{
		spinParams[ch].resize(spinIters);
		wetBuffer[ch].ensureSize(wetBufferSize);
		inputBuffer[ch].ensureSize(pow2BufferSize);

		ippsZero_32f(wetBuffer[ch], wetBufferSize);
		ippsZero_32f(inputBuffer[ch], pow2BufferSize);
	}

	if(print)
		dout << "\n";

	for(int spinIdx = 0; spinIdx < spinIters; ++spinIdx)
	{
		float pan 				= 0.5f + 0.49999f * spinAmount * sinf(spinIdx / float(spinIters) * IPP_2PI);
		float startingLevel 	= powf(feedback, spinIdx + 1);
		long inputDelaySamples 	= (spinIdx + 1) * delayTime * sampleRate;
		long spinDelaySamples 	= spinIters * delayTime * sampleRate;

		SpinParams& left 		= spinParams[0][spinIdx];
		left.inputDelaySamples 	= inputDelaySamples;
		left.spinDelaySamples 	= spinDelaySamples;
		left.buffer 			= Buffer<float>(wetBuffer[0] + spinIdx * pow2BufferSize, pow2BufferSize);
		left.startingLevel		= startingLevel;

		SpinParams& right 		= spinParams[1][spinIdx];
		right.inputDelaySamples = inputDelaySamples;
		right.spinDelaySamples 	= spinDelaySamples;
		right.buffer 			= Buffer<float>(wetBuffer[1] + spinIdx * pow2BufferSize, pow2BufferSize);
		right.startingLevel		= startingLevel;
		Arithmetic::getPans(pan, left.pan, right.pan);

		if(print)
		{
			dout << "i: " 			<< spinIdx 			 		<< "\n";
			dout << "pan: " 		<< pan 				 		<< "\n";
			dout << "level: " 		<< startingLevel 	 		<< "\n";
			dout << "input delay: " << (int) inputDelaySamples 	<< "\n";
			dout << "spin delay: " 	<< (int) spinDelaySamples  	<< "\n";

//			dout << "left buffer:\t " 	<< left.buffer.get() 	<< "\tend:\t" << (left.buffer.get()  + pow2BufferSize) << "\n";
//			dout << "right buffer:\t " 	<< right.buffer.get() 	<< "\tend:\t" << (right.buffer.get() + pow2BufferSize) << "\n";
		}
	}

	if(print)
		dout << "\n" << "\n";
}

