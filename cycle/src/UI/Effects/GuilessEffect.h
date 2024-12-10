#ifndef _guilesseffect_h
#define _guilesseffect_h

#include <App/Doc/Savable.h>
#include <App/SingletonAccessor.h>
#include <Design/Updating/Updater.h>
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

	virtual ~GuilessEffect() {}
	virtual void init();

	/* UI */
	virtual void paint(Graphics& g);
	virtual void layoutKnobs(Rectangle<int> rect, Array<int>& knobIdcs, int knobSize, int knobSpacing);

	/* Updating */
	bool shouldTriggerGlobalUpdate(Slider* slider);
	bool updateDsp(int knobIndex, double knobValue, bool doFurtherUpdate);
	void doGlobalUIUpdate(bool force);
	void makeKnobsVisibleAndListen();
	void reduceDetail();
	void restoreDetail();

	/* Events */
	virtual Array<Rectangle<int> > getOutlinableRects();
	virtual Array<int> getApplicableKnobs();
	virtual void resized();

	virtual String getKnobName(int index) const;
	virtual Component* getComponent(int which);
    virtual void buttonClicked (Button* button);
	virtual void effectEnablementChanged(bool sendUIUpdate, bool sendDspUpdate);
	virtual void setExtraTitleElements(Rectangle<int>& r) {}
	virtual void setExtraRightElements(Rectangle<int>& r) {}
	virtual void setEffectEnabled(bool enabled, bool sendUIUpdate, bool sendDspUpdate);

	virtual void writeXML(XmlElement* element) const;
	virtual bool readXML(const XmlElement* element);

	bool isEffectEnabled() const 		{ return enabled; 	}
	const String& getEffectName() const { return name; 		}

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

#endif
