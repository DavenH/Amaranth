#pragma once

#include <App/Doc/Savable.h>
#include <App/SingletonAccessor.h>
#include <Obj/Ref.h>
#include <UI/ParameterGroup.h>
#include <UI/Widgets/IconButton.h>
#include <UI/Widgets/InsetLabel.h>
#include "JuceHeader.h"

#include "../TourGuide.h"
#include "../../Audio/SynthAudioSource.h"
#include "../../Util/CycleEnums.h"

class Effect;

class GuilessEffect :
		public Component
	,	public Button::Listener
	,	public Savable
	,	public TourGuide
	,	public SingletonAccessor
	,	public ParameterGroup::Worker
{
public:
	GuilessEffect(const String& name, const String& displayName, int numParams,
				  SingletonRepo* repo, Effect* effect, int source = UpdateSources::SourceNull);

	~GuilessEffect() override = default;

	void init() override;

	/* UI */
	void paint(Graphics& g) override;
	virtual void layoutKnobs(Rectangle<int> rect, Array<int>& knobIdcs, int knobSize, int knobSpacing);

	/* Updating */
	bool shouldTriggerGlobalUpdate(Slider* slider) override;
	bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate) override;
	void doGlobalUIUpdate(bool force);
	void makeKnobsVisibleAndListen();
	void reduceDetail();
	void restoreDetail();

	/* Events */
	virtual Array<Rectangle<int> > getOutlinableRects();
	virtual Array<int> getApplicableKnobs();
	void resized() override;

	[[nodiscard]] virtual String getKnobName(int index) const;
	Component* getComponent(int which) override;
    void buttonClicked (Button* button) override;
	virtual void effectEnablementChanged(bool sendUIUpdate, bool sendDspUpdate);
	virtual void setExtraTitleElements(Rectangle<int>& r) {}
	virtual void setExtraRightElements(Rectangle<int>& r) {}
	virtual void setEffectEnabled(bool enabled, bool sendUIUpdate, bool sendDspUpdate);

	void writeXML(XmlElement* element) const override;
	bool readXML(const XmlElement* element) override;

	[[nodiscard]] bool isEffectEnabled() const 		{ return enabled; 	}
	[[nodiscard]] const String& getEffectName() const { return name; 		}

protected:
	int fxEnum;
	int updateSource;
	int minTitleSize;
	bool enabled, haveInitialised;

	Ref<Effect> effect;

	Font 		font;
	String 		name;
	InsetLabel 	title;
	IconButton enableButton;
};