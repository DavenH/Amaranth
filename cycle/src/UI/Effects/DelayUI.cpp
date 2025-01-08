#include <UI/Widgets/Knob.h>
#include <Util/StringFunction.h>

#include "DelayUI.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../Audio/Effects/Delay.h"

DelayUI::DelayUI(SingletonRepo* repo, Effect* effect) :
		GuilessEffect("DelayUI", "Delay", CycDelay::numDelayParams, repo, effect)
{
	Knob *spinItrKnob, *timeKnob;

	paramGroup->addSlider(timeKnob = new Knob(repo, CycDelay::Time, "Delay time", 0.5f));
	paramGroup->addSlider(new Knob(repo, CycDelay::Feedback, "Feedback", 0.5f));
	paramGroup->addSlider(spinItrKnob = new Knob(repo, CycDelay::SpinIters, "Spin revolution length", 0.2f));
	paramGroup->addSlider(new Knob(repo, CycDelay::Spin, "Spin Amount", 0.5f));
	paramGroup->addSlider(new Knob(repo, CycDelay::Wet, "Wet Amount", 0.5f));

	using namespace Ops;
	StringFunction delayStr(StringFunction(1).max(0.15).pow(2.0).mul(4.0));
	spinItrKnob->setStringFunction(StringFunction(0).pow(2.0).mul(12).flr().max(1.0));
	timeKnob->setStringFunctions(delayStr, delayStr.withPostString(" beats"));
}

String DelayUI::getKnobName(int index) const {
	bool little 	= getWidth() < 300;
	bool superSmall = getWidth() < 200;

	switch (index) {
		case CycDelay::Time: 		return superSmall ? "tm" 	: "Time";
		case CycDelay::Feedback:	return superSmall ? "fb" 	: little ? "Fdbk" : "Feedbk";
		case CycDelay::Spin:		return superSmall ? "spn" 	: "Spin";
		case CycDelay::SpinIters:	return little ? 	"spn #" : "Spin Len";
		case CycDelay::Wet:			return superSmall ? "wet" 	: "Wet";
		default:
			break;
	}

	return {};
}
