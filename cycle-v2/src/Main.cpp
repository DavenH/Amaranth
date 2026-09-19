#include <JuceHeader.h>

#include <memory>
#include <utility>
#include <vector>

#include "App/CycleV2Automation.h"
#include "App/GraphFileHistory.h"
#include "App/StandaloneAudioEngine.h"
#include "UI/NodeWorkspace.h"
#include "UI/PresetBrowserPage.h"
#include "incl/JucePluginDefines.h"

using namespace juce;

class CycleV2Application : public JUCEApplication {
public:
    CycleV2Application() = default;

    const String getApplicationName() override { return JucePlugin_Name; }
    const String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const String& commandLine) override {
        Process::makeForegroundProcess();
        automationOptions = CycleV2::CycleV2Automation::parseCommandLine(commandLine);
        audioEngine = std::make_unique<CycleV2::StandaloneAudioEngine>();
        audioEngine->start();
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), *audioEngine);
        mainWindow->setVisible(true);
        mainWindow->toFront(true);

        if (CycleV2::CycleV2Automation::hasAutomation(automationOptions)) {
            mainWindow->runAutomation(automationOptions);
        }
    }

    void shutdown() override {
        mainWindow = nullptr;
        audioEngine = nullptr;
    }

    class MainWindow :
            public DocumentWindow
        ,   public MenuBarModel
        ,   public ApplicationCommandTarget {
    public:
        enum Command : CommandID {
            CommandOpenGraph = 0x3000,
            CommandSaveGraph,
            CommandSaveGraphAs,
            CommandSpyRefreshOnRelease,
            CommandSpyRefreshLive
        };

        MainWindow(
                const String& name,
                CycleV2::StandaloneAudioEngine& audioEngine) :
                DocumentWindow(name, Colour(0xff101318), allButtons)
            ,   applicationName(name)
            ,   properties(createProperties())
            ,   fileHistory(*properties) {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            workspace = new CycleV2::NodeWorkspace(audioEngine);
            setContentOwned(workspace, true);
            workspace->setGraphDocumentStateChangedCallback([this] {
                updateDocumentPresentation();
            });

            commandManager.registerAllCommandsForTarget(this);
            addKeyListener(commandManager.getKeyMappings());
            setApplicationCommandManagerToWatch(&commandManager);
          #if JUCE_MAC
            MenuBarModel::setMacMainMenu(this);
          #else
            setMenuBar(this);
          #endif

            if (const auto* display = Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
                setBounds(display->userArea);
            } else {
                centreWithSize(1280, 760);
            }
            updateDocumentPresentation();
        }

        ~MainWindow() override {
            if (workspace != nullptr) {
                workspace->setGraphDocumentStateChangedCallback({});
            }
          #if JUCE_MAC
            if (MenuBarModel::getMacMainMenu() == this) {
                MenuBarModel::setMacMainMenu(nullptr);
            }
          #else
            setMenuBar(nullptr);
          #endif
            removeKeyListener(commandManager.getKeyMappings());
        }

        void closeButtonPressed() override {
            JUCEApplicationBase::quit();
        }

        void runAutomation(CycleV2::CycleV2Automation::Options options) {
            if (workspace == nullptr) {
                return;
            }

            automation = std::make_unique<CycleV2::CycleV2Automation>(*workspace, *this, std::move(options));
            automation->runScriptAsync();
        }

        StringArray getMenuBarNames() override {
            return { "File" };
        }

        PopupMenu getMenuForIndex(int menuIndex, const String&) override {
            PopupMenu menu;

            if (menuIndex == 0) {
                menu.addCommandItem(&commandManager, CommandOpenGraph);

                PopupMenu recentMenu;
                if (fileHistory.addRecentMenuItems(recentMenu) > 0) {
                    menu.addSubMenu("Open Recent", recentMenu);
                }

                menu.addSeparator();
                menu.addCommandItem(&commandManager, CommandSaveGraph);
                menu.addCommandItem(&commandManager, CommandSaveGraphAs);

                PopupMenu spyRefreshMenu;
                spyRefreshMenu.addCommandItem(
                        &commandManager,
                        CommandSpyRefreshOnRelease);
                spyRefreshMenu.addCommandItem(
                        &commandManager,
                        CommandSpyRefreshLive);
                menu.addSeparator();
                menu.addSubMenu("Spy Refresh", spyRefreshMenu);
            }

            return menu;
        }

        void menuItemSelected(int menuItemId, int) override {
            if (fileHistory.isRecentMenuItem(menuItemId)) {
                openGraphFile(fileHistory.fileForMenuItem(menuItemId));
            }
        }

        ApplicationCommandTarget* getNextCommandTarget() override {
            return nullptr;
        }

        void getAllCommands(Array<CommandID>& commands) override {
            commands.addArray({
                    CommandOpenGraph,
                    CommandSaveGraph,
                    CommandSaveGraphAs,
                    CommandSpyRefreshOnRelease,
                    CommandSpyRefreshLive
            });
        }

        void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) override {
            switch (commandID) {
                case CommandOpenGraph:
                    result.setInfo("Open Preset...", "Open a Cycle V2 preset", "File", 0);
                    result.addDefaultKeypress('o', ModifierKeys::commandModifier);
                    break;

                case CommandSaveGraph:
                    result.setInfo("Save Preset", "Save the current Cycle V2 preset", "File", 0);
                    result.addDefaultKeypress('s', ModifierKeys::commandModifier);
                    result.setActive(workspace != nullptr && workspace->isGraphDirty());
                    break;

                case CommandSaveGraphAs:
                    result.setInfo("Save Preset As...", "Save the current Cycle V2 preset to a new file", "File", 0);
                    result.addDefaultKeypress('s', ModifierKeys::commandModifier | ModifierKeys::shiftModifier);
                    break;

                case CommandSpyRefreshOnRelease:
                    result.setInfo(
                            "On Release",
                            "Refresh signal spies when an editing gesture finishes",
                            "File",
                            0);
                    result.setTicked(workspace != nullptr
                            && workspace->probeRefreshMode()
                                    == CycleV2::ProbeRefreshMode::OnGestureCommit);
                    break;

                case CommandSpyRefreshLive:
                    result.setInfo(
                            "Live",
                            "Refresh signal spies during editing gestures",
                            "File",
                            0);
                    result.setTicked(workspace != nullptr
                            && workspace->probeRefreshMode()
                                    == CycleV2::ProbeRefreshMode::LiveLatest);
                    break;

                default:
                    break;
            }
        }

        bool perform(const InvocationInfo& info) override {
            switch (info.commandID) {
                case CommandOpenGraph:
                    chooseOpenGraph();
                    return true;

                case CommandSaveGraph:
                    saveGraph();
                    return true;

                case CommandSaveGraphAs:
                    chooseSaveGraphAs();
                    return true;

                case CommandSpyRefreshOnRelease:
                    setSpyRefreshMode(CycleV2::ProbeRefreshMode::OnGestureCommit);
                    return true;

                case CommandSpyRefreshLive:
                    setSpyRefreshMode(CycleV2::ProbeRefreshMode::LiveLatest);
                    return true;

                default:
                    return false;
            }
        }

    private:
        static std::unique_ptr<PropertiesFile> createProperties() {
            PropertiesFile::Options options;
            options.applicationName = "CycleV2";
            options.folderName = "Amaranth Audio/Cycle V2";
            options.filenameSuffix = ".settings";
            options.osxLibrarySubFolder = "Application Support";
            options.storageFormat = PropertiesFile::storeAsXML;
            return std::make_unique<PropertiesFile>(options);
        }

        File repositoryPresetDirectory() const {
          #if defined(CYCLE_V2_SOURCE_DIR)
            return File(CYCLE_V2_SOURCE_DIR).getChildFile("content").getChildFile("presets");
          #else
            return {};
          #endif
        }

        File defaultGraphDirectory() const {
            File fallback = repositoryPresetDirectory();

            if (!fallback.isDirectory()) {
                fallback = File::getSpecialLocation(File::userDocumentsDirectory);
            }

            return fileHistory.initialOpenDirectory(fallback);
        }

        bool openGraphFile(const File& file) {
            if (file == File() || workspace == nullptr
                    || !workspace->loadGraphFromFile(file)) {
                return false;
            }

            currentGraphFile = file;
            fileHistory.recordOpened(file);
            updateDocumentPresentation();
            return true;
        }

        bool saveGraphFile(const File& file) {
            if (file == File() || workspace == nullptr
                    || !workspace->saveGraphToFile(file)) {
                return false;
            }

            currentGraphFile = file;
            fileHistory.recordSaved(file);
            updateDocumentPresentation();
            return true;
        }

        void updateDocumentPresentation() {
            if (workspace == nullptr) {
                return;
            }

            const File file = workspace->graphFile();
            const String presetName = file == File()
                    ? "Untitled"
                    : file.getFileNameWithoutExtension();
            setName(applicationName + " - " + presetName
                    + (workspace->isGraphDirty() ? "*" : ""));
            commandManager.commandStatusChanged();
            menuItemsChanged();
        }

        void setSpyRefreshMode(CycleV2::ProbeRefreshMode mode) {
            if (workspace == nullptr) {
                return;
            }
            workspace->setProbeRefreshMode(mode);
            commandManager.commandStatusChanged();
            menuItemsChanged();
        }

        void chooseOpenGraph() {
            if (presetBrowserWindow != nullptr) {
                presetBrowserWindow->toFront(true);
                return;
            }

            auto* page = new CycleV2::PresetBrowserPage(
                    std::vector<File> { repositoryPresetDirectory(), defaultGraphDirectory() },
                    [safeThis = SafePointer<MainWindow>(this)](const File& file) {
                        return safeThis != nullptr && safeThis->openGraphFile(file);
                    },
                    [safeThis = SafePointer<MainWindow>(this)] {
                        if (safeThis != nullptr) {
                            safeThis->chooseOpenGraphFile();
                            safeThis->closePresetBrowser();
                        }
                    },
                    [safeThis = SafePointer<MainWindow>(this)] {
                        if (safeThis != nullptr) {
                            safeThis->closePresetBrowser();
                        }
                    });
            page->setSize(900, 660);
            DialogWindow::LaunchOptions options;
            options.dialogTitle = "Preset Browser";
            options.dialogBackgroundColour = Colour(0xff111922);
            options.content.setOwned(page);
            options.componentToCentreAround = this;
            presetBrowserWindow = options.launchAsync();
        }

        void closePresetBrowser() {
            if (presetBrowserWindow != nullptr) {
                presetBrowserWindow->exitModalState(0);
                presetBrowserWindow = nullptr;
            }
        }

        void chooseOpenGraphFile() {
            fileChooser = std::make_unique<FileChooser>(
                    "Open Cycle V2 preset",
                    defaultGraphDirectory(),
                    "*.cyclegraph");

            fileChooser->launchAsync(
                    FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                    [safeThis = SafePointer<MainWindow>(this)](const FileChooser& chooser) {
                        if (safeThis == nullptr) {
                            return;
                        }

                        const File file = chooser.getResult();
                        safeThis->openGraphFile(file);

                        safeThis->fileChooser = nullptr;
                    });
        }

        void saveGraph() {
            if (currentGraphFile == File()) {
                chooseSaveGraphAs();
                return;
            }

            saveGraphFile(currentGraphFile);
        }

        void chooseSaveGraphAs() {
            const File initialFile = currentGraphFile == File()
                    ? defaultGraphDirectory().getChildFile("Untitled.cyclegraph")
                    : currentGraphFile;
            fileChooser = std::make_unique<FileChooser>(
                    "Save Cycle V2 preset",
                    initialFile,
                    "*.cyclegraph");

            fileChooser->launchAsync(
                    FileBrowserComponent::saveMode
                            | FileBrowserComponent::canSelectFiles
                            | FileBrowserComponent::warnAboutOverwriting,
                    [safeThis = SafePointer<MainWindow>(this)](const FileChooser& chooser) {
                        if (safeThis == nullptr) {
                            return;
                        }

                        File file = chooser.getResult();
                        if (file != File()) {
                            if (file.getFileExtension().isEmpty()) {
                                file = file.withFileExtension("cyclegraph");
                            }

                            safeThis->saveGraphFile(file);
                        }

                        safeThis->fileChooser = nullptr;
                    });
        }

        const String applicationName;
        ApplicationCommandManager commandManager;
        std::unique_ptr<PropertiesFile> properties;
        CycleV2::GraphFileHistory fileHistory;
        CycleV2::NodeWorkspace* workspace {};
        std::unique_ptr<CycleV2::CycleV2Automation> automation;
        std::unique_ptr<FileChooser> fileChooser;
        SafePointer<DialogWindow> presetBrowserWindow;
        File currentGraphFile;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    CycleV2::CycleV2Automation::Options automationOptions;
    std::unique_ptr<CycleV2::StandaloneAudioEngine> audioEngine;
    std::unique_ptr<MainWindow> mainWindow;
};

#ifndef BUILD_TESTING
    START_JUCE_APPLICATION(CycleV2Application)
    JUCE_MAIN_FUNCTION_DEFINITION
#endif
