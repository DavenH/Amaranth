#include <App/SingletonRepo.h>
#include <Util/Util.h>

#include "Delay.h"
#include "../../UI/Effects/GuilessEffect.h"
#include "../../UI/Effects/DelayUI.h"

CycDelay::CycDelay(SingletonRepo* repo) :
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
    for(long& i : readPosition) {
        inputBuffer[i].resize(delaySize);
        i = 0;
    }

    recalculateWetBuffers();
}

CycDelay::~CycDelay() {
    int i = 0;
    for (auto spinParam : spinParams) {
        spinParam.clear();
        inputBuffer[i].clear();
        wetBuffer[i].clear();
        ++i;
    }
}

void CycDelay::audioThreadUpdate() {
    if (pendingWetBufferUpdate) {
        recalculateWetBuffers();
        pendingWetBufferUpdate = false;
    }
}

void CycDelay::processBuffer(AudioSampleBuffer& buffer) {
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

    if(spinParams[0].empty() || buffer.getNumSamples() == 0) {
        return;
    }

    int readPos;
    int selfReadPos;
    int bufferReadPos;
    int inputModulo = inputBuffer[0].size() - 1;
    float wetSum, spinDelayedSelf;
    int modulo = spinParams[0].front().buffer.size() - 1;

    jassert(modulo > 0);

    if(modulo < 1) {
        return;
    }

    // feedback exponentiated by spin delay : delay ratio
    float pownFeedback = powf(feedback, (int) spinParams[0].size()) + 1e-17f;

    jassert(! (modulo & (modulo + 1)));
    jassert(! (inputModulo & (inputModulo + 1)));

    for (int ch = 0; ch < jmin(2, buffer.getNumChannels()); ++ch) {
        int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i) {
            wetSum = 0;
            readPos = readPosition[ch] & modulo;
            float& sample = *buffer.getWritePointer(ch, i);

            inputBuffer[ch][int(readPosition[ch]) & inputModulo] = sample;

            for (int k = 0; k < (int) spinParams[ch].size(); ++k) {
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
//					std::cout << "idx:\t" << readPosition[i] << "\tbuffer pos\t" << bufferReadPos << "\tself pos\t" << selfReadPos << "\tdly input\t" << inputBuffer[i][bufferReadPos] <<
//						"\tdly self\t" << params->buffer[selfReadPos] << "\tcurr self\t" << readSample << "\wetsum\t" << wetSum << "\n";
            }

//			if(i == 0)
//				info("\n");

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

int CycDelay::calcSpinIters(double value) {
    int iters = jmax(1, int(12 * value * value));
    return iters;
}

bool CycDelay::isEnabled() const {
    return ui->isEffectEnabled();
}

void CycDelay::setWetLevel(double value) {
    wetLevel = value;
}

double CycDelay::calcDelayTime(double unitValue)
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

bool CycDelay::doParamChange(int param, double value, bool doFurtherUpdate) {
    bool changed = false;

    switch (param) {
        case Time: 		changed = setDelayTime(value); 					break;
        case Feedback: 	changed = setFeedback(value);					break;
        case SpinIters:	changed = setSpinIters(value); 					break;
        case Spin:		changed = setSpinAmount(value); 				break;
        case Wet:		changed = doFurtherUpdate; setWetLevel(value);	break;
        default:
            throw std::out_of_range("CycDelay::doParamChange");
    }

    if(doFurtherUpdate && changed) {
        pendingWetBufferUpdate = true;
    }

    return false;
}

bool CycDelay::setDelayTime(double value) {
    double newValue = calcDelayTime(value);

    return Util::assignAndWereDifferent(delayTime, newValue);
}

bool CycDelay::setFeedback(double value) {
    return Util::assignAndWereDifferent(feedback, value);
}

bool CycDelay::setSpinAmount(double value) {
    return Util::assignAndWereDifferent(spinAmount, value);
}

bool CycDelay::setSpinIters(double value) {
    int oldSpinIters = spinIters;
    pendingSpinIters = calcSpinIters(value);

    if (oldSpinIters == pendingSpinIters) {
        return false;
    }

    return true;
}

//void CycDelay::setSampleRate(double value)
//{
//	if(sampleRate == value)
//		return;
//
//	sampleRate = value;
//
//	pendingWetBufferUpdate = true;
//}

void CycDelay::setUI(GuilessEffect* comp) {
    ui = comp;
}

void CycDelay::recalculateWetBuffers(bool print) {
    spinIters = pendingSpinIters;

    if (print) {
        info("Wet buffers" << "\n");
        info("original spin iters: " << spinIters << "\n");
    }

    int singleSize = spinIters * delayTime * sampleRate;
    int pow2BufferSize = NumberUtils::nextPower2(singleSize);
    int wetBufferSize = spinIters * pow2BufferSize;

    if (print) {
        info("delay time: " << delayTime << "\n");
        info("delay samples: " << int(delayTime * sampleRate) << "\n");
        info("spin iters : " << spinIters << "\n");
        info("single size: " << singleSize << "\n");
        info("pow2 size: " << pow2BufferSize << "\n");
        info("totalsize: " << wetBufferSize << "\n");
    }

    for (int ch = 0; ch < 2; ++ch) {
        spinParams[ch].resize(spinIters);
        wetBuffer[ch].ensureSize(wetBufferSize);
        inputBuffer[ch].ensureSize(pow2BufferSize);

        wetBuffer[ch].zero();
        inputBuffer[ch].zero();
    }

    if (print) {
        info("\n");
    }

    for (int spinIdx = 0; spinIdx < spinIters; ++spinIdx) {
        float pan 				= 0.5f + 0.49999f * spinAmount * sinf(spinIdx / float(spinIters) * MathConstants<float>::twoPi);
        float startingLevel 	= powf(feedback, spinIdx + 1);
        long inputDelaySamples 	= (spinIdx + 1) * delayTime * sampleRate;
        long spinDelaySamples 	= spinIters * delayTime * sampleRate;

        SpinParams& left 		= spinParams[0][spinIdx];
        left.inputDelaySamples 	= inputDelaySamples;
        left.spinDelaySamples 	= spinDelaySamples;
        left.buffer 			= Buffer(wetBuffer[0] + spinIdx * pow2BufferSize, pow2BufferSize);
        left.startingLevel		= startingLevel;

        SpinParams& right 		= spinParams[1][spinIdx];
        right.inputDelaySamples = inputDelaySamples;
        right.spinDelaySamples 	= spinDelaySamples;
        right.buffer 			= Buffer(wetBuffer[1] + spinIdx * pow2BufferSize, pow2BufferSize);
        right.startingLevel		= startingLevel;
        Arithmetic::getPans(pan, left.pan, right.pan);

        if(print) {
            info("i: " 			<< spinIdx 			 		<< "\n");
            info("pan: " 		<< pan 				 		<< "\n");
            info("level: " 		<< startingLevel 	 		<< "\n");
            info("input delay: " << (int) inputDelaySamples 	<< "\n");
            info("spin delay: " 	<< (int) spinDelaySamples  	<< "\n");

//			info("left buffer:\t " 	<< left.buffer.get() 	<< "\tend:\t" << (left.buffer.get()  + pow2BufferSize) << "\n");
//			info("right buffer:\t " 	<< right.buffer.get() 	<< "\tend:\t" << (right.buffer.get() + pow2BufferSize) << "\n");
        }
    }

    if(print) {
        info("\n" << "\n");
    }
}

