#pragma once

#include <App/EditWatcher.h>
#include <App/Doc/Document.h>
#include <App/SingletonRepo.h>
#include <UI/Widgets/Knob.h>

#include "GuilessEffect.h"
#include "../../Audio/Effects/AudioEffect.h"
#include "../../Audio/Effects/Reverb.h"
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

		using namespace Ops;
		StringFunction sizeShort = StringFunction().mul(6.f).add(12.f).pow(2.f).mul(1.f / 44100.f);
		size->setStringFunctions(sizeShort, sizeShort.withPostString(" seconds").withPrecision(2));

		paramGroup->addSlider(new Knob(repo, ReverbEffect::Damp, 	 "Damp", 		 0.2));
		paramGroup->addSlider(new Knob(repo, ReverbEffect::Width, 	 "Stereo Width", 1.0));
		paramGroup->addSlider(new Knob(repo, ReverbEffect::Highpass, "Highpass", 	 0.05));
		paramGroup->addSlider(new Knob(repo, ReverbEffect::Wet, 	 "Wet Amount", 	 0.4));
	}

	[[nodiscard]] String getKnobName(int index) const override {
		bool little = getWidth() < 300;
		bool superSmall = getWidth() < 200;

		switch (index) {
			case ReverbEffect::Size: 	return little 	  ? "sz" : "Size";
			case ReverbEffect::Damp:	return superSmall ? "dmp" : "Damp";
			case ReverbEffect::Width:	return little 	  ? "wdth" : "Width";
			case ReverbEffect::Highpass:return "HP";
			case ReverbEffect::Wet:		return "Wet";
			default: throw std::out_of_range("ReverbUI::getKnobName");
		}
	}

	void overrideValueOptionally(int number, double& value) override {
		// used to be the 'dry' parameter
		if (number == ReverbEffect::Highpass && getObj(Document).getVersionValue() < 1.5) {
			value = 0.05;
		}
	}
};
