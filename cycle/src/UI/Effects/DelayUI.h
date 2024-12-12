#pragma once

#include <UI/IConsole.h>

#include "../../Audio/Effects/AudioEffect.h"
#include "GuilessEffect.h"

class Effect;

class DelayUI :
	public GuilessEffect
{
public:
	DelayUI(SingletonRepo* repo, Effect* effect);
	String getKnobName(int index) const override;

};