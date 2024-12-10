#ifndef RESIZERPULLOUT_H_
#define RESIZERPULLOUT_H_

#include <App/SingletonAccessor.h>
#include <Definitions.h>
#include <Obj/Ref.h>
#include <UI/Widgets/PulloutComponent.h>
#include "JuceHeader.h"

class IconButton;

class ResizerPullout :
		public Button::Listener
	,	public Component
	, 	public SingletonAccessor
{
public:
	ResizerPullout(SingletonRepo* repo);

	void buttonClicked(Button* button);
	void resized();
	void updateHighlight(int windowSize);

private:
	ScopedPointer<PulloutComponent> pullout;

	ScopedPointer<IconButton> smallIcon;
	ScopedPointer<IconButton> medIcon;
	ScopedPointer<IconButton> fullIcon;
	ScopedPointer<IconButton> keybIcon;

    JUCE_LEAK_DETECTOR(ResizerPullout)
};


#endif
