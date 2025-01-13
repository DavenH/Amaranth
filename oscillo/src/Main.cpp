#include <JuceHeader.h>
#include <memory>
#include "MainComponent.h"
#include "AppSettings.h"
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

            canSaveResize = true;

            // Load saved window bounds
            auto& settings = AppSettings::getInstance()->getSettings();
            const String keyvals = settings.createXml("asdf")->toString();
            if (settings.containsKey("windowX") && settings.containsKey("windowY") &&
                settings.containsKey("windowW") && settings.containsKey("windowH")) {
                setBounds(settings.getIntValue("windowX"),
                         settings.getIntValue("windowY"),
                         jmax(640, settings.getIntValue("windowW")),
                         jmax(480, settings.getIntValue("windowH")));
            } else {
                centreWithSize(jmax(640, getWidth()), jmax(480, getHeight()));
            }

            Component::setVisible(true);
        }

        void closeButtonPressed() override {
            saveWindowPosition();
            AppSettings::deleteInstance();
            getInstance()->systemRequestedQuit();
        }

        void moved() override {
            DocumentWindow::moved();
            saveWindowPosition();
        }

        void resized() override {
            DocumentWindow::resized();
            if (canSaveResize) {
                saveWindowPosition();
            }
        }

    private:
        bool canSaveResize = false;

        void saveWindowPosition() {
            auto& settings = AppSettings::getInstance()->getSettings();
            settings.setValue("windowX", getX());
            settings.setValue("windowY", getY());
            settings.setValue("windowW", getWidth());
            settings.setValue("windowH", getHeight());
            settings.saveIfNeeded();
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

#ifndef BUILD_TESTING
    START_JUCE_APPLICATION(OscilloscopeApplication)
    JUCE_MAIN_FUNCTION_DEFINITION
#endif