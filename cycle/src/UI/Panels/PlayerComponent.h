#ifndef PLAYERCOMPONENT_H_
#define PLAYERCOMPONENT_H_

#include <Obj/Ref.h>
#include <UI/Widgets/IconButton.h>
#include <App/SingletonAccessor.h>

#include "../Widgets/MidiKeyboard.h"

class ResizerPullout;
class PulloutComponent;
class RetractableCallout;

class PlayerComponent :
		public Component
	,	public Button::Listener
	,	public SingletonAccessor
{
public:
	PlayerComponent(SingletonRepo* repo);
	~PlayerComponent();
	void init();
	void setComponents(bool add);
	void buttonClicked(Button* button);
	void paint(Graphics& g);
	void resized();

	void updateTitle();

private:
	IconButton 			prevIcon;
	IconButton 			nextIcon;
	IconButton 			tableIcon;

	Ref<MidiKeyboard> 	keyboard;
	Ref<ResizerPullout> pullout;

	String				name;
	String				author;

	JUCE_LEAK_DETECTOR(PlayerComponent)
};


#endif
