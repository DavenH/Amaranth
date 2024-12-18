#include "FileChooserContent.h"
#include "FileChooser.h"

#include <Definitions.h>

ContentComponent::ContentComponent (const String& name,
                                    const String& instructions_,
                                    FileBrowserComponent& chooserComponent_)
    : 	Component (name)
    ,	chooserComponent(chooserComponent_)
    ,	okButton 		(chooserComponent_.getActionVerb())
    ,	cancelButton 	("Cancel")
    ,	newFolderButton ("New Folder")
    ,	presetDetails	({}, "Preset Details")
    ,	authorLabel		({}, "Author")
    ,	tagsLabel		({}, "Tags")
    , 	packLabel		({}, "Pack")
    , 	detailsLabel	({}, "Comments")
    , 	instructions 	(instructions_) {
    addAndMakeVisible(&chooserComponent);
    addAndMakeVisible(&okButton);
    addAndMakeVisible(&cancelButton);
    cancelButton.addShortcut (KeyPress(KeyPress::escapeKey));

    tagsBox.setTextToShowWhenEmpty("(comma separated)", Colour::greyLevel(0.4f));

    TextEditor* editors[] = { &tagsBox, &authorBox, &packBox };
    Label* labels[] = { &tagsLabel, &authorLabel, &packLabel };

    for(int i = 0; i < numElementsInArray(editors); ++i) {
        addAndMakeVisible(editors[i]);
        editors[i]->setMultiLine(false);
        editors[i]->setSelectAllWhenFocused(true);
        editors[i]->setReadOnly(false);

        labels[i]->attachToComponent(editors[i], true);
    }

    presetDetails.setJustificationType(Justification::centred);
    presetDetails.setFont(FontOptions(15, Font::bold));
    addAndMakeVisible(&presetDetails);

    addChildComponent(&newFolderButton);
    setInterceptsMouseClicks (false, true);
}

void ContentComponent::paint (Graphics& g) {
    g.setColour (getLookAndFeel().findColour (FileChooserDialog::titleTextColourId));

    text.draw (g, getLocalBounds().reduced (6).removeFromTop ((int) text.getHeight()).toFloat());

    g.setColour(Colours::black);
    g.fillRect(chooserComponent.getBounds().reduced(8, 30));
}

void ContentComponent::resized() {
    const int buttonHeight 	= 26;
    const int buttonWidth 	= 110;
    const int detailsHeight = 200;
    const int boxWidth 		= jmin(getWidth() * 0.75f, 220.f);

    Rectangle area (getLocalBounds());

    text.createLayout (getLookAndFeel().createFileChooserHeaderText (getName(), instructions), getWidth() - 12.0f);

    area.removeFromTop (roundToInt (text.getHeight()) + 10);

    chooserComponent.setBounds (area.removeFromTop (area.getHeight() - buttonHeight - detailsHeight - 20));
    Rectangle detailsArea (area.removeFromTop(detailsHeight));

    detailsArea.removeFromTop(20);
    presetDetails.setBounds(detailsArea.removeFromTop(30));

    TextEditor* editors[] = { &tagsBox, &authorBox, &packBox, &detailsBox };

    for(auto& editor : editors) {
        editor->setBounds((getWidth() - boxWidth) / 2, detailsArea.getY(), boxWidth, buttonHeight);
        detailsArea.removeFromTop(30);
    }

    Rectangle buttonArea (area.reduced (16, 10));

    okButton.setSize(buttonWidth, buttonHeight);
    cancelButton.setSize(buttonWidth, buttonHeight);
    newFolderButton.setSize(buttonWidth, buttonHeight);

    cancelButton.setBounds (buttonArea.removeFromRight (cancelButton.getWidth()));
    buttonArea.removeFromRight (16);

    okButton.setBounds (buttonArea.removeFromRight (okButton.getWidth()));
    newFolderButton.setBounds (buttonArea.removeFromLeft (newFolderButton.getWidth()));
}
