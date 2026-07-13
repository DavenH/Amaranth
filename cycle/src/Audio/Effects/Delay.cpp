#include <App/SingletonRepo.h>
#include <Audio/PluginProcessor.h>
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
    recalculateWetBuffers();
}

CycDelay::~CycDelay() = default;

void CycDelay::audioThreadUpdate() {
    if (pendingWetBufferUpdate) {
        recalculateWetBuffers();
        pendingWetBufferUpdate = false;
    }
}

void CycDelay::processBuffer(AudioSampleBuffer& buffer) {
    for (int ch = 0; ch < jmin(2, buffer.getNumChannels()); ++ch) {
        delays[ch].process({ buffer.getWritePointer(ch), buffer.getNumSamples() });
    }
}

int CycDelay::calcSpinIters(double value) {
    return CycleDsp::delaySpinIterations(value);
}

bool CycDelay::isEnabled() const {
    return ui->isEffectEnabled();
}

void CycDelay::setWetLevel(double value) {
    wetLevel = value;
}

double CycDelay::calcDelayTime(double unitValue) {
    double bpm = 120.0;
    int beatsPerMeasure = 4;

  #if PLUGIN_MODE
    AudioPlayHead::CurrentPositionInfo info = getObj(PluginProcessor).getCurrentPosition();

    if(info.bpm > 0.0 && info.timeSigNumerator > 0)
    {
        bpm = info.bpm;
        beatsPerMeasure = info.timeSigNumerator;
    }
  #endif

    return CycleDsp::delayTimeSeconds(unitValue, bpm, beatsPerMeasure);
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
    (void) print;

    CycleDsp::DelayConfiguration configuration;
    configuration.sampleRate = sampleRate;
    configuration.delaySeconds = delayTime;
    configuration.feedback = (float) feedback;
    configuration.spin = (float) spinAmount;
    configuration.wet = (float) wetLevel;
    configuration.spinIterations = spinIters;

    configuration.channel = CycleDsp::DelayChannel::Left;
    delays[0].configure(configuration);
    configuration.channel = CycleDsp::DelayChannel::Right;
    delays[1].configure(configuration);
}
