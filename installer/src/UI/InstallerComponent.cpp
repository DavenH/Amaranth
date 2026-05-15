#include "InstallerComponent.h"

InstallerComponent::InstallerComponent(InstallerManifest manifestIn) :
        manifest(std::move(manifestIn)) {
    settings = std::make_unique<InstallerSettings>(manifest.getProductId());

    addAndMakeVisible(title);
    addAndMakeVisible(subtitle);
    addAndMakeVisible(packageLabel);
    addAndMakeVisible(packagePath);
    addAndMakeVisible(packageBrowse);
    addAndMakeVisible(eula);
    addAndMakeVisible(installButton);
    addAndMakeVisible(quitButton);

    title.setText(manifest.getProductName() + " Installer", dontSendNotification);
    title.setFont(Font(25.0f, Font::bold));
    subtitle.setText(manifest.getCompanyName() + "  " + manifest.getVersion(), dontSendNotification);
    packageLabel.setText("Package", dontSendNotification);

    packagePath.setMultiLine(false);
    packagePath.addListener(this);
    packageBrowse.setButtonText("Browse");
    packageBrowse.addListener(this);

    eula.setReadOnly(true);
    eula.setMultiLine(true);
    eula.setScrollbarsShown(true);
    eula.setText(InstallerEula::text, dontSendNotification);

    installButton.setButtonText("Accept and Install");
    installButton.addListener(this);
    quitButton.setButtonText("Cancel");
    quitButton.addListener(this);

    folderImage = PNGImageFormat::loadFrom(Images::folder_png, Images::folder_pngSize);

    buildRows();
    loadSettings();
    setSize(860, 620);
}

InstallerComponent::~InstallerComponent() {
    saveSettings();
}

void InstallerComponent::resized() {
    auto bounds = getLocalBounds().reduced(24);
    title.setBounds(bounds.removeFromTop(34));
    subtitle.setBounds(bounds.removeFromTop(22));
    bounds.removeFromTop(12);

    auto packageBounds = bounds.removeFromTop(28);
    packageLabel.setBounds(packageBounds.removeFromLeft(92));
    packageBrowse.setBounds(packageBounds.removeFromRight(96));
    packageBounds.removeFromRight(8);
    packagePath.setBounds(packageBounds);
    bounds.removeFromTop(16);

    for (auto* row : rows) {
        auto rowBounds = bounds.removeFromTop(24);
        row->enabled.setBounds(rowBounds.removeFromLeft(28));
        row->label.setBounds(rowBounds.removeFromLeft(320));
        row->browse.setBounds(rowBounds.removeFromRight(24));
        rowBounds.removeFromRight(8);
        row->path.setBounds(rowBounds);
        bounds.removeFromTop(10);
    }

    bounds.removeFromTop(8);
    auto buttonBounds = bounds.removeFromBottom(36);
    quitButton.setBounds(buttonBounds.removeFromRight(112));
    buttonBounds.removeFromRight(8);
    installButton.setBounds(buttonBounds.removeFromRight(170));
    bounds.removeFromBottom(14);
    eula.setBounds(bounds);
}

void InstallerComponent::paint(Graphics& g) {
    g.fillAll(Colour(0xff101113));
}

void InstallerComponent::buildRows() {
    for (const auto& artifact : manifest.getArtifacts()) {
        for (const auto& destination : artifact.destinations) {
            auto* row = rows.add(new DestinationRow());
            row->artifactId = artifact.id;
            row->destinationId = destination.id;
            row->source = artifact.source;
            row->label.setText(artifact.name + " - " + destination.name, dontSendNotification);
            row->enabled.setToggleState(destination.enabled, dontSendNotification);
            row->path.setText(manifest.resolvePath(destination.path), dontSendNotification);
            row->path.setMultiLine(false);
            row->browse.setImages(false, true, true,
                                  folderImage, 0.72f, Colours::transparentBlack,
                                  folderImage, 0.95f, Colours::transparentBlack,
                                  folderImage, 1.0f, Colours::transparentBlack);

            addAndMakeVisible(row->label);
            addAndMakeVisible(row->enabled);
            addAndMakeVisible(row->path);
            addAndMakeVisible(row->browse);

            row->path.addListener(this);
            row->browse.addListener(this);
        }
    }
}

void InstallerComponent::loadSettings() {
    const File defaultZip = manifest.getManifestFile().getSiblingFile(manifest.getZipName());
    packagePath.setText(settings->getString("packagePath", defaultZip.getFullPathName()), dontSendNotification);

    for (auto* row : rows) {
        row->path.setText(settings->getString(settingKey(*row, "path"), row->path.getText()), dontSendNotification);
        row->enabled.setToggleState(settings->getBool(settingKey(*row, "enabled"), row->enabled.getToggleState()), dontSendNotification);
    }
}

void InstallerComponent::saveSettings() {
    if (settings == nullptr) {
        return;
    }

    settings->setString("packagePath", packagePath.getText());
    for (auto* row : rows) {
        settings->setString(settingKey(*row, "path"), row->path.getText());
        settings->setBool(settingKey(*row, "enabled"), row->enabled.getToggleState());
    }
    settings->save();
}

void InstallerComponent::choosePackage() {
    fileChooser = std::make_unique<FileChooser>("Select " + manifest.getProductName() + " package",
                                                File(packagePath.getText()),
                                                "*.zip");
    fileChooser->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                             [this](const FileChooser& chooser) {
                                 const File result = chooser.getResult();
                                 if (result.existsAsFile()) {
                                     packagePath.setText(result.getFullPathName(), dontSendNotification);
                                     saveSettings();
                                 }
                             });
}

void InstallerComponent::chooseDestination(DestinationRow& row) {
    fileChooser = std::make_unique<FileChooser>("Select install location", File(row.path.getText()));
    fileChooser->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                             [&row, this](const FileChooser& chooser) {
                                 const File result = chooser.getResult();
                                 if (result.exists()) {
                                     row.path.setText(result.getFullPathName(), dontSendNotification);
                                     saveSettings();
                                 }
                             });
}

void InstallerComponent::runInstall() {
    saveSettings();

    InstallJob job;
    const Result result = job.install(File(packagePath.getText()), createSelections());
    showResult(result, job.getMessages());
}

void InstallerComponent::showResult(const Result& result, const StringArray& messages) {
    if (result.failed()) {
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon, "Install failed", result.getErrorMessage(), "OK", this);
        return;
    }

    String message = "Installation completed.";
    if (! messages.isEmpty()) {
        message << newLine << newLine << messages.joinIntoString(newLine);
    }

    AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon, "Installed", message, "OK", this);
}

String InstallerComponent::settingKey(const DestinationRow& row, const String& suffix) const {
    return row.artifactId + "." + row.destinationId + "." + suffix;
}

Array<InstallSelection> InstallerComponent::createSelections() const {
    Array<InstallSelection> selections;
    for (auto* row : rows) {
        InstallSelection selection;
        selection.artifactId = row->artifactId;
        selection.destinationId = row->destinationId;
        selection.source = row->source;
        selection.destinationDirectory = row->path.getText();
        selection.enabled = row->enabled.getToggleState();
        if (selection.enabled) {
            selections.add(selection);
        }
    }
    return selections;
}

void InstallerComponent::buttonClicked(Button* button) {
    if (button == &packageBrowse) {
        choosePackage();
        return;
    }

    if (button == &installButton) {
        runInstall();
        return;
    }

    if (button == &quitButton) {
        JUCEApplication::getInstance()->systemRequestedQuit();
        return;
    }

    for (auto* row : rows) {
        if (button == &row->browse) {
            chooseDestination(*row);
            return;
        }
    }
}

void InstallerComponent::textEditorTextChanged(TextEditor&) {
    saveSettings();
}
