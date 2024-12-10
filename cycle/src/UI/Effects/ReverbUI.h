#ifndef _reverbcomponent_h
#define _reverbcomponent_h

#include <App/EditWatcher.h>
#include <App/Doc/Document.h>
#include <App/SingletonRepo.h>
#include <UI/Widgets/Knob.h>

#include "GuilessEffect.h"
#include "../../Audio/Effects/AudioEffect.h"
#include "../../Audio/Effects/Reverb.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../Util/CycleEnums.h"

class ReverbUI :
	public GuilessEffect
{
public:
	ReverbUI(SingletonRepo* repo, Effect* effect) :
			GuilessEffect("ReverbUI", "Reverb", ReverbEffect::numReverbParams, repo, effect, EffectTypes::TypeReverb)
	{
		Knob* size = new Knob(repo, ReverbEffect::Size, "Reverb Length", 0.5);
		paramGroup->addSlider(size);

//				StringFunction(StringFunction::MulAddPowMul).setArgs(6.f, 12.f, 2.f, 1.f / 44100.f).setRounds(false);
		using namespace Ops;
		StringFunction sizeShort = StringFunction().chain(Mul, 6.f).chain(Add, 12.f).chain(Pow, 2.f).chain(Mul, 1.f / 44100.f);
		size->setStringFunctions(sizeShort, sizeShort.withPostString(" seconds").withPrecision(2));

		paramGroup->addSlider(new Knob(repo, ReverbEffect::Damp, 	"Damp", 		0.2));
		paramGroup->addSlider(new Knob(repo, ReverbEffect::Width, 	"Stereo Width", 1.0));
		paramGroup->addSlider(new Knob(repo, ReverbEffect::Highpass, "Highpass", 	0.05));
		paramGroup->addSlider(new Knob(repo, ReverbEffect::Wet, 	"Wet Amount", 	0.4));
	}


	String getKnobName(int index) const
	{
		bool little = getWidth() < 300;
		bool superSmall = getWidth() < 200;

		switch(index)
		{
			case ReverbEffect::Size: 	return little 	  ? "sz" : "Size";
			case ReverbEffect::Damp:	return superSmall ? "dmp" : "Damp";
			case ReverbEffect::Width:	return little 	  ? "wdth" : "Width";
			case ReverbEffect::Highpass:return "HP";
			case ReverbEffect::Wet:		return "Wet";
		}

		return String::empty;
	}

	void overrideValueOptionally(int number, double& value)
	{
		// used to be the 'dry' parameter
		if(number == ReverbEffect::Highpass && getObj(Document).getVersionValue() < 1.5)
		{
			value = 0.05;
		}
	}
};

#endif
