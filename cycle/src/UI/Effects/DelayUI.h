#ifndef _delaycomponent_h
#define _delaycomponent_h

#include <Obj/Ref.h>
#include <UI/IConsole.h>
#include <Util/StringFunction.h>

#include "../../Audio/Effects/AudioEffect.h"
#include "GuilessEffect.h"

class Effect;

class DelayUI :
	public GuilessEffect
{
public:
	DelayUI(SingletonRepo* repo, Effect* effect);
	String getKnobName(int index) const;

private:
};

#endif
