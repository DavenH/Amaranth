#ifndef _console_h
#define _console_h

#include <string>
#include <time.h>

#include <App/SingletonAccessor.h>
#include <Definitions.h>
#include <Obj/Ref.h>
#include <UI/IConsole.h>
#include "JuceHeader.h"

#include "../TourGuide.h"

using std::string;

class IconButton;
class ResizerPullout;


class Console:
		public Component
	,	public Timer
	,	public IConsole {
public:
	Console(SingletonRepo* repo);
	virtual ~Console();

	void timerCallback();
	void paint(Graphics& g);
	void setKeys(const String& keys);
	void write(const String& str, int priority);
	void setMouseUsage(const MouseUsage& usage);
	void setMouseUsage(bool left, bool scroll, bool middle, bool right);

	void init();
	void reset();
	void resized();
	void addPullout();

private:
	bool 	firstCallback;
	bool 	fading;
	int 	currentPriority;
	float 	opacity;

	String 	text;
	String 	shortcut;
	String 	keys;
	Image 	mouseparts;
	MouseUsage usage;

	Ref<ResizerPullout> pullout;
};

#endif
