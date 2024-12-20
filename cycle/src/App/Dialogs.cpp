#include <Definitions.h>
#include <UI/IConsole.h>

#if PLUGIN_MODE && defined(JUCE_WINDOWS)
#include "../Util/WinHeader.h"
#endif

#include <Algo/AutoModeller.h>
#include <Algo/PitchTracker.h>
#include <App/AppConstants.h>
#include <App/EditWatcher.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/AudioHub.h>
#include <UI/Widgets/IconButton.h>
#include <Util/Util.h>
#include <App/Doc/Document.h>
#include <App/Doc/DocumentDetails.h>

#include "CycleTour.h"
#include "Dialogs.h"
#include "Directories.h"
#include "FileManager.h"

#include "../Audio/SampleUtils.h"
#include "../Audio/SynthAudioSource.h"
#include "../UI/Dialogs/FileChooser.h"
#include "../UI/Dialogs/PresetPage.h"
#include "../UI/Dialogs/HelpDialog.h"
#include "../UI/Panels/PlayerComponent.h"
#include "../UI/Effects/IrModellerUI.h"
#include "../UI/Panels/MainPanel.h"
#include "../UI/Panels/ModMatrixPanel.h"
#include "../Util/CycleEnums.h"

Dialogs::Dialogs(SingletonRepo* repo) :
        SingletonAccessor(repo, "Dialogs") {
}

// TODO
#define _productName "Cycle"

class LightPopup : public DocumentWindow {
public:
    LightPopup(const String &title, const Colour &colour, int requiredButtons, bool addToDesktop)
            : DocumentWindow(title, colour, requiredButtons, addToDesktop) {
    }

    void closeButtonPressed() override {
        this->removeFromDesktop();
        delete this;
    }

    static int getDesktopWindowStyleFlags() {
        return ComponentPeer::windowIsTemporary |
               ComponentPeer::windowHasTitleBar |
               ComponentPeer::windowHasCloseButton;
    }
};

void Dialogs::init() {
    watcher = &getObj(EditWatcher);

    int width = -1, height = -1;
    Dialogs::getSizeFromSetting(getSetting(WindowSize), width, height);
}


void Dialogs::showPresetSaveAsDialog() {
    String presetStr("*.");
    String ext = getStrConstant(DocumentExt);

    presetStr += ext;

    DocumentDetails deets = getObj(Document).getDetails();
    String originalAuthor = deets.getAuthor();
    String presetAlias = getObj(Settings).getProperty("AuthorAlias", "Anonymous");

    if (presetAlias != "Daven") {
        if (originalAuthor.isEmpty() || originalAuthor.equalsIgnoreCase("Anonymous"))
            deets.setAuthor(presetAlias);

        else if (!originalAuthor.containsIgnoreCase(presetAlias))
            deets.setAuthor(originalAuthor + ", " + presetAlias);

        if (deets.getPack().equalsIgnoreCase("Factory"))
            deets.setPack("none");
    }

    String presetsStr("Presets");
    WildcardFileFilter filter(presetStr, String(), presetsStr);
    String filename = getObj(Directories).getUserPresetDir() + deets.getName();
    File absfile = File::getCurrentWorkingDirectory().getChildFile(filename);

    FileBrowserComponent browser(FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles |
                                 FileBrowserComponent::warnAboutOverwriting, absfile, &filter, nullptr);

    FileChooserDialog dialogBox("Save Preset", String(), browser,
                                true, Colour::greyLevel(0.08f), deets);

    if (dialogBox.show()) {
        File currentFile = browser.getSelectedFile(0);
        String filename = currentFile.getFullPathName();

        bool existsBefore = currentFile.existsAsFile();

        if (!filename.endsWith(ext))
            filename += String(".") + ext;

        String author = dialogBox.getAuthorBoxContent();

        if (author.isEmpty())
            author = String("Anonymous");

        if (filename.length() > 4) {
            deets.setRevision(0);
            deets.setName(currentFile.getFileNameWithoutExtension());
            deets.setAuthor(author);
            deets.setPack(dialogBox.getPackBoxContent());

            getObj(FileManager).setCurrentPresetName(filename);

            const String &tagsString = dialogBox
                .getTagsBoxContent()
                .removeCharacters("\t. ")
                .toLowerCase();

            StringArray strings;
            strings.addTokens(tagsString, ",", {});
            strings.trim();

            deets.setTags(strings);
            deets.setDateMillis(Time::currentTimeMillis());
            deets.setFilename(filename);

            getObj(Document).getDetails() = deets;
            getObj(Document).save(filename);

            if (!existsBefore) {
                getObj(PresetPage).addItem(deets);
            }

            watcher->reset();
            watcher->update();
        }
    }

    handleNextPendingModalAction();
}

void Dialogs::showDeviceErrorApplicably() {
    String deviceError = getObj(AudioHub).getDeviceErrorAndReset();

    if (deviceError.isNotEmpty()) {
        auto* window = new AlertWindow("Problem Opening Audio Device",
                                              "Another application is probably using the audio driver -- "
                                              "select a different audio driver (Edit > audio settings) "
                                              "or close the other audio application.\n",
                                              AlertWindow::WarningIcon, mainPanel);
        window->setAlwaysOnTop(true);
        window->addButton("Okay", DialogCancel, KeyPress(KeyPress::escapeKey));
        window->enterModalState(true, nullptr, true);
    }
}

void Dialogs::showOpenWaveDialog(PitchedSample* dstWav, const String &subDir,
                                 int actionContext, PostModalAction postAction,
                                 bool forDirectory) {
    WaveOpenData data(this, dstWav, actionContext, postAction);
    String filter("*.wav;*.aiff;*.aif;*.ogg;*.flac;*.mp3");

    String lastWaveDirectory = getObj(Directories).getLastWaveDirectory();

    if (lastWaveDirectory.isEmpty())
        lastWaveDirectory = getObj(PresetPage).getWavDirectory();

    if (getSetting(NativeDialogs) == 1 || forDirectory) {
        nativeFileChooser = std::make_unique<FileChooser>("Open Audio File", lastWaveDirectory, filter, true);

        if (int result = (forDirectory
                              ? nativeFileChooser->browseForDirectory()
                              : nativeFileChooser->browseForFileToOpen())) {
            data.audioFile = nativeFileChooser->getResult();
            openWaveCallback(result, data);
        }
    } else {
        String instructions = actionContext == DialogActions::TrackPitchAction
                                  ? "The best reference sound files are short (1 - 10s), single note, with few applied effects."
                                  : "Load a short cabinet / amplifier impulse response wave file. Reverb impulses are not supported.";

        if (!forDirectory) {
            fileFilter = std::make_unique<WildcardFileFilter>(filter, String(), "Audio files");
        }

        int openFlags = forDirectory
                            ? FileBrowserComponent::useTreeView |
                              FileBrowserComponent::canSelectDirectories
                            : FileBrowserComponent::canSelectFiles;

        fileBrowser = std::make_unique<FileBrowserComponent>(FileBrowserComponent::openMode | openFlags,
                                                             lastWaveDirectory, fileFilter.get(), 0);

        fileLoader = std::make_unique<FileChooserDialogBox>("Open Audio File", instructions, *fileBrowser,
                                                            true, Colour::greyLevel(0.08f));
        fileLoader->setAlwaysOnTop(true);

        if (int result = fileLoader->show()) {
            data.audioFile = fileBrowser->getSelectedFile(0);
            openWaveCallback(result, data);
        }
    }
}

void Dialogs::showOpenPresetDialog() {
    String lastPresetDirectory = getObj(Directories).getLastPresetDir();

    if (lastPresetDirectory.isEmpty()) {
        lastPresetDirectory = getObj(Directories).getPresetDir();
    }

    if (getSetting(NativeDialogs) == 1) {
        nativeFileChooser = std::make_unique<FileChooser>("Open " + getStrConstant(ProductName) + " Preset",
                                            lastPresetDirectory, "*." + getStrConstant(DocumentExt), true);

        if (int result = nativeFileChooser->browseForFileToOpen()) {
            openPresetCallback(result, nativeFileChooser->getResult(), this);
        }
    } else {
        fileFilter = std::make_unique<WildcardFileFilter>("*." + getStrConstant(DocumentExt),
                                            String(), "Preset files");
        fileBrowser = std::make_unique<FileBrowserComponent>(FileBrowserComponent::openMode |
                                               FileBrowserComponent::canSelectFiles,
                                               lastPresetDirectory, fileFilter.get(), 0);

        fileLoader = std::make_unique<FileChooserDialogBox>("Open " + getStrConstant(ProductName) +
                                              " Preset", String(), *fileBrowser,
                                              true, Colour::greyLevel(0.08f));

        fileLoader->setAlwaysOnTop(true);

        if (int result = fileLoader->show()) {
            openPresetCallback(result, fileBrowser->getSelectedFile(0), this);
        }
    }
}

#if !PLUGIN_MODE
void Dialogs::showAudioSettings() {
    AudioDeviceSelectorComponent audioSettingsComp(
            *(getObj(AudioHub).getAudioDeviceManager()), 0, 1, 2, 2, true, false, true, false);

    audioSettingsComp.setSize(600, 500);
    DialogWindow::showModalDialog("Audio Settings", &audioSettingsComp,
                                  mainPanel, Colour::greyLevel(0.08f),
                                  true, true, true);
}
#endif

void Dialogs::showQualityOptions() {
    if (QualityDialog* qd = &getObj(QualityDialog)) {
        qd->updateSelections();
        DialogWindow::showDialog("Quality Options", qd, mainPanel, Colour::greyLevel(0.1f), true);
    }
}

void Dialogs::showModMatrix() {
    auto &matrix = getObj(ModMatrixPanel);
    matrix.selfSize();
    matrix.setPendingFocusGrab(true);

    matrix.addToDesktop(ComponentPeer::windowIsTemporary);
    matrix.setAlwaysOnTop(true);

    Rectangle<int> rect = Util::withSizeKeepingCentre<int>(getObj(MainPanel).getBounds(),
                                                           matrix.getWidth(), matrix.getHeight());

    matrix.setTopLeftPosition(rect.getX(), rect.getY());
    matrix.setVisible(true);
}

void Dialogs::showSamplePlacer() {
//	getObj(SamplePlacer).setSize(600, 800);
//	DialogWindow::showDialog("Sample Placer", &getObj(SamplePlacer), mainPanel, Colour::greyLevel(0.1f), true);
}

void Dialogs::showOutputPopup() {}
void Dialogs::showDebugPopup() {}

void Dialogs::launchHelp() {
    const String &homeUrl = getObj(Directories).getHomeUrl();
    URL url(homeUrl + "/help/");

    url.launchInDefaultBrowser();
}

void Dialogs::showAboutDialog() {
    auto* help = new HelpDialog(repo);

    help->setSize(350, 300);
    help->addToDesktop(ComponentPeer::windowIsTemporary);
    help->setAlwaysOnTop(true);

    Rectangle<int> rect = Util::withSizeKeepingCentre<int>(getObj(MainPanel).getBounds(),
                                                           help->getWidth(), help->getHeight());

    help->setTopLeftPosition(rect.getX(), rect.getY());
    help->setVisible(true);
}

String Dialogs::getPresetSaveAction() {
    return getObj(FileManager).canSaveOverCurrentPreset() ? "Save" : "Save As...";
}

void Dialogs::promptForSaveModally(const std::function<void(bool)>& completionCallback) {
    progressMark

    if (! watcher->getHaveEdited()) {
        completionCallback(false);
        return;
    }

    String saveAction(getPresetSaveAction());


    AlertWindow::showYesNoCancelBox(
        AlertWindow::QuestionIcon,
        "Save Current Preset",
        "Do you want to save your work?",
        saveAction, "Don't Save", "Cancel",
        mainPanel,
        ModalCallbackFunction::create(completionCallback));
}

void Dialogs::promptForSaveApplicably(PostModalAction action) {
    pendingModalActions.push(action);

    if (watcher->getHaveEdited()) {
        auto* window = new AlertWindow("Save Current Preset",
                                       "Do you want to save your work?",
                                       AlertWindow::QuestionIcon, mainPanel);
        window->setAlwaysOnTop(true);

        String saveAction(getPresetSaveAction());

        window->addButton(saveAction, DialogSave);
        window->addButton("Don't Save", DialogDontSave);
        window->addButton("Cancel", DialogCancel, KeyPress(KeyPress::escapeKey));

        window->enterModalState(true, ModalCallbackFunction::create(saveCallback, repo.get()), true);
    } else {
        saveCallback(DialogDontSave, repo);
    }
}

void Dialogs::showPresetBrowserModal() {
    cout << "Showing preset browser\n";
    bool playerView = false;

#if PLUGIN_MODE
    playerView = getSetting(WindowSize) == WindowSizes::PlayerSize;
#endif

    getObj(PresetPage).setSize(800, 600);

    Component* c = mainPanel;
    if (playerView) {
        c = &getObj(PlayerComponent);
    }

    startTimer(30);
    DialogWindow::showDialog("Preset Browser", &getObj(PresetPage), c,
                             Colour::greyLevel(0.08f), true, true, true);

    getObj(MainPanel).resetTabToWaveform();
}

void Dialogs::getSizeFromSetting(int sizeEnum, int &width, int &height) {
    float factor = 1.f;

    switch (sizeEnum) {
        case WindowSizes::SmallSize:
            factor = 0.6f;
            break;
        case WindowSizes::MedSize:
            factor = 0.8f;
            break;
        case WindowSizes::FullSize:
            factor = formatSplit(1.f, 0.95f);
            break;

        case WindowSizes::PlayerSize: {
            width = 540;
            height = 110;

            return;
        }
        default:
            break;
    }

    Rectangle<int> screenArea = Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;

  #if !PLUGIN_MODE
    screenArea.removeFromBottom(25);
  #endif

    float aspect = screenArea.getHeight() / float(screenArea.getWidth());

    width = jlimit(960, 1920, int(factor * screenArea.getWidth()));
    height = jlimit(720, 1200, int(factor * aspect * screenArea.getWidth()));
}

void Dialogs::saveCallback(int returnId, SingletonRepo* repo) {
    bool operationCanceled = false;

    switch (returnId) {
        case DialogCancel:
            operationCanceled = true;
            break;

        case DialogSave:
            if (getObj(FileManager).canSaveOverCurrentPreset())
                getObj(FileManager).saveCurrentPreset();
            else
                getObj(Dialogs).addPendingAction(ShowSavePresetDialog);

            break;

        case DialogDontSave:
        default:
            break;
    }

    if (!operationCanceled) {
        getObj(Dialogs).handleNextPendingModalAction();
    }
}

struct AsyncPresetDialog : public CallbackMessage {
    explicit AsyncPresetDialog(Dialogs* ops) : ops(ops) { post(); }
    void messageCallback() override { ops->showOpenPresetDialog(); }
    Dialogs* ops;
};

struct AsyncSaveasDialog : public CallbackMessage {
    explicit AsyncSaveasDialog(Dialogs* ops) : ops(ops) { post(); }
    void messageCallback() override { ops->showPresetSaveAsDialog(); }
    Dialogs* ops;
};

void Dialogs::handleNextPendingModalAction() {
    if (pendingModalActions.empty())
        return;

    PostModalAction action = pendingModalActions.top();
    pendingModalActions.pop();
    switch (action) {
        case LoadPresetItem:
            getObj(PresetPage).loadPendingItem();
            break;

        case LoadEmptyPreset:
            getObj(FileManager).openFactoryPreset("Empty");
            break;

        case ShowOpenPresetDialog:
            new AsyncPresetDialog(this);
            break;

        case ShowSavePresetDialog:
            new AsyncSaveasDialog(this);
            break;

        case DoNothing:
        default:
            break;
    }
}

void Dialogs::openWaveCallback(int returnId, WaveOpenData data) {
    if (returnId == DialogCancel) {
        return;
    }

    Dialogs* ops = data.ops;
    SingletonRepo* repo = ops->getSingletonRepo();

    File currentFile = data.audioFile;
    File parentFile = currentFile.getParentDirectory();
    String filename = currentFile.getFullPathName();

    if (ops->fileLoader != nullptr) {
        getObj(MainPanel).removeChildComponent(ops->fileLoader.get());
    }

    if (data.actionContext == DialogActions::TrackPitchAction) {
        getObj(PresetPage).setWavDirectory(parentFile.getFullPathName());
        getObj(Directories).setLastWaveDirectory(parentFile.getFullPathName());
    } else if (data.actionContext == DialogActions::LoadMultisample) {
        getObj(Directories).setLastWaveDirectory(currentFile.getFullPathName());
    }

    if (filename.isNotEmpty()) {
        if (data.actionContext == DialogActions::TrackPitchAction ||
            data.actionContext == DialogActions::LoadMultisample) {
            getObj(Directories).setLoadedWave(filename);
            getObj(FileManager).openWave(currentFile, DialogSource);
        } else if (data.actionContext == DialogActions::LoadImpulse) {

            bool succeeded = data.wrapper->load(filename) >= 0;

            if (succeeded) {
                getObj(IrModellerUI).setWaveImpulsePath(filename);

                if (data.postAction == LoadIRWave) {
                    getObj(IrModellerUI).loadWaveFile();
                } else if (data.postAction == ModelIRWave) {
                    getObj(IrModellerUI).modelLoadedWave();
                }
            } else {
                getObj(IConsole).write("Failed to load audio file", IConsole::DefaultPriority);
            }
        }
    }

    ops->fileLoader = nullptr;
    ops->fileFilter = nullptr;
    ops->nativeFileChooser = nullptr;
    ops->fileBrowser = nullptr;
}

void Dialogs::openPresetCallback(int returnId, const File &currentFile, Dialogs* ops) {
    File parentFile = currentFile.getParentDirectory();
    String filename = currentFile.getFullPathName();

    SingletonRepo* repo = ops->getSingletonRepo();
    getObj(Directories).setLastPresetDir(parentFile.getFullPathName());

    if (ops->fileLoader != nullptr) {
        getObj(MainPanel).removeChildComponent(ops->fileLoader.get());
    }

    if (filename.isNotEmpty()) {
        getObj(FileManager).openPreset(currentFile);
    }

    ops->fileLoader = nullptr;
    ops->fileFilter = nullptr;
    ops->nativeFileChooser = nullptr;
    ops->fileBrowser = nullptr;
}

bool Dialogs::handleSaveAction(int menuResult) {
    switch (menuResult) {
        // cancel
        case DialogCancel:
            return false;

        case DialogSave:
            getObj(FileManager).saveCurrentPreset();
            return true;

        case DialogDontSave:    // don't save
            return true;

        default:
            throw std::invalid_argument("Dialogs::promptForSaveModally");
    }
}

void Dialogs::timerCallback() {
    getObj(PresetPage).grabSearchFocus();
    stopTimer();
}
