#pragma once

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
	~BannerPanel() override {}

	void paint(Graphics& g) override;
	void mouseEnter(const MouseEvent& e) override;
	void mouseDown(const MouseEvent& e);
	void timerCallback() override;

private:
	float secondsSinceStart{};

	Image logo;
	MicroTimer timer;

};
