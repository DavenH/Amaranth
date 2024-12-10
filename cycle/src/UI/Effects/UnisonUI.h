#ifndef _unisoneffect_h
#define _unisoneffect_h

#include <App/SingletonAccessor.h>
#include <Design/Updating/Updateable.h>
#include <Obj/Ref.h>

#include "GuilessEffect.h"
#include "../Widgets/Controls/LayerAddRemover.h"
#include "../Widgets/Controls/SelectorPanel.h"
#include "../TourGuide.h"

class Unison;
class Effect;

class UnisonUI :
		public GuilessEffect
	,	public ComboBox::Listener
{
public:
	class VoiceSelector : public SelectorPanel
	{
	public:
		VoiceSelector(SingletonRepo* repo, UnisonUI* panel) :
				SelectorPanel(repo)
			,	panel		(panel)
			,	numVoices	(1)
		{}

		int getSize();
		int getCurrentIndexExternal();
		void selectionChanged();
		void rowClicked(int row);

		Ref<UnisonUI> panel;

		int numVoices;
	};

	UnisonUI(SingletonRepo* repo, Effect* effect);
	virtual ~UnisonUI() {}

	Array<Rectangle<int> > getOutlinableRects();
	Array<int> getApplicableKnobs();
	bool isGroupMode() const;
	int getCurrentIndex() { return voiceSelector.getCurrentIndex(); }
	void modeChanged(bool updateAudio, bool graphicUpdate);
	void orderChangedTo(int order);
	void updateSelection();

	void setExtraTitleElements(Rectangle<int>& r);
	void setExtraRightElements(Rectangle<int>& r);

	void effectEnablementChanged(bool sendUIUpdate, bool sendDspUpdate);
	void comboBoxChanged (ComboBox* box);
	void buttonClicked(Button* button);
	void triggerModeChanged(bool isGroup);

	String getKnobName(int index) const;
	Component* getComponent(int which);

	void init();
	void writeXML(XmlElement* registryElem) const;
	bool readXML(const XmlElement* element);

	enum UniMode
	{
		Group = 1, Single
	};

	friend class VoiceSelector;

private:
	Unison* unison;
	ComboBox modeBox;
	VoiceSelector voiceSelector;
	LayerAddRemover addRemover;
};

#endif
