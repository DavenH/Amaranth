#include "JuceHeader.h"
#include <Definitions.h>

#if PLUGIN_MODE

#include <App/SingletonRepo.h>
#include <App/Settings.h>
#include <Audio/PluginProcessor.h>
#include <Design/Updating/Updater.h>
#include <Util/Util.h>
#include <Util/ScopedBooleanSwitcher.h>

#include "PluginWindow.h"

#include <App/Dialogs.h>
#include <Vst/PluginWindow.h>

#include "VisualDsp.h"
#include "SynthLookAndFeel.h"

#include "Panels/MainPanel.h"
#include "Panels/PlayerComponent.h"
#include "../Util/CycleEnums.h"
#include "../Incl/JucePluginDefines.h"
#include "../CycleDefs.h"

using namespace juce;

PluginWindow::PluginWindow (PluginProcessor* proc) :
        SingletonAccessor(proc->repo, "PluginWindow")
    ,	AudioProcessorEditor (proc)
    ,	doUpdateAfterResize(false)
    ,	haveFreedResources(false)
{
    info("Created plugin window\n");

    setLookAndFeel(&getObj(SynthLookAndFeel));
    changeSizeAndSet(getSetting(WindowSize));

    Ref mainPanel = &getObj(MainPanel);

    using namespace WindowSizes;

    if (getSetting(WindowSize) != PlayerSize) {
        addAndMakeVisible(mainPanel);
        mainPanel->grabKeyboardFocus();

        if (haveFreedResources) {
            info("have freed UI, so activating contexts\n");
            // TODO, this is an opengl thing -- not sure if still needed
            // repo->activateContexts();

          #ifndef JUCE_MAC
            doUpdate(SourceMorph);
          #endif
        }

      #ifdef JUCE_MAC
        mainPanel->triggerDelayedRepaint();
      #endif
    }
}

PluginWindow::~PluginWindow() {
    freeUIResources();
    resizer = nullptr;
}

void PluginWindow::paint(Graphics& g) {
}

void PluginWindow::resized() {
    ScopedBooleanSwitcher sbs(getObj(MainPanel).getForceResizeFlag());
    auto& mp = getObj(MainPanel);

    if (doUpdateAfterResize)	{
        doUpdateAfterResize = false;
        mp.setAttachNextResize(true);
        addAndMakeVisible(&mp);

      #ifndef JUCE_MAC
        doUpdate(SourceMorph);
      #endif
    }

    mp.triggerDelayedRepaint();

    bool willResize = mp.getWidth() != getWidth() || mp.getHeight() != getHeight();

    (getSetting(WindowSize) == AppSettings::PlayerSize) ?
        getObj(PlayerComponent).setBounds(0, 0, getWidth(), getHeight()) :
        mp.setBounds(0, 0, getWidth(), getHeight());

    if (! willResize) {
        mp.resized();
    }
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
    Ref mainPanel 				= &getObj(MainPanel);
    Ref playerComponent 	= &getObj(PlayerComponent);

    int oldSize = getSetting(WindowSize);
    getSetting(WindowSize) = sizeEnum;

    int width, height;
    Dialogs::getSizeFromSetting(sizeEnum, width, height);

    doUpdateAfterResize = false;

    if (sizeEnum == AppSettings::PlayerSize) {
        removeChildComponent(mainPanel);
        playerComponent->setComponents(true);
        playerComponent->resized();

        addAndMakeVisible(playerComponent);
        freeUIResources();
    }

    else if (oldSize == AppSettings::PlayerSize) {
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
