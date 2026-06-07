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

    class MainWindow :
            public DocumentWindow
        ,   public MenuBarModel
        ,   public ApplicationCommandTarget {
    public:
        enum Command : CommandID {
            CommandOpenGraph = 0x3000,
            CommandSaveGraph,
            CommandSaveGraphAs
        };

        explicit MainWindow(const String& name) :
                DocumentWindow(name, Colour(0xff101318), allButtons) {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            workspace = new CycleV2::NodeWorkspace();
            setContentOwned(workspace, true);

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
        }

        ~MainWindow() override {
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

        StringArray getMenuBarNames() override {
            return { "File" };
        }

        PopupMenu getMenuForIndex(int menuIndex, const String&) override {
            PopupMenu menu;

            if (menuIndex == 0) {
                menu.addCommandItem(&commandManager, CommandOpenGraph);
                menu.addSeparator();
                menu.addCommandItem(&commandManager, CommandSaveGraph);
                menu.addCommandItem(&commandManager, CommandSaveGraphAs);
            }

            return menu;
        }

        void menuItemSelected(int, int) override {}

        ApplicationCommandTarget* getNextCommandTarget() override {
            return nullptr;
        }

        void getAllCommands(Array<CommandID>& commands) override {
            commands.addArray({ CommandOpenGraph, CommandSaveGraph, CommandSaveGraphAs });
        }

        void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) override {
            switch (commandID) {
                case CommandOpenGraph:
                    result.setInfo("Open Graph...", "Open a Cycle V2 graph file", "File", 0);
                    result.addDefaultKeypress('o', ModifierKeys::commandModifier);
                    break;

                case CommandSaveGraph:
                    result.setInfo("Save Graph", "Save the current Cycle V2 graph", "File", 0);
                    result.addDefaultKeypress('s', ModifierKeys::commandModifier);
                    break;

                case CommandSaveGraphAs:
                    result.setInfo("Save Graph As...", "Save the current Cycle V2 graph to a new file", "File", 0);
                    result.addDefaultKeypress('s', ModifierKeys::commandModifier | ModifierKeys::shiftModifier);
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

                default:
                    return false;
            }
        }

    private:
        File defaultGraphDirectory() const {
            return currentGraphFile == File()
                    ? File::getSpecialLocation(File::userDocumentsDirectory)
                    : currentGraphFile.getParentDirectory();
        }

        void chooseOpenGraph() {
            fileChooser = std::make_unique<FileChooser>(
                    "Open Cycle V2 graph",
                    defaultGraphDirectory(),
                    "*.cyclegraph;*.xml");

            fileChooser->launchAsync(
                    FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                    [safeThis = SafePointer<MainWindow>(this)](const FileChooser& chooser) {
                        if (safeThis == nullptr) {
                            return;
                        }

                        const File file = chooser.getResult();
                        if (file != File() && safeThis->workspace != nullptr
                                && safeThis->workspace->loadGraphFromFile(file)) {
                            safeThis->currentGraphFile = file;
                        }

                        safeThis->fileChooser = nullptr;
                    });
        }

        void saveGraph() {
            if (currentGraphFile == File()) {
                chooseSaveGraphAs();
                return;
            }

            if (workspace != nullptr) {
                workspace->saveGraphToFile(currentGraphFile);
            }
        }

        void chooseSaveGraphAs() {
            const File initialFile = currentGraphFile == File()
                    ? defaultGraphDirectory().getChildFile("Untitled.cyclegraph")
                    : currentGraphFile;
            fileChooser = std::make_unique<FileChooser>(
                    "Save Cycle V2 graph",
                    initialFile,
                    "*.cyclegraph;*.xml");

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

                            if (safeThis->workspace != nullptr
                                    && safeThis->workspace->saveGraphToFile(file)) {
                                safeThis->currentGraphFile = file;
                            }
                        }

                        safeThis->fileChooser = nullptr;
                    });
        }

        ApplicationCommandManager commandManager;
        CycleV2::NodeWorkspace* workspace {};
        std::unique_ptr<FileChooser> fileChooser;
        File currentGraphFile;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

#ifndef BUILD_TESTING
    START_JUCE_APPLICATION(CycleV2Application)
    JUCE_MAIN_FUNCTION_DEFINITION
#endif
