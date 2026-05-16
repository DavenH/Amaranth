#pragma once

#include <JuceHeader.h>

#include "Binary/Images.h"
#include "InstallJob.h"
#include "InstallerEula.h"
#include "InstallerManifest.h"
#include "InstallerSettings.h"

using namespace juce;

class InstallerComponent :
        public Component,
        private Button::Listener,
        private TextEditor::Listener {
public:
    explicit InstallerComponent(InstallerManifest manifest);
    ~InstallerComponent() override;

    void resized() override;
    void paint(Graphics& g) override;

private:
    struct DestinationRow {
        String artifactId;
        String destinationId;
        String source;
        Label label;
        ToggleButton enabled;
        TextEditor path;
        ImageButton browse;
    };

    void buildRows();
    void loadSettings();
    void saveSettings();
    void choosePackage();
    void chooseDestination(DestinationRow& row);
    void runInstall();
    void showResult(const Result& result, const StringArray& messages);
    void buttonClicked(Button* button) override;
    void textEditorTextChanged(TextEditor& editor) override;

    String settingKey(const DestinationRow& row, const String& suffix) const;
    Array<InstallSelection> createSelections() const;

    InstallerManifest manifest;
    std::unique_ptr<InstallerSettings> settings;
    OwnedArray<DestinationRow> rows;
    std::unique_ptr<FileChooser> fileChooser;
    Image folderImage;

    Label title;
    Label subtitle;
    Label packageLabel;
    TextEditor packagePath;
    TextButton packageBrowse;
    TextEditor eula;
    TextButton installButton;
    TextButton quitButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstallerComponent)
};
