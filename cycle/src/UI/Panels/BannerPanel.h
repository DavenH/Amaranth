#ifndef _bannerpanel_h
#define _bannerpanel_h

#include <Definitions.h>
#include <App/Settings.h>
#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include <App/Doc/Document.h>
#include <Binary/Images.h>
#include <UI/IConsole.h>
#include <UI/MiscGraphics.h>
#include "JuceHeader.h"
#include <Util/MicroTimer.h>
#include <Util/Util.h>

#include <ippdefs.h>

class BannerPanel:
		public Component
	, 	public SingletonAccessor
	,	public Timer {
public:
	BannerPanel(SingletonRepo* repo);
	~BannerPanel() {}

	void paint(Graphics& g);
	void mouseEnter(const MouseEvent& e);
	void mouseDown(const MouseEvent& e);
	void timerCallback();

private:
	float secondsSinceStart;

	Image logo;
	MicroTimer timer;

};
#endif
