
#include <Definitions.h>
#if !PLUGIN_MODE

#include "MainAppWindow.h"

class AppClass : public JUCEApplication {
    std::unique_ptr <MainAppWindow> mainWindow;

public:
    AppClass() = default;

    ~AppClass() override = default;

    void initialise(const String &commandLine) override {
        mainWindow = std::make_unique<MainAppWindow>(commandLine);
        mainWindow->maximiseButtonPressed();
        mainWindow->setVisible(true);
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
        return false;
    }

    void anotherInstanceStarted(const String &commandLine) override {
        mainWindow->openFile(commandLine);
    }
};

START_JUCE_APPLICATION(AppClass)

#endif
