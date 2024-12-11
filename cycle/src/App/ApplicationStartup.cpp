#include <Definitions.h>

#if !PLUGIN_MODE

#include "MainAppWindow.h"

namespace ProjectInfo
{
    const char* const  projectName    = "Cycle";
    const char* const  companyName    = "Amaranth Audio";
    const char* const  versionString  = "1.9.0";
    const int          versionNumber  = 0x10000;
}


class AppClass : public JUCEApplication {
    std::unique_ptr <MainAppWindow> mainWindow;

public:
    AppClass()
            : mainWindow(0) {
    }

    ~AppClass() {
    }

    void initialise(const String &commandLine) {
        mainWindow = new MainAppWindow(commandLine);
        mainWindow->maximiseButtonPressed();
        mainWindow->setVisible(true);
    }

    void shutdown() {
        mainWindow = 0;
    }

    const String getApplicationName() {
        return ProjectInfo::projectName;
    }

    const String getApplicationVersion() {
        return String(ProjectInfo::versionString);
    }

    bool moreThanOneInstanceAllowed() {
        return false;
    }

    void anotherInstanceStarted(const String &commandLine) {
        mainWindow->openFile(commandLine);
    }
};

START_JUCE_APPLICATION(AppClass)

#endif
