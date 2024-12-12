#pragma once
#include <Obj/Ref.h>

#include "../../UI/Effects/GuilessEffect.h"
#include "../../Audio/Effects/AudioEffect.h"
#include "../../Audio/Effects/Phaser.h"

class SingletonRepo;

class PhaserUI :
	public GuilessEffect
{
	Ref<Effect> phaser;

public:
	explicit PhaserUI(SingletonRepo* repo) :
		GuilessEffect("PhaserUI", "Phaser", Phaser::numPhaserParams, repo, phaser, UpdateSources::SourceNull)
	{
	}

	[[nodiscard]] String getKnobName(int index) const override
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
			default:
				throw std::out_of_range("PhaserUI::getKnobName");
		}
	}
};