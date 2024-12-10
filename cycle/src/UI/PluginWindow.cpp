#include <Definitions.h>

#if PLUGIN_MODE

#include <App/SingletonRepo.h>
#include <App/Settings.h>
#include <Audio/PluginProcessor.h>
#include <Design/Updating/Updater.h>
#include <Util/Util.h>
#include <Util/ScopedBooleanSwitcher.h>

#include "PluginWindow.h"
#include "VisualDsp.h"
#include "SynthLookAndFeel.h"

#include "Panels/MainPanel.h"
#include "Panels/PlayerComponent.h"
#include "../App/GlobalOperations.h"
#include "../Util/CycleEnums.h"


PluginWindow::PluginWindow (PluginProcessor* proc) :
		SingletonAccessor(proc->repo, "PluginWindow")
	,	AudioProcessorEditor (proc)
	,	doUpdateAfterResize(false)
	,	haveFreedResources(false)
{
	dout << "Created plugin window\n";

	setLookAndFeel(&getObj(SynthLookAndFeel));
	changeSizeAndSet(getSetting(WindowSize));

	Ref<MainPanel> mainPanel = &getObj(MainPanel);

	using namespace WindowSizes;

	if(getSetting(WindowSize) != PlayerSize)
	{
		addAndMakeVisible(mainPanel);
		mainPanel->grabKeyboardFocus();

		if(haveFreedResources)
		{
			dout << "have freed UI, so activating contexts\n";
			repo->activateContexts();

		  #ifndef JUCE_MAC
			doUpdate(SourceMorph);
		  #endif
		}

	  #ifdef JUCE_MAC
		mainPanel->triggerDelayedRepaint();
	  #endif
	}
}


PluginWindow::~PluginWindow()
{
	freeUIResources();
	resizer = nullptr;
}


void PluginWindow::paint (Graphics& g)
{
}


void PluginWindow::resized()
{
	ScopedBooleanSwitcher sbs(getObj(MainPanel).getForceResizeFlag());
	MainPanel& mp = getObj(MainPanel);

	if(doUpdateAfterResize)
	{
		doUpdateAfterResize = false;
		mp.setAttachNextResize(true);
		addAndMakeVisible(&mp);

	  #ifndef JUCE_MAC
		doUpdate(ModUpdate);
	  #endif
	}

	mp.triggerDelayedRepaint();

	bool willResize = mp.getWidth() != getWidth() || mp.getHeight() != getHeight();

	(getSetting(WindowSize) == Settings::PlayerSize) ?
		getObj(PlayerComponent).setBounds(0, 0, getWidth(), getHeight()) :
		mp.setBounds(0, 0, getWidth(), getHeight());

	if(! willResize)
		mp.resized();
}


void PluginWindow::focusLost (FocusChangeType cause)
{
}


void PluginWindow::focusGained (FocusChangeType cause)
{
	jassertfalse;
}


void PluginWindow::changeSizeAndSet(int sizeEnum)
{
	Ref<MainPanel> mainPanel 				= &getObj(MainPanel);
	Ref<PlayerComponent> playerComponent 	= &getObj(PlayerComponent);

	int oldSize = getSetting(WindowSize);
	getSetting(WindowSize) = sizeEnum;

	int width, height;
	GlobalOperations::getSizeFromSetting(sizeEnum, width, height);

	doUpdateAfterResize = false;

	if(sizeEnum == Settings::PlayerSize)
	{
		removeChildComponent(mainPanel);
		playerComponent->setComponents(true);
		playerComponent->resized();

		addAndMakeVisible(playerComponent);
		freeUIResources();
	}

	else if(oldSize == Settings::PlayerSize)
	{
		playerComponent->setComponents(false);
		removeChildComponent(playerComponent);

		mainPanel->setPlayerComponents();
		getObj(IConsole).addPullout();
		getObj(IConsole).resized();

		doUpdateAfterResize = true;
	}

	setSize(width, height);
}


void PluginWindow::freeUIResources()
{

}


void PluginWindow::moved()
{
}

#endif
