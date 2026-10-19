
#include "JuceHeader.h"
#include <Definitions.h>
#if !PLUGIN_MODE

#include "MainAppWindow.h"
#include "../CycleDefs.h"

class AppClass : public JUCEApplication {
    std::unique_ptr <MainAppWindow> mainWindow;

public:
    void initialise(const String &commandLine) override {
        DBG("AppClass::initialise commandLine=\"" + commandLine + "\"");
        mainWindow = std::make_unique<MainAppWindow>(commandLine);
#if JUCE_LINUX
        mainWindow->setUsingNativeTitleBar(true);
#endif
        if (const auto* primaryDisplay = Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
            auto bounds = primaryDisplay->totalArea;
            bounds.setSize(mainWindow->getWidth(), mainWindow->getHeight());
            bounds.setCentre(primaryDisplay->totalArea.getCentre());
            mainWindow->setBounds(bounds);
        }

        mainWindow->setVisible(true);
        Component::SafePointer<MainAppWindow> safeWindow(mainWindow.get());

        MessageManager::callAsync([safeWindow, commandLine]() {
            if (safeWindow != nullptr) {
                if (const auto* primaryDisplay = Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
                    auto bounds = primaryDisplay->totalArea;
                    bounds.setSize(safeWindow->getWidth(), safeWindow->getHeight());
                    bounds.setCentre(primaryDisplay->totalArea.getCentre());
                    safeWindow->setBounds(bounds);
                }

                DBG("AppClass::initialise async startup preset open");
                safeWindow->openFile(commandLine);
            }
        });

    }

    void shutdown() override {
        mainWindow = nullptr;
    }

    const String getApplicationName() override {
        return ProjectInfo::projectName;
    }

    const String getApplicationVersion() override {
        return String(ProjectInfo::versionString);
    }

    bool moreThanOneInstanceAllowed() override {
        const auto arguments = getCommandLineParameters();
        return arguments.contains("--agent-session") || arguments.contains("--agent-script");
    }

    void anotherInstanceStarted(const String &commandLine) override {
        mainWindow->openFile(commandLine);
    }
};

  #ifndef BUILD_TESTING
    START_JUCE_APPLICATION(AppClass)
    JUCE_MAIN_FUNCTION_DEFINITION
  #endif

#endif
