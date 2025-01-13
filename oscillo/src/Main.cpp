#include <JuceHeader.h>

#include <memory>
#include "MainComponent.h"
#include "incl/JucePluginDefines.h"

class OscilloscopeApplication : public JUCEApplication {
public:
    OscilloscopeApplication() = default;

    const String getApplicationName() override { return JucePlugin_Name; }
    const String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const String&) override {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
        mainWindow->setVisible(true);
    }

    void shutdown() override { mainWindow = nullptr; }

    class MainWindow : public DocumentWindow {
    public:
        explicit MainWindow(const String& name)
            : DocumentWindow(name, Desktop::getInstance().getDefaultLookAndFeel()
                                                         .findColour(backgroundColourId),
                             allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            Component::setVisible(true);
        }

        void closeButtonPressed() override { getInstance()->systemRequestedQuit(); }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

#ifndef BUILD_TESTING
    START_JUCE_APPLICATION(OscilloscopeApplication)
    JUCE_MAIN_FUNCTION_DEFINITION
#endif