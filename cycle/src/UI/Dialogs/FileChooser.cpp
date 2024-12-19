#include "FileChooser.h"

FileChooserDialog::FileChooserDialog(const String& name,
                                     const String& instructions,
                                     FileBrowserComponent& chooserComponent,
                                     const bool warnAboutOverwritingExistingFiles_,
                                     const Colour& backgroundColour,
                                     const DocumentDetails& details)
    : ResizableWindow(name, backgroundColour, true),
      warnAboutOverwritingExistingFiles(warnAboutOverwritingExistingFiles_) {
    content = new ContentComponent(name, instructions, chooserComponent);

    setAlwaysOnTop(true);
    setContentOwned(content, false);
    setResizable(true, true);
    setResizeLimits(300, 300, 700, 700);

    content->okButton.addListener(this);
    content->cancelButton.addListener(this);
    content->newFolderButton.addListener(this);
    content->chooserComponent.addListener(this);
    content->authorBox.setText(details.getAuthor(), dontSendNotification);
    content->packBox.setText(details.getPack(), dontSendNotification);
    content->tagsBox.setText(details.getTags().joinIntoString(","), dontSendNotification);

    FileChooserDialog::selectionChanged();
}

FileChooserDialog::~FileChooserDialog() {
    content->chooserComponent.removeListener(this);
}

bool FileChooserDialog::show(int w, int h) {
    return showAt(-1, -1, w, h);
}

bool FileChooserDialog::showAt(int x, int y, int w, int h) {
    if (w <= 0) {
        Component* const previewComp = content->chooserComponent.getPreviewComponent();

        if (previewComp != nullptr) {
            w = 400 + previewComp->getWidth();
        } else {
            w = 600;
        }
    }

    if (h <= 0) {
        h = 500;
    }

    if (x < 0 || y < 0) {
        centreWithSize(w, h);
    } else {
        setBounds(x, y, w, h);
    }

    const bool ok = (runModalLoop() != 0);
    setVisible(false);
    return ok;
}

void FileChooserDialog::centreWithDefaultSize(Component* componentToCentreAround) {
    Component* const previewComp = content->chooserComponent.getPreviewComponent();

    centreAroundComponent(componentToCentreAround,
                          previewComp != nullptr ? 400 + previewComp->getWidth() : 600,
                          500);
}

void FileChooserDialog::buttonClicked(Button* button) {
    if (button == &(content->okButton)) {
        okButtonPressed();
    } else if (button == &(content->cancelButton)) {
        closeButtonPressed();
    } else if (button == &(content->newFolderButton)) {
        createNewFolder();
    }
}

void FileChooserDialog::closeButtonPressed() {
    setVisible(false);
}

void FileChooserDialog::selectionChanged() {
    content->okButton.setEnabled(content->chooserComponent.currentFileIsValid());

    content->newFolderButton.setVisible(content->chooserComponent.isSaveMode()
                                        && content->chooserComponent.getRoot().isDirectory());
}

void FileChooserDialog::fileClicked(const File&, const MouseEvent&) {
}

void FileChooserDialog::fileDoubleClicked(const File&) {
    selectionChanged();
    content->okButton.triggerClick();
}

void FileChooserDialog::okToOverwriteFileCallback(int result, FileChooserDialog* box) {
    if (result != 0 && box != nullptr) {
        box->exitModalState(1);
    }
}

void FileChooserDialog::okButtonPressed() {
    if (warnAboutOverwritingExistingFiles
        && content->chooserComponent.isSaveMode()
        && content->chooserComponent.getSelectedFile(0).exists()) {
        ModalComponentManager::Callback* callback =
                ModalCallbackFunction::forComponent(okToOverwriteFileCallback, this);

        String message = String("There's already a file called:\n\n")
                         + content->chooserComponent.getSelectedFile(0).getFullPathName()
                         + "\n\n" + "Are you sure you want to overwrite it?";

        AlertWindow::showOkCancelBox(AlertWindow::WarningIcon, "File already exists",
                                     message, "Overwrite", "Cancel", this, callback);
    } else {
        authorBoxContent = content->authorBox.getText();
        tagsBoxContent = content->tagsBox.getText();
        packBoxContent = content->packBox.getText();
        //    	commentBoxContent = content->detailsBox.getText();

        exitModalState(1);
    }
}

void FileChooserDialog::browserRootChanged(const File& newRoot) {
}

void FileChooserDialog::createNewFolderCallback(int result, FileChooserDialog* box,
                                                SafePointer<AlertWindow> alert) {
    if (result != 0 && alert != nullptr && box != nullptr) {
        alert->setVisible(false);
        box->createNewFolderConfirmed(alert->getTextEditorContents("name"));
    }
}

void FileChooserDialog::createNewFolder() {
    File parent(content->chooserComponent.getRoot());

    if (parent.isDirectory()) {
        auto* alertWindow = new AlertWindow("New Folder", "Please enter the name for the folder",
                                                   AlertWindow::NoIcon, this);

        ModalComponentManager::Callback* callback =
                ModalCallbackFunction::forComponent(createNewFolderCallback, this, SafePointer(alertWindow));

        alertWindow->addTextEditor("name", String(), String(), false);

        alertWindow->addButton("Ok", 1, KeyPress(KeyPress::returnKey));
        alertWindow->addButton("Cancel", 1, KeyPress(KeyPress::escapeKey));
        alertWindow->enterModalState(true, callback, true);
    }
}

void FileChooserDialog::createNewFolderConfirmed(const String& nameFromDialog) {
    const String name(File::createLegalFileName(nameFromDialog));

    if (!name.isEmpty()) {
        const File parent(content->chooserComponent.getRoot());

        if (!parent.getChildFile(name).createDirectory()) {
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                             "New Folder", "Couldn't create the folder!");
        }

        content->chooserComponent.refresh();
    }
}
