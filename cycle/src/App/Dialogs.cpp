#include <UI/IConsole.h>

#if PLUGIN_MODE && defined(JUCE_WINDOWS)
#include "../Util/WinHeader.h"
#endif

#include <Algo/Pitch/PitchTracker.h>
#include <App/AppConstants.h>
#include <App/EditWatcher.h>
#include <App/Settings.h>
#include <App/SingletonRepo.h>
#include <Audio/AudioHub.h>
#include <UI/Widgets/IconButton.h>
#include <Util/Util.h>
#include <App/Doc/Document.h>
#include <App/Doc/DocumentDetails.h>

#include "Dialogs.h"
#include "Directories.h"
#include "FileManager.h"

#include "../Audio/SynthAudioSource.h"
#include "../UI/Dialogs/FileChooser.h"
#include "../UI/Dialogs/PresetPage.h"
#include "../UI/Dialogs/HelpDialog.h"
#include "../UI/Panels/Console.h"
#include "../UI/Panels/PlayerComponent.h"
#include "../UI/Effects/IrModellerUI.h"
#include "../UI/Panels/MainPanel.h"
#include "../UI/Panels/ModMatrixPanel.h"
#include "../Util/CycleEnums.h"
#include <Definitions.h>

Dialogs::Dialogs(SingletonRepo* repo) :
        SingletonAccessor(repo, "Dialogs") {
}

class LightPopup : public DocumentWindow {
public:
    LightPopup(const String &title, const Colour &colour, int requiredButtons, bool addToDesktop)
            : DocumentWindow(title, colour, requiredButtons, addToDesktop) {
    }

    void closeButtonPressed() override {
        this->removeFromDesktop();
        delete this;
    }

    [[nodiscard]] int getDesktopWindowStyleFlags() const override {
        return ComponentPeer::windowIsTemporary |
               ComponentPeer::windowHasTitleBar |
               ComponentPeer::windowHasCloseButton;
    }
};

class CycleSettingsComponent :
        public Component
    ,   public Button::Listener
    ,   public ComboBox::Listener
    ,   public SingletonAccessor {
public:
    explicit CycleSettingsComponent(SingletonRepo* repo) :
            SingletonAccessor(repo, "CycleSettingsComponent")
        ,   tabs(TabbedButtonBar::TabsAtTop) {
        setOpaque(true);
        addAndMakeVisible(tabs);

      #if ! PLUGIN_MODE
        auto* audioSettingsComp = new AudioDeviceSelectorComponent(
                *(getObj(AudioHub).getAudioDeviceManager()), 0, 1, 2, 2, true, false, true, false);
        tabs.addTab("Audio", Colour::greyLevel(0.08f), audioSettingsComp, true);
      #endif

        visualPanel = new Component();
        addSectionLabel(visualEditingLabel, "Editing", visualPanel);
        addSettingButton(snapButton, "Snap to grid", getSetting(SnapMode) == 1);
        addSettingButton(collisionButton, "Prevent line collisions", getSetting(CollisionDetection) == 1);
        addSettingButton(realtimeUpdateButton, "Update graphics during drags", getSetting(UpdateGfxRealtime) == 1);
        addSectionLabel(visualInputLabel, "Input and dialogs", visualPanel);
        addSettingButton(selectWithRightButton, "Select with right click", getSetting(SelectWithRight) == 1);
        addSettingButton(nativeDialogsButton, "Use native file dialogs", getSetting(NativeDialogs) == 1);
        visualPanel->setSize(560, 280);
        tabs.addTab("Visual", Colour::greyLevel(0.08f), visualPanel, true);

        pitchPanel = new Component();
        pitchAlgoLabel.setText("Tracking algorithm", dontSendNotification);
        pitchAlgoLabel.setJustificationType(Justification::centredLeft);
        pitchAlgoLabel.setColour(Label::textColourId, labelColour());
        pitchAlgoBox.addItem("Auto", PitchAlgorithmAuto);
        pitchAlgoBox.addItem("YIN", PitchAlgorithmYin);
        pitchAlgoBox.addItem("Swipe", PitchAlgorithmSwipe);
        pitchAlgoBox.setSelectedId(getPitchAlgorithmId(), dontSendNotification);
        pitchAlgoBox.addListener(this);
        pitchTipLabel.setText("Tip: right-click a MIDI key to set the sample's fundamental pitch.",
                              dontSendNotification);
        pitchTipLabel.setJustificationType(Justification::centredLeft);
        pitchTipLabel.setColour(Label::textColourId, labelColour());
        pitchPanel->addAndMakeVisible(pitchAlgoLabel);
        pitchPanel->addAndMakeVisible(pitchAlgoBox);
        pitchPanel->addAndMakeVisible(pitchTipLabel);
        pitchPanel->setSize(560, 280);
        tabs.addTab("Sample", Colour::greyLevel(0.08f), pitchPanel, true);

        getObj(QualityDialog).updateSelections();
        tabs.addTab("Quality", Colour::greyLevel(0.08f), &getObj(QualityDialog), false);

        setSize(620, 500);
    }

    void resized() override {
        tabs.setBounds(getLocalBounds());
        layoutVisualPanel();
        layoutPitchPanel();
    }

    void paint(Graphics& g) override {
        g.fillAll(panelColour());

        for (auto* panel : { visualPanel, pitchPanel }) {
            if (panel != nullptr) {
                Graphics::ScopedSaveState saveState(g);
                g.reduceClipRegion(panel->getBounds());
                g.setOrigin(panel->getPosition());
                g.fillAll(panelColour());
            }
        }
    }

    void buttonClicked(Button* button) override {
        if (button == &snapButton) {
            getSetting(SnapMode) = snapButton.getToggleState();
        } else if (button == &collisionButton) {
            getSetting(CollisionDetection) = collisionButton.getToggleState();
        } else if (button == &realtimeUpdateButton) {
            getSetting(UpdateGfxRealtime) = realtimeUpdateButton.getToggleState();
        } else if (button == &selectWithRightButton) {
            getSetting(SelectWithRight) = selectWithRightButton.getToggleState();
        } else if (button == &nativeDialogsButton) {
            getSetting(NativeDialogs) = nativeDialogsButton.getToggleState();
        }
    }

    void comboBoxChanged(ComboBox* box) override {
        if (box == &pitchAlgoBox) {
            switch (pitchAlgoBox.getSelectedId()) {
                case PitchAlgorithmAuto:   getSetting(PitchAlgo) = PitchAlgos::AlgoAuto;  break;
                case PitchAlgorithmYin:    getSetting(PitchAlgo) = PitchAlgos::AlgoYin;   break;
                case PitchAlgorithmSwipe:  getSetting(PitchAlgo) = PitchAlgos::AlgoSwipe; break;
                default: break;
            }
        }
    }

private:
    enum PitchAlgorithmItem {
        PitchAlgorithmAuto = 1,
        PitchAlgorithmYin,
        PitchAlgorithmSwipe
    };

    static Colour panelColour() { return Colour::greyLevel(0.075f); }
    static Colour labelColour() { return Colour::greyLevel(0.76f); }
    static Colour sectionColour() { return Colour::greyLevel(0.88f); }

    void addSectionLabel(Label& label, const String& text, Component* panel) {
        label.setText(text, dontSendNotification);
        label.setJustificationType(Justification::centredLeft);
        label.setFont(FontOptions(15.0f, Font::bold));
        label.setColour(Label::textColourId, sectionColour());
        panel->addAndMakeVisible(label);
    }

    void addSettingButton(ToggleButton& button, const String& text, bool selected) {
        button.setButtonText(text);
        button.setToggleState(selected, dontSendNotification);
        button.setColour(ToggleButton::textColourId, labelColour());
        button.setColour(ToggleButton::tickColourId, Colours::orange);
        button.setColour(ToggleButton::tickDisabledColourId, Colour::greyLevel(0.58f));
        button.addListener(this);
        visualPanel->addAndMakeVisible(button);
    }

    int getPitchAlgorithmId() {
        switch ((int) getSetting(PitchAlgo)) {
            case PitchAlgos::AlgoYin:   return PitchAlgorithmYin;
            case PitchAlgos::AlgoSwipe: return PitchAlgorithmSwipe;
            case PitchAlgos::AlgoAuto:
            default:                    return PitchAlgorithmAuto;
        }
    }

    void layoutVisualPanel() {
        if (visualPanel == nullptr) {
            return;
        }

        Rectangle<int> r = visualPanel->getLocalBounds().reduced(24, 20);

        visualEditingLabel.setBounds(r.removeFromTop(22));
        r.removeFromTop(8);

        for (auto* button : { &snapButton, &collisionButton, &realtimeUpdateButton }) {
            button->setBounds(r.removeFromTop(32));
            r.removeFromTop(6);
        }

        r.removeFromTop(14);
        visualInputLabel.setBounds(r.removeFromTop(22));
        r.removeFromTop(8);

        for (auto* button : { &selectWithRightButton, &nativeDialogsButton }) {
            button->setBounds(r.removeFromTop(32));
            r.removeFromTop(6);
        }
    }

    void layoutPitchPanel() {
        if (pitchPanel == nullptr) {
            return;
        }

        Rectangle<int> r = pitchPanel->getLocalBounds().reduced(24, 20);
        Rectangle<int> row = r.removeFromTop(32);
        pitchAlgoLabel.setBounds(row.removeFromLeft(170));
        row.removeFromLeft(12);
        pitchAlgoBox.setBounds(row.removeFromLeft(180));

        r.removeFromTop(18);
        pitchTipLabel.setBounds(r.removeFromTop(44));
    }

    TabbedComponent tabs;
    Component* visualPanel = nullptr;
    Component* pitchPanel = nullptr;

    ToggleButton snapButton;
    ToggleButton collisionButton;
    ToggleButton realtimeUpdateButton;
    ToggleButton selectWithRightButton;
    ToggleButton nativeDialogsButton;

    Label visualEditingLabel;
    Label visualInputLabel;

    Label pitchAlgoLabel;
    Label pitchTipLabel;
    ComboBox pitchAlgoBox;
};

void Dialogs::init() {
    watcher = &getObj(EditWatcher);

    int width = -1, height = -1;
    getSizeFromSetting(getSetting(WindowSize), width, height);
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

        if (deets.getPack().equalsIgnoreCase("Factory")) {
            deets.setPack("none");
        }
    }

    String presetsStr("Presets");
    fileFilter = std::make_unique<WildcardFileFilter>(presetStr, String(), presetsStr);
    String filename = getObj(Directories).getUserPresetDir() + deets.getName();
    File absfile = File::getCurrentWorkingDirectory().getChildFile(filename);

    auto browser = std::make_unique<FileBrowserComponent>(
            FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles
                | FileBrowserComponent::warnAboutOverwriting,
            absfile, fileFilter.get(), nullptr);

    auto* dialogBox = new FileChooserDialog("Save Preset", String(), std::move(browser),
                                            true, Colour::greyLevel(0.08f), deets);

    dialogBox->showAsync([this, dialogBox, deets, ext](bool ok) mutable {
        if (!ok) {
            handleNextPendingModalAction();
            return;
        }

        File currentFile = dialogBox->getSelectedFile();
        String filename = currentFile.getFullPathName();

        bool existsBefore = currentFile.existsAsFile();

        if (!filename.endsWith(ext)) {
            filename += String(".") + ext;
        }

        String author = dialogBox->getAuthorBoxContent();

        if (author.isEmpty()) {
            author = String("Anonymous");
        }

        if (filename.length() > 4) {
            deets.setRevision(0);
            deets.setName(currentFile.getFileNameWithoutExtension());
            deets.setAuthor(author);
            deets.setPack(dialogBox->getPackBoxContent());

            getObj(FileManager).setCurrentPresetName(filename);

            const String& tagsString = dialogBox->getTagsBoxContent()
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

        handleNextPendingModalAction();
    });
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

    if (lastWaveDirectory.isEmpty()) {
        lastWaveDirectory = getObj(PresetPage).getWavDirectory();
    }

    const bool useNativeChooser = (getSetting(NativeDialogs) == 1 || forDirectory);
    nativeFileChooser = std::make_unique<FileChooser>("Open Audio File", lastWaveDirectory, filter, useNativeChooser);

    int openFlags = FileBrowserComponent::openMode;
    if (forDirectory) {
        openFlags |= FileBrowserComponent::canSelectDirectories;
    } else {
        openFlags |= FileBrowserComponent::canSelectFiles;
    }

    nativeFileChooser->launchAsync(openFlags, [data](const FileChooser& chooser) mutable {
        data.audioFile = chooser.getResult();

        if (data.audioFile.exists() || data.audioFile.isDirectory()) {
            Dialogs::openWaveCallback(DialogSave, data);
        } else {
            Dialogs::openWaveCallback(DialogCancel, data);
        }
    });
}

void Dialogs::showOpenPresetDialog() {
    String lastPresetDirectory = getObj(Directories).getLastPresetDir();

    if (lastPresetDirectory.isEmpty()) {
        lastPresetDirectory = getObj(Directories).getRepoPresetDir();

        if (lastPresetDirectory.isEmpty()) {
            lastPresetDirectory = getObj(Directories).getPresetDir();
        }
    }

    const bool useNativeChooser = getSetting(NativeDialogs) == 1;
    nativeFileChooser = std::make_unique<FileChooser>("Open " + getStrConstant(ProductName) + " Preset",
                                        lastPresetDirectory, "*." + getStrConstant(DocumentExt), useNativeChooser);

    nativeFileChooser->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                   [this](const FileChooser& chooser) {
                                       const File selected = chooser.getResult();

                                       if (selected.existsAsFile()) {
                                           openPresetCallback(DialogSave, selected, this);
                                       } else {
                                           openPresetCallback(DialogCancel, File(), this);
                                       }
                                   });
}

#if !PLUGIN_MODE
void Dialogs::showAudioSettings() {
    showSettingsDialog();
}
#endif

void Dialogs::showSettingsDialog() {
    auto* settingsComp = new CycleSettingsComponent(repo);
    Component::SafePointer<Component> focusTarget(mainPanel.get());

    DialogWindow::LaunchOptions options;
    options.dialogTitle = "Settings";
    options.content.setOwned(settingsComp);
    options.componentToCentreAround = mainPanel;
    options.dialogBackgroundColour = Colour::greyLevel(0.08f);
    options.escapeKeyTriggersCloseButton = true;
    options.resizable = true;
    options.useBottomRightCornerResizer = true;

    DialogWindow* window = options.create();
    window->enterModalState(true, ModalCallbackFunction::create([focusTarget](int) {
        MessageManager::callAsync([focusTarget] {
            if (focusTarget != nullptr && focusTarget->isShowing()) {
                focusTarget->grabKeyboardFocus();
            }
        });
    }), true);
}

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
        DBG("Dialogs::promptForSaveModally haven't edited");
        completionCallback(true);
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
    getObj(PresetPage).refresh();

    juce::Component* c = mainPanel;
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

    auto* primaryDisplay = Desktop::getInstance().getDisplays().getPrimaryDisplay();

    if (primaryDisplay == nullptr) {
        width = 1280;
        height = 800;
        return;
    }

    Rectangle<int> screenArea = primaryDisplay->userArea;

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
                getObj(Console).write("Failed to load audio file", IConsole::DefaultPriority);
            }
        }
    }

    ops->fileLoader = nullptr;
    ops->fileFilter = nullptr;
    ops->nativeFileChooser = nullptr;
    ops->fileBrowser = nullptr;
}

void Dialogs::openPresetCallback(int returnId, const File &currentFile, Dialogs* ops) {
    if (returnId == DialogCancel) {
        return;
    }

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
        case DialogCancel:
            return false;

        case DialogSave:
            getObj(FileManager).saveCurrentPreset();
            return true;

        case DialogDontSave:
            return true;

        default:
            throw std::invalid_argument("Dialogs::promptForSaveModally");
    }
}

void Dialogs::timerCallback() {
    getObj(PresetPage).grabSearchFocus();
    stopTimer();
}
