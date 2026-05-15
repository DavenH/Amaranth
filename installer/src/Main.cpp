#include <JuceHeader.h>

#include "UI/AmaranthLookAndFeel.h"
#include "InstallerComponent.h"
#include "InstallerManifest.h"
#include "incl/JucePluginDefines.h"

using namespace juce;

class InstallerApplication : public JUCEApplication {
public:
    InstallerApplication() = default;

    const String getApplicationName() override { return JucePlugin_Name; }
    const String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const String& commandLine) override {
        lookAndFeel = std::make_unique<AmaranthLookAndFeel>(nullptr);
        LookAndFeel::setDefaultLookAndFeel(lookAndFeel.get());

        InstallerManifest manifest;
        const File manifestFile = getManifestFile(commandLine);
        const Result result = manifest.loadFromFile(manifestFile);
        if (result.failed()) {
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                             "Installer manifest error",
                                             result.getErrorMessage(),
                                             "Quit",
                                             nullptr,
                                             ModalCallbackFunction::create([](int) {
                                                 JUCEApplication::getInstance()->systemRequestedQuit();
                                             }));
            return;
        }

        mainWindow = std::make_unique<MainWindow>(manifest.getProductName() + " Installer", std::move(manifest));
        mainWindow->setVisible(true);
    }

    void shutdown() override {
        mainWindow = nullptr;
        LookAndFeel::setDefaultLookAndFeel(nullptr);
        lookAndFeel = nullptr;
    }

private:
    class MainWindow : public DocumentWindow {
    public:
        MainWindow(const String& name, InstallerManifest manifest) :
                DocumentWindow(name, Colour(0xff101113), DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setContentOwned(new InstallerComponent(std::move(manifest)), true);
            centreWithSize(860, 620);
        }

        void closeButtonPressed() override {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    File getManifestFile(const String& commandLine) const {
        const StringArray arguments = StringArray::fromTokens(commandLine, true);
        if (! arguments.isEmpty()) {
            const File argumentFile(arguments[0]);
            if (File::isAbsolutePath(arguments[0]) || argumentFile.existsAsFile()) {
                return argumentFile;
            }

            const File sourceRelative = File(AMARANTH_SOURCE_DIR).getChildFile(arguments[0]);
            if (sourceRelative.existsAsFile()) {
                return sourceRelative;
            }

            const File configRelative = File(INSTALLER_DEFAULT_MANIFEST)
                    .getParentDirectory()
                    .getChildFile(argumentFile.getFileName());
            if (configRelative.existsAsFile()) {
                return configRelative;
            }

            const File productManifest = File(AMARANTH_SOURCE_DIR)
                    .getChildFile(argumentFile.getFileNameWithoutExtension())
                    .getChildFile("installer.json");
            if (productManifest.existsAsFile()) {
                return productManifest;
            }

            return argumentFile;
        }

        return File(INSTALLER_DEFAULT_MANIFEST);
    }

    std::unique_ptr<AmaranthLookAndFeel> lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(InstallerApplication)

#if JucePlugin_Build_Standalone && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP
JUCE_MAIN_FUNCTION_DEFINITION
#endif
