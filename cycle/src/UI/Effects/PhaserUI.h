#ifndef _phasercomponent_h
#define _phasercomponent_h

#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>

#include "../../UI/Effects/GuilessEffect.h"
#include "../../Audio/Effects/AudioEffect.h"
#include "../../Audio/Effects/Phaser.h"
#include "../../Audio/SynthAudioSource.h"

class SingletonRepo;

class PhaserUI :
	public GuilessEffect
{
private:
	Ref<Effect> phaser;

public:
	PhaserUI(SingletonRepo* repo) :
		GuilessEffect("PhaserUI", "Phaser", Phaser::numPhaserParams, repo, phaser, UpdateSources::SourceNull)
	{
	}

	String getKnobName(int index) const
	{
		bool little 	= getWidth() < 300;
		bool superSmall = getWidth() < 200;

		switch(index)
		{
			case Phaser::Rate: 		return superSmall ? "rt" : "Rate";
			case Phaser::Feedback:	return little ? "fb" : "Feedback";
			case Phaser::Depth:		return little ? "dpth" : "Depth";
			case Phaser::Min:		return superSmall ? "mn" : "Min";
			case Phaser::Max:		return superSmall ? "mx" : "Max";
		}

		return String::empty;
	}

};

#endif
