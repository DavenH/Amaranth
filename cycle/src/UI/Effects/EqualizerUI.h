#ifndef _EQUALIZERCOMPONENT_H_
#define _EQUALIZERCOMPONENT_H_

#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>
#include <Obj/Ref.h>

#include "GuilessEffect.h"

class Effect;
class Equalizer;

class EqualizerUI :
		public GuilessEffect
	,	public Updateable
{
private:
	Ref<Equalizer> equalizer;

public:
	EqualizerUI(SingletonRepo* repo, Effect* effect);
	void init();
	bool paramTriggersAggregateUpdate(int knobIndex);
	void layoutKnobs(Rectangle<int> rect, Array<int>& knobIdcs, int knobSize, int knobSpacing);
	String getKnobName(int index) const;
	void finishedUpdatingAllSliders();
	void doLocalUIUpdate();
	void performUpdate(int updateType);
};

#endif

