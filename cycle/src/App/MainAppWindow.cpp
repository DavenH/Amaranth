#include "JuceHeader.h"
#include <Definitions.h>

#if ! PLUGIN_MODE

#include <App/SingletonAccessor.h>
#include <App/SingletonRepo.h>
#include "JuceHeader.h"

#include "MainAppWindow.h"
#include "FileManager.h"
#include "Initializer.h"
#include "../CycleDefs.h"
#include "../UI/Panels/MainPanel.h"

MainAppWindow::MainAppWindow(const String& commandLine) :
        DocumentWindow("Cycle", Colours::black, allButtons, true)
    ,	SingletonAccessor(repo, "MainAppWindow") {
    setResizable(platformSplit(true, false, true), false);
    setTitleBarHeight(25);

    initializer = std::make_unique<Initializer>();
    repo = initializer->getSingletonRepo();
    repo->add(this);

    initializer->setCommandLine(commandLine);
    initializer->init();

    setContentNonOwned(&getObj(MainPanel), true);
    setResizeLimits(960, 650, 1920, 1200);
}

void MainAppWindow::openFile(const String &commandLine) {
    initializer->setCommandLine(commandLine);
    getObj(FileManager).openDefaultPreset();
}

void MainAppWindow::closeButtonPressed() {
    getObj(Dialogs).promptForSaveModally([this](bool shouldContinue) {
        if (shouldContinue) {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
    });
}

void MainAppWindow::maximiseButtonPressed() {
    static int presses = 0;
    static const int numResolutions = 4;

    int press = presses % numResolutions;

    Rectangle<int> screenArea = Desktop::getInstance().getDisplays().getMainDisplay().userArea;
    float aspect = screenArea.getHeight() / float(screenArea.getWidth());
    int width = screenArea.getWidth();

    if (press == 0) {
        setFullScreen(true);
    } else {
        int reduction = width * (1 - powf(0.95, press));
        Rectangle<int> smaller = screenArea.reduced(reduction, aspect * reduction);

        setBounds(24, 24, smaller.getWidth(), smaller.getHeight());
    }

    ++presses;
}

void MainAppWindow::handleMessage(const Message &message) {
  #if JUCE_MAC
    if (! getObj(MainPanel).hasKeyboardFocus(false))
    {
        postMessage(new Message());
    }
    getObj(MainPanel).grabKeyboardFocus();
  #endif
}

#endif
