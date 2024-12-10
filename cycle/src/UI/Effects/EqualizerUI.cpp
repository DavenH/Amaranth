#include <App/SingletonRepo.h>
#include <UI/Widgets/Knob.h>

#include "EqualizerUI.h"
#include "../../Audio/Effects/AudioEffect.h"
#include "../../Audio/Effects/Equalizer.h"
#include "../../Audio/SynthAudioSource.h"


EqualizerUI::EqualizerUI(SingletonRepo* repo, Effect* effect) :
	GuilessEffect("EqualizerUI", "EQ", Equalizer::numParams, repo, effect, UpdateSources::SourceEqualizer)
{
	using namespace Ops;
	double logTension = getConstant(LogTension);

	StringFunction emptyStr;
	StringFunction decibel30(StringFunction(0).chain(Mul, 2.0).chain(Add, -1.).chain(Mul, 30.0));
	StringFunction eqStr(StringFunction(1).chain(Mul, log(logTension) + 1).chain(PowRev, IPP_E)
										  .chain(Add, -1.0).chain(Mul, 16000.0 / logTension).chain(Max, 40.0));

	for(int i = 0; i <= Equalizer::Band5Gain; ++i)
	{
		Knob* knob = new Knob(repo, i, String::formatted("Band %d Gain", i + 1), 0.5f);
		knob->setStringFunctions(emptyStr, decibel30);

		paramGroup->addSlider(knob);
	}

	for(int i = Equalizer::Band1Freq; i <= Equalizer::Band5Freq; ++i)
	{
		Knob* knob = new Knob(repo, i, String::formatted("Band %d Frequency", i - Equalizer::Band1Freq + 1), (i + 0.5f) * 0.2f);
		knob->setStringFunctions(emptyStr, eqStr);

		paramGroup->addSlider(knob);
	}

	minTitleSize = 45;
}


void EqualizerUI::init()
{
	GuilessEffect::init();

	equalizer = &getObj(SynthAudioSource).getEqualizer();

	for(int i = 0; i < Equalizer::Band1Gain; ++i)
		paramGroup->setKnobValue(i, equalizer->getFrequencyTarget(i), false, false);
}


bool EqualizerUI::paramTriggersAggregateUpdate(int knobIndex)
{
	// if we're updating all knobs, we only want to update the partition once per pair of (gain, freq) params, and
	// the freq ones come last
	return ! paramGroup->updatingAllSliders && enabled;
}

void EqualizerUI::layoutKnobs(Rectangle<int> rect, Array<int>& knobIdcs, int knobSize, int knobSpacing)
{
	int numParams = 5;

	knobSize 	= jmin((rect.getWidth()) / numParams, int(getHeight() * 0.75f));
	knobSpacing = (rect.getWidth() - numParams * knobSize) / (numParams + 1);

	for(int i = 0; i < numParams; ++i)
	{
		int eighth = int(knobSize * 0.75);
		int sixth  = int(knobSize * 0.55);

		int i2 = i + Equalizer::Band1Freq;

		Rectangle<int> r = rect.removeFromLeft(knobSize);
		Knob* knob = paramGroup->getKnob<Knob>(i);
		Knob* knob2 = paramGroup->getKnob<Knob>(i2);

		knob->setBounds(r.withSize(eighth, eighth).translated(0, int(-knobSize * 0.35)));
		knob2->setBounds(r.withSize(sixth, sixth).translated((knobSize + knobSpacing) * 0.62, int(knobSize * 0.25)));

		rect.removeFromLeft(knobSpacing);
	}
}


String EqualizerUI::getKnobName(int index) const
{
	bool little = getWidth() < 300;

	if(index < Equalizer::Band1Freq)
	{
		int gain = (int) equalizer->getGainTarget(index);
		String prepend = gain > 0 ? "+" : String::empty;
		return prepend << gain;
	}
	else
	{
		index -= 5;
		int freq = (int) equalizer->getFrequencyTarget(index);
		if((little && freq >= 1000) || freq >= 10000)
		{
			freq /= 1000;
			return String(freq) + "k";
		}

		return String(freq);
	}
}


void EqualizerUI::finishedUpdatingAllSliders()
{
	equalizer->updateParametersToTarget();
}


void EqualizerUI::doLocalUIUpdate()
{
	repaint();
}

void EqualizerUI::performUpdate(int updateType)
{
	repaint();
}
