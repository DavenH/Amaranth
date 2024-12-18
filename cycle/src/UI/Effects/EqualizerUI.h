#pragma once

#include <Design/Updating/Updateable.h>
#include <Obj/Ref.h>

#include "GuilessEffect.h"

class Effect;
class Equalizer;

class EqualizerUI :
		public GuilessEffect
	,	public Updateable
{
	Ref<Equalizer> equalizer;

public:
	EqualizerUI(SingletonRepo* repo, Effect* effect);
	void init() override;
	bool paramTriggersAggregateUpdate(int knobIndex);
	void layoutKnobs(Rectangle<int> rect, Array<int>& knobIdcs, int knobSize, int knobSpacing) override;
	[[nodiscard]] String getKnobName(int index) const override;
	void finishedUpdatingAllSliders() override;
	void doLocalUIUpdate() override;
	void performUpdate(UpdateType updateType) override;
};