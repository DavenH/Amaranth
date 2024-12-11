#pragma once

#include "JuceHeader.h"

using namespace juce;

class ContentComponent  : public Component
{
public:
    ContentComponent (const String& name, const String& instructions_, FileBrowserComponent& chooserComponent_);
    void paint (Graphics& g) override;
    void resized() override;

    FileBrowserComponent& chooserComponent;
    TextButton okButton, cancelButton, newFolderButton;
    Label authorLabel, presetDetails, tagsLabel, packLabel, detailsLabel;

    TextEditor tagsBox, authorBox, packBox, detailsBox;

private:
    String instructions;
    TextLayout text;
};
