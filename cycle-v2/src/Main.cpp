#include <JuceHeader.h>

#include <memory>

#include "UI/NodeWorkspace.h"
#include "incl/JucePluginDefines.h"

using namespace juce;

class CycleV2Application : public JUCEApplication {
public:
    CycleV2Application() = default;

    const String getApplicationName() override { return JucePlugin_Name; }
    const String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const String&) override {
        Process::makeForegroundProcess();
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
        mainWindow->setVisible(true);
        mainWindow->toFront(true);
    }

    void shutdown() override {
        mainWindow = nullptr;
    }

    class MainWindow : public DocumentWindow {
    public:
        explicit MainWindow(const String& name) :
                DocumentWindow(name, Colour(0xff101318), allButtons) {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setContentOwned(new CycleV2::NodeWorkspace(), true);

            if (const auto* display = Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
                setBounds(display->userArea);
            } else {
                centreWithSize(1280, 760);
            }
        }

        void closeButtonPressed() override {
            JUCEApplicationBase::quit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

#ifndef BUILD_TESTING
    START_JUCE_APPLICATION(CycleV2Application)
    JUCE_MAIN_FUNCTION_DEFINITION
#endif
